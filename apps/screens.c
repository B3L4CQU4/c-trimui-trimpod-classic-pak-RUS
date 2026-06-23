/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 Björn Stenberg
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
#include <string-extra.h>
#include <stdio.h>
#include <stdlib.h>
#include "backlight.h"
#include "action.h"
#include "lcd.h"
#include "lang.h"
#include "icons.h"
#include "font.h"
#include "audio.h"
#include "usb.h"
#include "settings.h"
#include "status.h"
#include "playlist.h"
#include "kernel.h"
#include "power.h"
#include "system.h"
#include "powermgmt.h"
#include "misc.h"
#include "screens.h"
#include "debug.h"
#include "led.h"
#include "sound.h"
#include "splash.h"
#include "statusbar.h"
#include "screen_access.h"
#include "list.h"
#include "yesno.h"
#include "backdrop.h"
#include "viewport.h"
#include "language.h"
#include "replaygain.h"

#include "ctype.h"

#if CONFIG_CHARGING
void charging_splash(void)
{
    splash(2*HZ, str(LANG_BATTERY_CHARGE));
    button_clear_queue();
}
#endif



static const int id3_headers[]=
{
    LANG_TAGNAVI_ALL_TRACKS,
    LANG_ID3_TITLE,
    LANG_ID3_ARTIST,
    LANG_ID3_COMPOSER,
    LANG_ID3_ALBUM,
    LANG_ID3_ALBUMARTIST,
    LANG_ID3_GROUPING,
    LANG_ID3_DISCNUM,
    LANG_ID3_TRACKNUM,
    LANG_ID3_COMMENT,
    LANG_ID3_GENRE,
    LANG_ID3_YEAR,
    LANG_ID3_LENGTH,
    LANG_ID3_PLAYLIST,
    LANG_FORMAT,
    LANG_ID3_BITRATE,
    LANG_ID3_FREQUENCY,
    LANG_ID3_TRACK_GAIN,
    LANG_ALBUM_GAIN,
    LANG_FILESIZE,
    LANG_ID3_PATH,
    LANG_DATE,
    LANG_TIME,
};

struct id3view_info {
    struct mp3entry* id3;
    struct tm *modified;
    int track_ct;
    int count;
    struct playlist_info *playlist;
    int playlist_display_index;
    int playlist_amount;
    int info_id[ARRAYLEN(id3_headers)];
};

static const char * id3_get_or_speak_info(int selected_item, void* data,
                                          char *buffer, size_t buffer_len)
{
    struct id3view_info *info = (struct id3view_info*)data;
    struct mp3entry* id3 =info->id3;
    const unsigned char * const *unit;
    unsigned int unit_ct;
    unsigned long length;
    bool pl_modified;
    struct tm *tm = info->modified;
    int info_no=selected_item/2;
    if(!(selected_item%2))
    {/* header */
        snprintf(buffer, buffer_len,
                 info->info_id[info_no] > 0 ? "[%s]" : "%s",
                 str(id3_headers[info->info_id[info_no]]));
        return buffer;
    }
    else
    {/* data */

        char * val=NULL;
        switch(id3_headers[info->info_id[info_no]])
        {
            case LANG_TAGNAVI_ALL_TRACKS:
                if (info->track_ct <= 1)
                    return NULL;
                itoa_buf(buffer, buffer_len, info->track_ct);
                val = buffer;
                break;
            case LANG_ID3_TITLE:
                val=id3->title;
                break;
            case LANG_ID3_ARTIST:
                val=id3->artist;
                break;
            case LANG_ID3_ALBUM:
                val=id3->album;
                break;
            case LANG_ID3_ALBUMARTIST:
                val=id3->albumartist;
                break;
            case LANG_ID3_GROUPING:
                val=id3->grouping;
                break;
            case LANG_ID3_DISCNUM:
                if (id3->disc_string)
                {
                    val = id3->disc_string;
                }
                else if (id3->discnum)
                {
                    itoa_buf(buffer, buffer_len, id3->discnum);
                    val = buffer;
                }
                break;
            case LANG_ID3_TRACKNUM:
                if (id3->track_string)
                {
                    val = id3->track_string;
                }
                else if (id3->tracknum >= 0)
                {
                    itoa_buf(buffer, buffer_len, id3->tracknum);
                    val = buffer;
                }
                break;
            case LANG_ID3_COMMENT:
                if (!id3->comment)
                    return NULL;
                val = id3->comment;
                break;
            case LANG_ID3_GENRE:
                val = id3->genre_string;
                break;
            case LANG_ID3_YEAR:
                if (id3->year_string)
                {
                    val = id3->year_string;
                }
                else if (id3->year)
                {
                    itoa_buf(buffer, buffer_len, id3->year);
                    val = buffer;
                }
                break;
            case LANG_ID3_LENGTH:
                length = info->track_ct > 1 ? id3->length : id3->length / 1000;

                format_time_auto(buffer, buffer_len,
                                 length, UNIT_SEC | UNIT_TRIM_ZERO, true);
                val=buffer;
                break;
            case LANG_ID3_PLAYLIST:
                if (info->playlist_display_index == 0 || info->playlist_amount == 0 )
                    return NULL;

                pl_modified = playlist_modified(info->playlist);

                snprintf(buffer, buffer_len, "%d/%d%s",
                         info->playlist_display_index, info->playlist_amount,
                         pl_modified ? "* " :"  ");
                val = buffer;
                size_t prefix_len = strlen(buffer);
                buffer += prefix_len;
                buffer_len -= prefix_len;

                if (info->playlist)
                     playlist_name(info->playlist, buffer, buffer_len);
                else
                {
                    if (playlist_allow_dirplay(NULL))
                        strmemccpy(buffer, "(Folder)", buffer_len);
                    else if (playlist_dynamic_only())
                        strmemccpy(buffer, "(Dynamic)", buffer_len);
                    else
                        playlist_name(NULL, buffer, buffer_len);
                }
                break;
            case LANG_FORMAT:
                if (id3->codectype == AFMT_UNKNOWN && info->track_ct > 1)
                    return NULL;

                val = (char*) get_codec_string(id3->codectype);
                break;
            case LANG_ID3_BITRATE:
                if (!id3->bitrate)
                    return NULL;
                snprintf(buffer, buffer_len, "%d kbps%s%s", id3->bitrate,
            id3->vbr ? " " : "",
            id3->vbr ? str(LANG_ID3_VBR) : (const unsigned char*) "");
                val=buffer;
                break;
            case LANG_ID3_FREQUENCY:
                if (!id3->frequency)
                    return NULL;
                snprintf(buffer, buffer_len, "%ld Hz", id3->frequency);
                val=buffer;
                break;
            case LANG_ID3_TRACK_GAIN:
                replaygain_itoa(buffer, buffer_len, id3->track_level);
                val=(id3->track_level) ? buffer : NULL; /* only show level!=0 */
                break;
            case LANG_ALBUM_GAIN:
                replaygain_itoa(buffer, buffer_len, id3->album_level);
                val=(id3->album_level) ? buffer : NULL; /* only show level!=0 */
                break;
            case LANG_ID3_PATH:
                val=id3->path;
                break;
            case LANG_ID3_COMPOSER:
                val=id3->composer;
                break;
            case LANG_FILESIZE: /* not LANG_ID3_FILESIZE because the string is shared */
                if (!id3->filesize)
                    return NULL;
                if (info->track_ct > 1)
                {
                    unit = kibyte_units;
                    unit_ct = 3;
                }
                else
                {
                    unit = byte_units;
                    unit_ct = 4;
                }
                output_dyn_value(buffer, buffer_len, id3->filesize, unit, unit_ct, true);
                val=buffer;
                break;
            case LANG_DATE:
                if (!tm)
                    return NULL;

                snprintf(buffer, buffer_len, "%04d/%02d/%02d",
                         tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

                val = buffer;
                break;
            case LANG_TIME:
                if (!tm)
                    return NULL;

                snprintf(buffer, buffer_len, "%02d:%02d:%02d",
                         tm->tm_hour, tm->tm_min, tm->tm_sec);

                val = buffer;
                break;
        }
        return val && *val ? val : NULL;
    }
}

/* gui_synclist callback */
static const char* id3_get_name_cb(int selected_item, void* data,
                                   char *buffer, size_t buffer_len)
{
    return id3_get_or_speak_info(selected_item, data, buffer,
                                 buffer_len) ? : "";
}

/* Note: If track_ct > 1, filesize value will be treated as
 * KiB (instead of Bytes), and length as s instead of ms.
 */
bool browse_id3_ex(struct mp3entry *id3, struct playlist_info *playlist,
                int playlist_display_index, int playlist_amount,
                struct tm *modified, int track_ct,
                int (*view_text)(const char *title, const char *text))
{
    struct gui_synclist id3_lists;
    int key;
    unsigned int i;
    struct id3view_info info;
    info.id3 = id3;
    info.modified = modified;
    info.track_ct = track_ct;
    info.playlist = playlist;
    info.playlist_amount = playlist_amount;
    bool ret = false;
    int curr_activity = get_current_activity();
    bool is_curr_track_info = curr_activity != ACTIVITY_PLAYLISTVIEWER;
    if (is_curr_track_info)
        push_current_activity(ACTIVITY_ID3SCREEN);
refresh_info:
    info.count = 0;
    info.playlist_display_index = playlist_display_index;
    for (i = 0; i < ARRAYLEN(id3_headers); i++)
    {
        char temp[8];
        info.info_id[i] = i;
        if (id3_get_or_speak_info((i*2)+1, &info, temp, 8) != NULL)
            info.info_id[info.count++] = i;
    }

    gui_synclist_init(&id3_lists, &id3_get_name_cb, &info, true, 2, NULL);
    gui_synclist_set_nb_items(&id3_lists, info.count*2);
    gui_synclist_set_title(&id3_lists, str(LANG_TRACK_INFO), NOICON);
    gui_synclist_draw(&id3_lists);
    while (true) {
        if(!list_do_action(CONTEXT_LIST,HZ/2, &id3_lists, &key)
           && key!=ACTION_NONE && key!=ACTION_UNKNOWN)
        {
            if (key == ACTION_STD_OK)
            {
                gui_synclist_scroll_stop(&id3_lists);
                int header_id = id3_headers[info.info_id[id3_lists.selected_item/2]];
                char* title_and_text[2];
                title_and_text[0] = str(header_id);

                char buffer[MAX_PATH];
                title_and_text[1] = (char*)id3_get_or_speak_info(id3_lists.selected_item+1,&info, buffer, sizeof(buffer));

                if (view_text)
                    view_text(title_and_text[0], title_and_text[1]);
                gui_synclist_set_title(&id3_lists, str(LANG_TRACK_INFO), NOICON);
                gui_synclist_draw(&id3_lists);
                continue;
            }
            if (key == ACTION_STD_CANCEL)
            {
                ret = false;
                break;
            }
            else if (key == ACTION_STD_MENU ||
                        default_event_handler(key) == SYS_USB_CONNECTED)
            {
                ret =  true;
                break;
            }
        }
        else if (is_curr_track_info)
        {
            if (!audio_status())
            {
                ret = false;
                break;
            }
            else
            {
                playlist_display_index = playlist_get_display_index();
                if (playlist_display_index != info.playlist_display_index)
                    goto refresh_info;
            }
        }
    }
    FOR_NB_SCREENS(i)
        screens[i].scroll_stop(); /* when custom lists are used */

    if (is_curr_track_info)
        pop_current_activity();
    return ret;
}

bool browse_id3(struct mp3entry *id3, int playlist_display_index, int playlist_amount,
                struct tm *modified, int track_ct,
                int (*view_text)(const char *title, const char *text))
{
    return browse_id3_ex(id3, NULL, playlist_display_index, playlist_amount,
                         modified, track_ct, view_text);
}


