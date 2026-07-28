/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2010 by Thomas Martitz
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
 * table lands on evenly-spaced perceived-loudness steps instead of snapping to
 * coarse whole-dB values (the old "dB",0,1 quantized every notch to 1 dB, which
 * made the rocker steps -- and the volume bar -- visibly/audibly uneven).  Range
 * -80.0..-1.5 dB: the -1.5 dB ceiling keeps a sliver of speaker headroom while
 * staying near the device's shipped maximum.  Default -22.0 dB.
 * NOTE: min/max/default are in 0.1 dB units; the storage unit changed, so any
 * saved whole-dB "volume:" value must be migrated (x10) or reset -- see the pak
 * config.cfg and the clean-deploy requirement. */
AUDIOHW_SETTING(VOLUME,      "dB",   1,  1, -800, -15, -220)

#endif /* _TRIMPOD_CODEC_H */
