/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/* Trimpod: audiohw for the ALSA PCM output.  Volume is split -- the coarse
 * attenuation goes to a mixer control, software carries only the remainder.
 *
 * A mixer only attenuates what passes through it, so WHICH control depends on
 * where the OS is sending audio: the codec's 'digital volume' for the internal
 * speaker, BlueALSA's A2DP volume for a Bluetooth device.  The active sink
 * comes from the shared settings block NextUI's audiomon maintains.
 *
 * Driving the sink's own control also means the level survives exit, so the
 * volume the user leaves is the volume the system keeps -- nothing to pin and
 * restore, and nothing left wrong if the app is killed.
 *
 * Keep the software remainder small: deep attenuation in a 16-bit multiply
 * quantizes quiet passages to silence (at -40 dB everything under -56 dBFS
 * rounds to zero).  If no control can be driven the code falls back to
 * software-only, which still works, just with that limitation. */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include "config.h"
#include "sound.h"
#include "fixedpoint.h"
#include "pcm_sw_volume.h"
#include "trimpod_alsa.h"

/* Codec 'digital volume': 0..63, 1.16 dB/step, 0 = loudest.  The driver's TLV
 * dB info has the direction backwards -- do not "correct" this from it. */
#define DV_CTL_CARD   "hw:audiocodec"
#define DV_CTL_NAME   "digital volume"
#define DV_STEP_MDB   1160  /* milli-dB per hardware step */
#define DV_MAX_STEPS  63

/* NextUI publishes the active audio sink in its shared settings block, updated
 * by its audiomon daemon on connect/disconnect.  Byte 112 is SettingsV11 field
 * 29; the same arithmetic puts toggled_volume at byte 56, which is where
 * launch.sh pokes MutedVolume, and the file is 132 bytes = that struct's
 * 33 ints. */
#define SHM_SETTINGS      "/dev/shm/SharedSettings"
#define SHM_AUDIOSINK_OFF 112
#define SINK_SPEAKER      TRIMPOD_SINK_SPEAKER

/* stdio, not the Rockbox file API -- that one is macro-redirected and would
 * mangle a host path (firmware/target/hosted/sysfs.c does the same). */
int trimpod_audio_sink(void)
{
    FILE *f = fopen(SHM_SETTINGS, "rb");
    int32_t sink = SINK_SPEAKER;        /* NextUI absent -> speaker */

    if (f)
    {
        if (fseek(f, SHM_AUDIOSINK_OFF, SEEK_SET) != 0 ||
            fread(&sink, sizeof sink, 1, f) != 1)
            sink = SINK_SPEAKER;
        fclose(f);
    }
    return sink;
}

/* snd_config_update_free_global() is not thread-safe, and the PCM writer thread
 * calls it on every (re)open while this file's mixer handles are live on the
 * Rockbox side.  One lock covers both.  Static init because the first volume
 * change can land before any explicit setup. */
static pthread_mutex_t alsa_cfg_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- internal speaker: the codec's own control --------------------------- */

static snd_ctl_t *dv_ctl = NULL;
static snd_ctl_elem_value_t *dv_val = NULL;
static bool dv_open_tried = false;

/* One open attempt; on failure software carries the whole volume range. */
static void hw_dv_open(void)
{
    if (dv_open_tried)
        return;
    dv_open_tried = true;

    if (snd_ctl_open(&dv_ctl, DV_CTL_CARD, 0) < 0)
    {
        dv_ctl = NULL;
        return;
    }

    if (snd_ctl_elem_value_malloc(&dv_val) < 0)
    {
        snd_ctl_close(dv_ctl);
        dv_ctl = NULL;
        return;
    }

    snd_ctl_elem_value_set_interface(dv_val, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(dv_val, DV_CTL_NAME);
}

/* ---- Bluetooth: BlueALSA's A2DP volume, out of process ------------------
 *
 * Deliberately NOT snd_ctl_open("bluealsa").  Doing that loads
 * libasound_module_ctl_bluealsa.so -- with its own D-Bus connection and thread
 * -- into this address space, and when the speaker is switched off that plugin
 * faults and takes the app with it.  Every crash log named that library, a
 * PCM-only probe survived the same disconnect repeatedly, and NextUI (which
 * never calls snd_ctl_open anywhere) does not crash.
 *
 * So the mixer runs as a child process, exactly as NextUI's SetRawVolume does:
 * "bluealsa is a mixer plugin, not exposed as a separate card".  A dying plugin
 * can then only kill the child.  amixer inherits our HOME, so ctl.!default
 * resolves to the same BlueALSA control audiomon configured.
 *
 * Volume changes are user keypresses, so the fork/exec cost is irrelevant. */

static char bt_ctl_name[128];
static bool bt_ctl_looked_up;

/* First A2DP simple control, as listed by amixer.  Same scan NextUI does. */
static void bt_find_control(void)
{
    FILE *fp;
    char line[256];

    if (bt_ctl_looked_up)
        return;
    bt_ctl_looked_up = true;
    bt_ctl_name[0] = '\0';

    fp = popen("amixer scontrols 2>/dev/null", "r");
    if (!fp)
        return;

    while (fgets(line, sizeof line, fp))
    {
        char *start = strchr(line, '\'');
        char *end = strrchr(line, '\'');

        if (!start || !end || end <= start)
            continue;
        *end = '\0';
        if (!strstr(start + 1, "A2DP") || strchr(start + 1, '"'))
            continue;               /* skip anything we would have to escape */
        snprintf(bt_ctl_name, sizeof bt_ctl_name, "%s", start + 1);
        break;
    }

    pclose(fp);
}

static void bt_forget(void)
{
    bt_ctl_looked_up = false;
    bt_ctl_name[0] = '\0';
}

/* Returns true if the control took the level. */
static bool bt_set_percent(int percent)
{
    char cmd[256];

    bt_find_control();
    if (!bt_ctl_name[0])
        return false;

    snprintf(cmd, sizeof cmd, "amixer -M sset \"%s\" %d%% >/dev/null 2>&1",
             bt_ctl_name, percent);
    return system(cmd) == 0;
}

void trimpod_alsa_lock(void)   { pthread_mutex_lock(&alsa_cfg_lock); }
void trimpod_alsa_unlock(void) { pthread_mutex_unlock(&alsa_cfg_lock); }

void trimpod_alsa_forget_bt(void)
{
    pthread_mutex_lock(&alsa_cfg_lock);
    bt_forget();
    pthread_mutex_unlock(&alsa_cfg_lock);
}

/* Q16 amplitude factor for an attenuation in milli-dB (unity = 1<<16). */
static int32_t mdb_to_factor(int att_mdb)
{
    if (att_mdb <= 0)
        return 1 << 16;
    long f = fp_factor(fp_div(-att_mdb, 1000, 16), 16);
    return f >= (1 << 16) ? (1 << 16) : (int32_t)f;
}

/* Volume arrives in tenth-dB (trimpod_codec.h numdecimals=1) and is <= 0; the
 * bottom of the range is a hard mute. */
void audiohw_set_volume(int vol_l, int vol_r)
{
    static int last_sink = -1;
    int sink = trimpod_audio_sink();

    /* Held across the whole body: the PCM writer thread can free the config
     * tree these handles came from at any moment. */
    pthread_mutex_lock(&alsa_cfg_lock);

    if (sink != last_sink)
    {
        last_sink = sink;
        bt_forget();                    /* re-probe once for the new route */
    }

    /* attenuations in milli-dB, INT_MAX = mute */
    int att_l = vol_l <= sound_min(SOUND_VOLUME) ? INT_MAX : -vol_l * 100;
    int att_r = vol_r <= sound_min(SOUND_VOLUME) ? INT_MAX : -vol_r * 100;
    int common = att_l < att_r ? att_l : att_r;
    int hw_mdb = 0;

    if (sink == SINK_SPEAKER)
    {
        hw_dv_open();
        if (dv_ctl)
        {
            int steps = common == INT_MAX ? DV_MAX_STEPS
                                          : common / DV_STEP_MDB;
            if (steps > DV_MAX_STEPS)
                steps = DV_MAX_STEPS;

            snd_ctl_elem_value_set_integer(dv_val, 0, steps);
            snd_ctl_elem_write(dv_ctl, dv_val);
            hw_mdb = steps * DV_STEP_MDB;
        }
    }
    else if (common != INT_MAX)
    {
        /* Mute stays a software zero -- instant, and it leaves the device's own
         * level where the user set it.
         *
         * A2DP volume is defined as linear in LOUDNESS, so step it ~2x per
         * 10 dB; an amplitude map would collapse the dial.  When the control
         * takes it, it carries the whole attenuation and software stays at
         * unity, which keeps the 16-bit truncation floor out of the way. */
        int percent = (int)((mdb_to_factor(common * 602 / 1000) * 100) >> 16);

        if (percent < 0)
            percent = 0;
        if (percent > 100)
            percent = 100;

        if (bt_set_percent(percent))
            hw_mdb = common;
    }

    pthread_mutex_unlock(&alsa_cfg_lock);

    /* software covers what the control did not, rounded to the setting's
     * tenth-dB unit (<=0.05 dB of rounding error) */
    int rem_l = att_l == INT_MAX ? PCM_MUTE_LEVEL : -((att_l - hw_mdb + 50) / 100);
    int rem_r = att_r == INT_MAX ? PCM_MUTE_LEVEL : -((att_r - hw_mdb + 50) / 100);

    pcm_set_master_volume(rem_l, rem_r);
}

/* shutdown_hw() calls this after audio_stop(); stopping the writer here is what
 * lets the process actually exit instead of hanging on "Shutting Down". */
void audiohw_close(void)
{
    pcm_alsa_shutdown();
}
