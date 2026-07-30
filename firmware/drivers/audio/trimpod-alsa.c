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

/* Trimpod: audiohw for the SDL PCM output.  Volume is split -- coarse
 * attenuation goes to the codec's 'digital volume' mixer control, software
 * carries only the sub-step remainder, channel balance (the control is mono)
 * and any tail past its floor.
 *
 * Keep the software trim small: deep attenuation in a 16-bit multiply
 * quantizes quiet passages to silence (at -40 dB everything under -56 dBFS
 * rounds to zero).  At <=1.16 dB the truncation floor stays ~95 dB down. */

#include <limits.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>
#include "config.h"
#include "sound.h"
#include "pcm_sw_volume.h"

/* Codec 'digital volume': 0..63, 1.16 dB/step, 0 = loudest.  The driver's TLV
 * dB info has the direction backwards -- do not "correct" this from it. */
#define DV_CTL_CARD   "hw:audiocodec"
#define DV_CTL_NAME   "digital volume"
#define DV_STEP_MDB   1160  /* milli-dB per hardware step */
#define DV_MAX_STEPS  63

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

/* Volume arrives in tenth-dB (trimpod_codec.h numdecimals=1) and is <= 0; the
 * bottom of the range is a hard mute. */
void audiohw_set_volume(int vol_l, int vol_r)
{
    hw_dv_open();

    /* attenuations in milli-dB, INT_MAX = mute */
    int att_l = vol_l <= sound_min(SOUND_VOLUME) ? INT_MAX : -vol_l * 100;
    int att_r = vol_r <= sound_min(SOUND_VOLUME) ? INT_MAX : -vol_r * 100;

    /* the shared part goes to the codec, in whole hardware steps */
    int common = att_l < att_r ? att_l : att_r;
    int steps = common == INT_MAX ? DV_MAX_STEPS : common / DV_STEP_MDB;
    if (steps > DV_MAX_STEPS)
        steps = DV_MAX_STEPS;
    if (!dv_ctl)
        steps = 0;

    if (dv_ctl)
    {
        snd_ctl_elem_value_set_integer(dv_val, 0, steps);
        snd_ctl_elem_write(dv_ctl, dv_val);
    }

    /* software covers what the hardware steps did not, rounded to the setting's
     * tenth-dB unit (<=0.05 dB of rounding error) */
    int hw_mdb = steps * DV_STEP_MDB;
    int rem_l = att_l == INT_MAX ? PCM_MUTE_LEVEL : -((att_l - hw_mdb + 50) / 100);
    int rem_r = att_r == INT_MAX ? PCM_MUTE_LEVEL : -((att_r - hw_mdb + 50) / 100);

    pcm_set_master_volume(rem_l, rem_r);
}

void audiohw_close(void) {}
