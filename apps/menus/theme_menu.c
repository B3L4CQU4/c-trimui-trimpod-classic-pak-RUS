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

/* Trimpod: themes are hardcoded to the iPod-style skin, so there is no Theme
 * Settings menu here.  browse_folder() remains because the Language picker, the
 * .cfg config browser, and the EQ browser use it. */

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"
#include "lang.h"
#include "settings.h"
#include "rbpaths.h"
#include "dir.h"
#include "tree.h"
#include "root_menu.h"
#include "exported_menus.h"
#include "splash.h"

int browse_folder(void *param)
{
    const char *ext, *setting;
    int lang_id = -1;
    char selected[MAX_FILENAME+10];
    const struct browse_folder_info *info =
        (const struct browse_folder_info*)param;

    struct browse_context browse = {
        .dirfilter = info->show_options,
        .icon = Icon_NOICON,
        .root = info->dir,
    };

    if (!dir_exists(info->dir)) {
        splash(HZ, ID2P(LANG_PLAYLIST_DIRECTORY_ACCESS_ERROR));
        return GO_TO_PREVIOUS;
    }

    /* if we are in a special settings folder, center the current setting */
    switch(info->show_options)
    {
        case SHOW_LNG:
            ext = "lng";
            if (global_settings.lang_file[0])
                setting = global_settings.lang_file;
            else
                setting = "english";
            lang_id = LANG_LANGUAGE;
            break;
        default:
            ext = setting = NULL;
            break;
    }

    /* If we've found a file to center on, do it */
    if (setting)
    {
        /* if setting != NULL, ext is initialized */
        snprintf(selected, sizeof(selected), "%s.%s", setting, ext);
        browse.selected = selected;
        browse.icon = Icon_Questionmark;
        browse.title = str(lang_id);
    }

    tree_get_context()->browse = NULL;  /*bugfix - force root dir reload */
    return rockbox_browse(&browse);
}
