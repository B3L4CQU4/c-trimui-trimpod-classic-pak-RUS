/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2006 Dan Everton
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

#ifndef __UISDL_H__
#define __UISDL_H__

#include <stdbool.h>
#include "SDL.h"
#include "config.h"

/* colour definitions are R, G, B */

#if   defined(APPLICATION)
#define UI_TITLE                    "Rockbox"
#define UI_LCD_POSX                 0
#define UI_LCD_POSY                 0
#define UI_WIDTH                    LCD_WIDTH
#define UI_HEIGHT                   LCD_HEIGHT

#endif

#endif /* #ifndef __UISDL_H__ */
