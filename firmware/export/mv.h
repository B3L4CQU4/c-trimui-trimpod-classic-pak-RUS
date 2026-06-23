/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2008 by Frank Gevaerts
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

#ifndef __MV_H__
#define __MV_H__

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

/* FixMe: These macros are a bit nasty and perhaps misplaced here.
   We'll get rid of them once decided on how to proceed with multivolume. */

/* Drives are things like a disk, a card, a flash chip. They can have volumes
   on them */
#define IF_MD(x...)
#define IF_MD_NONVOID(x...) void
#define IF_MD_DRV(d)  0

/* Storage size */
typedef unsigned long sector_t;
#undef STORAGE_64BIT_SECTOR


/* Volumes mean things that have filesystems on them, like partitions */
#define IF_MV(x...)
#define IF_MV_NONVOID(x...) void
#define IF_MV_VOL(v) 0

#define CHECK_VOL(volume) \
    ((unsigned int)IF_MV_VOL(volume) < NUM_VOLUMES)

#define CHECK_DRV(drive) \
    ((unsigned int)IF_MD_DRV(drive) < NUM_DRIVES)

/* contains info about a volume */
struct volumeinfo
{
    int drive;      /* drive number */
    int partition;  /* partition number (0 for superfloppy drives) */
};

/* Volume-centric functions (in disk.c) */
void volume_recalc_free(IF_MV_NONVOID(int volume));
unsigned int volume_get_cluster_size(IF_MV_NONVOID(int volume));
void volume_size(IF_MV(int volume,) sector_t *size, sector_t *free);
#define volume_present(x) 1
#define volueme_removeable(x) 0

static inline int volume_drive(int volume)
{
    return 0;
    (void)volume;
}

int volume_partition(int volume);

#endif /* __MV_H__ */
