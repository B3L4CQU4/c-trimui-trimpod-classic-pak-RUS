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
#ifndef _TRIMPOD_CODEC_H
#define _TRIMPOD_CODEC_H

/* Trimpod: 0.1 dB resolution (numdecimals=1, step=1) so the perceptual notch
 * table lands on evenly spaced loudness steps instead of snapping to whole dB.
 * Limits below are in 0.1 dB units; 0 = unity into the hardware ceiling
 * launch.sh pins (DAC volume 160, NextUI's shipped max). */
AUDIOHW_SETTING(VOLUME,      "dB",   1,  1, -800,   0, -220)

#endif /* _TRIMPOD_CODEC_H */
