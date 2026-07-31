/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Shared scratch RAM. Only one consumer uses it at a time -- every user
 * (cuesheet load, playlist viewer, skin parse, file/folder ops) is a modal
 * operation, and they never run concurrently.
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

#ifndef __SCRATCH_BUF_H__
#define __SCRATCH_BUF_H__

#include <stddef.h>

/* Return the shared scratch buffer; *size (if non-NULL) is set to its size. */
void *scratch_buffer_get(size_t *size);

#endif /* __SCRATCH_BUF_H__ */
