/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright © 2010 Thomas Martitz
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

#include <limits.h>
#include "config.h"
#include "sound.h"

/* Audio "hardware" for the direct-ALSA output: the only real control is
 * volume, split by pcm-alsa.c into codec 'digital volume' steps plus a small
 * software trim.  Tone/EQ live in the DSP chain. */

extern void pcm_set_mixer_volume(int vol_l, int vol_r);

/* Volume arrives in tenth-dB (trimpod_codec.h numdecimals=1); the bottom of the
 * range is a hard mute. */
static int alsa_volume_level(int volume)
{
    return volume <= sound_min(SOUND_VOLUME) ? INT_MIN : volume;
}

void audiohw_set_volume(int vol_l, int vol_r)
{
    pcm_set_mixer_volume(alsa_volume_level(vol_l), alsa_volume_level(vol_r));
}

void audiohw_close(void) {}
