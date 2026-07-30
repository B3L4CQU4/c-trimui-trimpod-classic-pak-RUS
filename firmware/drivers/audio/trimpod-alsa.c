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

/* Trimpod: audiohw for the SDL PCM output.  Volume is split -- the coarse
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
#include <alsa/asoundlib.h>
#include "config.h"
#include "sound.h"
#include "fixedpoint.h"
#include "pcm_sw_volume.h"

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
#define SINK_SPEAKER      0

/* stdio, not the Rockbox file API -- that one is macro-redirected and would
 * mangle a host path (firmware/target/hosted/sysfs.c does the same). */
static int read_audiosink(void)
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

/* ---- Bluetooth: BlueALSA's A2DP volume ----------------------------------- */

static snd_ctl_t *bt_ctl = NULL;
static snd_ctl_elem_value_t *bt_val = NULL;
static long bt_min, bt_max;
static bool bt_open_tried = false;

static void bt_close(void)
{
    if (bt_val)
        snd_ctl_elem_value_free(bt_val);
    if (bt_ctl)
        snd_ctl_close(bt_ctl);
    bt_val = NULL;
    bt_ctl = NULL;
    bt_open_tried = false;
}

/* One attempt per sink change -- retrying a failure would reconnect to
 * BlueALSA's D-Bus service on every volume change.
 *
 * The type check must be inside the loop: BlueALSA exposes a boolean
 * "<device> A2DP Playback Switch" whose name matches before the integer
 * "<device> A2DP Playback Volume" does. */
static void bt_open(void)
{
    snd_ctl_elem_list_t *list = NULL;
    snd_ctl_elem_info_t *info = NULL;
    snd_ctl_elem_id_t *id = NULL;
    unsigned int used, i;
    bool found = false;

    if (bt_open_tried)
        return;
    bt_open_tried = true;

    if (snd_ctl_open(&bt_ctl, "bluealsa", 0) < 0)
    {
        bt_ctl = NULL;
        return;
    }
    if (snd_ctl_elem_list_malloc(&list) < 0 ||
        snd_ctl_elem_info_malloc(&info) < 0 ||
        snd_ctl_elem_id_malloc(&id) < 0)
        goto out;

    if (snd_ctl_elem_list(bt_ctl, list) < 0 ||
        snd_ctl_elem_list_alloc_space(list,
                                      snd_ctl_elem_list_get_count(list)) < 0 ||
        snd_ctl_elem_list(bt_ctl, list) < 0)
        goto out;

    /* iterate `used`, not `count`: entries past it are uninitialised */
    used = snd_ctl_elem_list_get_used(list);
    for (i = 0; i < used && !found; i++)
    {
        const char *name = snd_ctl_elem_list_get_name(list, i);

        if (!name || !strstr(name, "A2DP"))
            continue;
        snd_ctl_elem_list_get_id(list, i, id);
        snd_ctl_elem_info_set_id(info, id);
        if (snd_ctl_elem_info(bt_ctl, info) < 0 ||
            snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_INTEGER)
            continue;                   /* the boolean switch */
        bt_min = snd_ctl_elem_info_get_min(info);
        bt_max = snd_ctl_elem_info_get_max(info);
        if (bt_max <= bt_min)
            continue;
        if (snd_ctl_elem_value_malloc(&bt_val) == 0)
        {
            snd_ctl_elem_value_set_id(bt_val, id);
            found = true;
        }
    }

out:
    if (list)
    {
        snd_ctl_elem_list_free_space(list);
        snd_ctl_elem_list_free(list);
    }
    if (info)
        snd_ctl_elem_info_free(info);
    if (id)
        snd_ctl_elem_id_free(id);
    if (!found && bt_ctl)
    {
        snd_ctl_close(bt_ctl);          /* keep bt_open_tried set */
        bt_ctl = NULL;
    }
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
    int sink = read_audiosink();

    if (sink != last_sink)
    {
        last_sink = sink;
        bt_close();                     /* re-probe once for the new route */
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
    else
    {
        bt_open();
        /* Mute stays a software zero -- instant, and it leaves the device's
         * own level where the user set it. */
        if (bt_ctl && common != INT_MAX)
        {
            /* A2DP volume is defined as linear in LOUDNESS, so step it ~2x per
             * 10 dB; an amplitude map would collapse the dial.  The control
             * then carries the whole attenuation and software stays at unity. */
            long v = bt_min + (((bt_max - bt_min) *
                                mdb_to_factor(common * 602 / 1000)) >> 16);
            if (v < bt_min)
                v = bt_min;
            if (v > bt_max)
                v = bt_max;

            snd_ctl_elem_value_set_integer(bt_val, 0, v);
            snd_ctl_elem_value_set_integer(bt_val, 1, v);
            if (snd_ctl_elem_write(bt_ctl, bt_val) >= 0)
                hw_mdb = common;
        }
    }

    /* software covers what the control did not, rounded to the setting's
     * tenth-dB unit (<=0.05 dB of rounding error) */
    int rem_l = att_l == INT_MAX ? PCM_MUTE_LEVEL : -((att_l - hw_mdb + 50) / 100);
    int rem_r = att_r == INT_MAX ? PCM_MUTE_LEVEL : -((att_r - hw_mdb + 50) / 100);

    pcm_set_master_volume(rem_l, rem_r);
}

void audiohw_close(void) {}
