/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2004 by Linus Nielsen Feltzing
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
#ifndef __CPU_H
#define __CPU_H

#include "config.h"

/* Trimpod targets only the hosted TrimUI Brick (CONFIG_CPU == 0); the per-CPU
 * register headers and native-arch cache setup do not apply.  The guards below
 * stay as assertions that no CPU-cache config leaks in. */

# if defined(CACHEALIGN_BITS) && defined(CACHEALIGN_SIZE)
#  error "CACHEALIGN_BITS and CACHEALIGN_SIZE must not be defined for targets with no CPU cache"
# endif

/*
 * Note: NOCACHE_BASE assumes that DRAM is linearly mapped both
 * at a lower cached address and an upper uncached address, so
 * that you can add NOCACHE_BASE to the cached DRAM address to
 * get the corresponding uncached address.
 *
 * Defining NOCACHE_BASE is only required if you need plugins to
 * be able to link data at uncached addresses. If in doubt, you
 * don't need this. It's mainly of use for dual-core PortalPlayer
 * targets which need to do this for things like mutexes/queues;
 * since PP lacks hardware cache coherency, data which is writable
 * by more than one core often needs to accessed uncached.
 */
#if defined(NOCACHE_BASE)
#  error "NOCACHE_BASE cannot be defined on targets with no CPU cache!"
#endif

#endif /* __CPU_H */
