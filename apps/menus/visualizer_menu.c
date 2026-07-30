/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Trimpod: the Settings -> Visualizers menu -- "Start Visualizer After" (WPS idle
 * before auto-start) and "Visualization Transition" (seconds per preset),
 * persisted settings rows, plus the presets toggle list.
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

#include <stddef.h>
#include "config.h"
#include "lang.h"
#include "settings.h"
#include "menu.h"
#include "trimpod_visualizer.h"
#include "exported_menus.h"

MENUITEM_SETTING(tp_av_delay, &global_settings.viz_start_delay, NULL);
MENUITEM_SETTING(tp_av_transition, &global_settings.viz_transition, NULL);
MENUITEM_FUNCTION(tp_av_presets, MENU_SHOW_CHEVRON, ID2P(LANG_TRIMPOD_VISUALIZERS),
                  trimpod_visualizer_menu, NULL, Icon_NOICON);
MAKE_MENU(trimpod_av_menu, ID2P(LANG_TRIMPOD_AUDIO_VISUALIZER), NULL,
          Icon_Submenu_Entered,
          &tp_av_presets,                      /* sub-page first */
          &tp_av_delay, &tp_av_transition);    /* then in-page settings */

int trimpod_av_page(void)
{
    do_menu(&trimpod_av_menu, NULL, NULL, false);
    return 0;
}
