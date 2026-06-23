/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2006 Jonathan Gordon
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
#ifndef _EXPORTED_MENUS_H
#define _EXPORTED_MENUS_H

#include "menu.h"
/* not needed for plugins */
#ifndef PLUGIN 

extern const struct menu_item_ex
        sound_settings,             /* sound_menu.c     */
#ifdef AUDIOHW_HAVE_EQ
        audiohw_eq_tone_controls,   /* audiohw_eq_menu.c */
#endif
        equalizer_menu;             /* eq_menu.c        */

/* Trimpod sub-menu page openers (each in its own per-menu file). */
int trimpod_settings_page(void);        /* main_menu.c */
int trimpod_power_page(void);           /* power_menu.c */
int trimpod_audio_folders_page(void);   /* audio_folders_menu.c */
int trimpod_av_page(void);              /* visualizer_menu.c */

struct browse_folder_info {
    const char* dir;
    int show_options;
};
int browse_folder(void *param); /* lives in theme_menu.c; used by Language/.cfg/EQ browsers */

#endif /* ! PLUGIN */
#endif /*_EXPORTED_MENUS_H */
