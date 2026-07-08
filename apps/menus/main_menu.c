/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 Jonathan Gordon
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

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include "config.h"
#include "string.h"
#include "lang.h"
#include "action.h"
#include "settings.h"
#include "power.h"
#include "powermgmt.h"
#include "menu.h"
#include "misc.h"
#include "exported_menus.h"
#include "trimpod_mainmenu.h"
#include "tree.h"
#include "storage.h"
#include "yesno.h"
#include "keyboard.h"
#include "screens.h"
#include "splash.h"
#include "dir.h"
#include "version.h"
#include "time.h"
#include "wps.h"
#include "skin_buffer.h"
#include "disk.h"
#include "trimpod_ui.h"

static const struct browse_folder_info config = {ROCKBOX_DIR, SHOW_CFG};
/***********************************/
/*    MANAGE SETTINGS MENU        */

static int reset_settings(void)
{
    static const char *lines[]={ID2P(LANG_ARE_YOU_SURE), ID2P(LANG_RESET)};
    static const char *yes_lines[]={
        ID2P(LANG_SETTINGS),
        ID2P(LANG_RESET_DONE_CLEAR)
    };
    static const char *no_lines[]={
        ID2P(LANG_SETTINGS),
        ID2P(LANG_CANCEL)
    };
    static const struct text_message message={lines, 2};
    static const struct text_message yes_message={yes_lines, 2};
    static const struct text_message no_message={no_lines, 2};

    switch(gui_syncyesno_run(&message, &yes_message, &no_message))
    {
        case YESNO_YES:
            settings_reset();
            settings_save();
            settings_apply(true);
            settings_apply_skins();
            break;
        case YESNO_TMO:
        case YESNO_NO:
            break;
        case YESNO_USB:
            return 1;
    }
    return 0;
}
static int write_settings_file(void* param)
{
    return settings_save_config((intptr_t)param);
}

MENUITEM_FUNCTION_W_PARAM(browse_configs, 0, ID2P(LANG_CUSTOM_CFG),
                          browse_folder, (void*)&config, NULL, Icon_NOICON);
MENUITEM_FUNCTION_W_PARAM(save_settings_item, 0, ID2P(LANG_SAVE_SETTINGS),
                          write_settings_file, (void*)SETTINGS_SAVE_ALL,
                          NULL, Icon_NOICON);
MENUITEM_FUNCTION_W_PARAM(save_theme_item, 0, ID2P(LANG_SAVE_THEME),
                          write_settings_file, (void*)SETTINGS_SAVE_THEME,
                          NULL, Icon_NOICON);
MENUITEM_FUNCTION_W_PARAM(save_sound_item, 0, ID2P(LANG_SAVE_SOUND),
                          write_settings_file, (void*)SETTINGS_SAVE_SOUND,
                          NULL, Icon_NOICON);
MENUITEM_FUNCTION(reset_settings_item, 0, ID2P(LANG_RESET),
                  reset_settings, NULL, Icon_NOICON);

MAKE_MENU(manage_settings, ID2P(LANG_MANAGE_MENU), NULL, Icon_Config,
          &browse_configs, &reset_settings_item,
          &save_settings_item, &save_sound_item, &save_theme_item);
/*    MANAGE SETTINGS MENU        */
/**********************************/


/* About: a scrollable credits list (title from %Lt; scroll with up/down, B exits). */
static int trimpod_about_page(void)
{
    trimpod_about();
    return 0;
}

/* The Power, Audio Folders, Visualizer Effects and Advanced sub-menus each live
 * in their own file (power_menu.c, audio_folders_menu.c, visualizer_menu.c,
 * advanced_menu.c); their page openers are declared in exported_menus.h. */

/* Settings list: nav rows (chevron) open sub-pages via do_menu. */
MENUITEM_FUNCTION(tp_set_mainmenu, MENU_SHOW_CHEVRON, ID2P(LANG_TRIMPOD_MAINMENU),
                  trimpod_mainmenu_settings, NULL, Icon_NOICON);
MENUITEM_FUNCTION(tp_set_audio, MENU_SHOW_CHEVRON, ID2P(LANG_TRIMPOD_LIBRARY),
                  trimpod_audio_folders_page, NULL, Icon_NOICON);
MENUITEM_FUNCTION(tp_set_viz, MENU_SHOW_CHEVRON, ID2P(LANG_TRIMPOD_AUDIO_VISUALIZER),
                  trimpod_av_page, NULL, Icon_NOICON);
MENUITEM_FUNCTION(tp_set_power, MENU_SHOW_CHEVRON, ID2P(LANG_TRIMPOD_DEVICE),
                  trimpod_power_page, NULL, Icon_NOICON);
MENUITEM_FUNCTION(tp_set_about, MENU_SHOW_CHEVRON, ID2P(LANG_TRIMPOD_ABOUT),
                  trimpod_about_page, NULL, Icon_NOICON);
MAKE_MENU(trimpod_settings_menu, ID2P(LANG_SETTINGS), NULL, Icon_Submenu_Entered,
          &tp_set_mainmenu, &tp_set_audio, &tp_set_viz, &sound_settings, &tp_set_power,
          &tp_set_about);
int trimpod_settings_page(void)
{
    do_menu(&trimpod_settings_menu, NULL, NULL, false);
    return 0;
}
