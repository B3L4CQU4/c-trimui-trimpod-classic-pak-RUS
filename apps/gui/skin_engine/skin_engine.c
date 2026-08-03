/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2010 Jonathan Gordon
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
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include "inttypes.h"
#include "config.h"
#include "action.h"
#include "crc32.h"
#include "settings.h"
#include "rbpaths.h"
#include "wps.h"
#include "file.h"
#include "misc.h"
#include "gui/list.h"
#include "skin_engine.h"
#include "skin_buffer.h"
#include "statusbar-skinned.h"
#include "wps_internals.h"

#define FAILSAFENAME "rockbox_failsafe"

void skin_data_free_buflib_allocs(struct wps_data *wps_data);
char* wps_default_skin(enum screen_type screen);
static bool skins_initialised = false;

static char* get_skin_filename(char *buf, size_t buf_size,
                               enum skinnable_screens skin, enum screen_type screen);

struct gui_skin_helper {
    void (*process)(enum screen_type screen, struct wps_data *data, bool preprocess);
    char* (*default_skin)(enum screen_type screen);
    bool load_on_boot;
};

void dummy_process(enum screen_type screen, struct wps_data *data, bool preprocess)
{ (void)screen, (void)data, (void)preprocess; } /* dummy replaces conditionals */

static const struct gui_skin_helper empty_skin_helper = {&dummy_process,NULL,false};
static const struct gui_skin_helper * const skin_helpers[SKINNABLE_SCREENS_COUNT] =
{
#define SKH(proc, def, lob) &((struct gui_skin_helper){proc, def, lob})
    &empty_skin_helper,
    [CUSTOM_STATUSBAR] = SKH(sb_process, sb_create_from_settings, true),
    [WPS] =  SKH(dummy_process, wps_default_skin, true),
};

static struct gui_skin {
    struct gui_wps      gui_wps;
    struct wps_data     data;
    struct skin_stats   stats;
    bool                failsafe_loaded;
    bool                needs_full_update;
} skins[SKINNABLE_SCREENS_COUNT][NB_SCREENS];

int skin_get_num_skins(void)
{
    return SKINNABLE_SCREENS_COUNT;
}

struct skin_stats *skin_get_stats(int number, int screen)
{
    return &skins[number][screen].stats;
}

static void gui_skin_reset(struct gui_skin *skin)
{
    struct wps_data *data;
    skin->failsafe_loaded = false;
    skin->needs_full_update = true;
    skin->gui_wps.data = data = &skin->data;
        memset(data, 0, sizeof(struct wps_data));
    skin->data.wps_loaded = false;
    skin->data.buflib_handle = -1;
    skin->data.tree = -1;
    skin->data.font_ids = -1;
    skin->data.images = -1;
#ifdef HAVE_BACKDROP_IMAGE
    skin->gui_wps.data->backdrop_id = -1;
#endif
}

void gui_sync_skin_init(void)
{
    int j;
    for(j=0; j<SKINNABLE_SCREENS_COUNT; j++)
    {
        FOR_NB_SCREENS(i)
        {
            skin_data_free_buflib_allocs(&skins[j][i].data);
#ifdef HAVE_BACKDROP_IMAGE
            if (skins[j][i].data.backdrop_id != -1)
                skin_backdrop_unload(skins[j][i].data.backdrop_id);
#endif
            gui_skin_reset(&skins[j][i]);
            skins[j][i].gui_wps.display = &screens[i];
        }
    }
}

static void skin_reset_buffers(int item, int screen)
{
    skin_data_free_buflib_allocs(&skins[item][screen].data);
#ifdef HAVE_BACKDROP_IMAGE
    if (skins[item][screen].data.backdrop_id >= 0)
        skin_backdrop_unload(skins[item][screen].data.backdrop_id);
#endif
}

/* Recolour one element subtree in place: viewports still on the previous theme
 * default, plus cached %Vf(-)/%Vb(-) tag colours -- those re-apply at render
 * time, so leaving them stale would revert their viewport on the next draw.
 * %dr rectangles cache the fg colour at parse time too (dividers, boxes), so
 * the fg pass recolours those as well. */
static void skin_recolour_tree(char *buf, struct skin_element *e, bool fg,
                               unsigned old_colour, unsigned new_colour)
{
    for (; e; e = SKINOFFSETTOPTR(buf, e->next))
    {
        if (e->type == VIEWPORT)
        {
            struct skin_viewport *svp = SKINOFFSETTOPTR(buf, e->data);
            unsigned *pat = svp ? (fg ? &svp->vp.fg_pattern
                                      : &svp->vp.bg_pattern) : NULL;
            if (pat && *pat == old_colour)
                *pat = new_colour;
        }
        else if (e->type == TAG && e->tag &&
                 e->tag->type == (fg ? SKIN_TOKEN_VIEWPORT_FGCOLOUR
                                     : SKIN_TOKEN_VIEWPORT_BGCOLOUR))
        {
            struct wps_token *tok = SKINOFFSETTOPTR(buf, e->data);
            struct viewport_colour *col =
                tok ? SKINOFFSETTOPTR(buf, tok->value.data) : NULL;
            if (col && col->colour == old_colour)
                col->colour = new_colour;
        }
        else if (e->type == TAG && e->tag && fg &&
                 e->tag->type == SKIN_TOKEN_DRAWRECTANGLE)
        {
            struct wps_token *tok = SKINOFFSETTOPTR(buf, e->data);
            struct draw_rectangle *rect =
                tok ? SKINOFFSETTOPTR(buf, tok->value.data) : NULL;
            if (rect)
            {
                if (rect->start_colour == old_colour)
                    rect->start_colour = new_colour;
                if (rect->end_colour == old_colour)
                    rect->end_colour = new_colour;
            }
        }
        OFFSETTYPE(struct skin_element*) *kids =
            SKINOFFSETTOPTR(buf, e->children);
        for (int i = 0; kids && i < e->children_count; i++)
            skin_recolour_tree(buf, SKINOFFSETTOPTR(buf, kids[i]), fg,
                               old_colour, new_colour);
    }
}

/* Recolour the loaded skins' default fore/background in place -- no reparse,
 * so a live colour change never has to stop playback.  Only colours still on
 * the previous theme default are touched; explicit and transparent viewport
 * colours keep theirs. */
static void skin_update_colour(bool fg, unsigned old_colour, unsigned new_colour)
{
    int item, screen;
    if (old_colour == new_colour)
        return;
    for (item = 0; item < SKINNABLE_SCREENS_COUNT; item++)
    {
        for (screen = 0; screen < NB_SCREENS; screen++)
        {
            char *buf = get_skin_buffer(&skins[item][screen].data);
            if (!buf)
                continue;
            skin_recolour_tree(buf,
                SKINOFFSETTOPTR(buf, skins[item][screen].data.tree),
                fg, old_colour, new_colour);
        }
    }
}

void skin_update_bg_color(unsigned old_bg, unsigned new_bg)
{
    skin_update_colour(false, old_bg, new_bg);
}

void skin_update_fg_color(unsigned old_fg, unsigned new_fg)
{
    skin_update_colour(true, old_fg, new_fg);
}

void settings_apply_skins(void)
{
    int i;
    char filename[MAX_PATH];

    if (audio_status() & AUDIO_STATUS_PLAY)
        audio_stop();

    bool first_run = skin_backdrop_init();

    if (!first_run)
    {
        /* Make sure all skins unloaded */
        for (i=0; i<SKINNABLE_SCREENS_COUNT; i++)
        {
            FOR_NB_SCREENS(j)
                skin_reset_buffers(i, j);
        }
    }
    skins_initialised = true;

    /* Make sure each skin is loaded */
    for (i=0; i<SKINNABLE_SCREENS_COUNT; i++)
    {
        FOR_NB_SCREENS(j)
        {
            get_skin_filename(filename, MAX_PATH, i,j);
            gui_skin_reset(&skins[i][j]);
            skins[i][j].gui_wps.display = &screens[j];
            if (skin_helpers[i]->load_on_boot)
                skin_get_gwps(i, j);
        }
    }

    /* any backdrop that was loaded with "-" has to be reloaded because
     * the setting may have changed */
    skin_backdrop_load_setting();
    viewportmanager_theme_changed(THEME_STATUSBAR);

    FOR_NB_SCREENS(i)
        skin_backdrop_show(sb_get_backdrop(i));
}

void skin_load(enum skinnable_screens skin, enum screen_type screen,
               const char *buf, bool isfile)
{
    bool loaded = false;

    skin_helpers[skin]->process(screen, &skins[skin][screen].data, true);

    if (buf && *buf)
        loaded = skin_data_load(screen, &skins[skin][screen].data, buf, isfile,
                                &skins[skin][screen].stats);

    if (!loaded && skin_helpers[skin]->default_skin)
    {
        loaded = skin_data_load(screen, &skins[skin][screen].data,
                                skin_helpers[skin]->default_skin(screen), false,
                                &skins[skin][screen].stats);
        skins[skin][screen].failsafe_loaded = loaded;
    }

    skins[skin][screen].needs_full_update = true;
    skin_helpers[skin]->process(screen, &skins[skin][screen].data, false);
#ifdef HAVE_BACKDROP_IMAGE
    if (loaded)
        skin_backdrops_preload();
#endif
}

static char* get_skin_filename(char *buf, size_t buf_size,
                                enum skinnable_screens skin, enum screen_type screen)
{
    (void)screen;
    char *setting = NULL, *ext = NULL;
    switch (skin)
    {
        case CUSTOM_STATUSBAR:
            {
                setting = global_settings.sbs_file;
                ext = "sbs";
            }
            break;
        case WPS:
            {
                setting = global_settings.wps_file;
                ext = "wps";
            }
            break;
        default:
            return NULL;
    }

    buf[0] = '\0'; /* force it to reload the default */
    if (strcmp(setting, FAILSAFENAME) && strcmp(setting, "-"))
    {
        snprintf(buf, buf_size, WPS_DIR "/%s.%s", setting, ext);
    }
    return buf;
}

struct gui_wps *skin_get_gwps(enum skinnable_screens skin, enum screen_type screen)
{
    if (skin == CUSTOM_STATUSBAR && !skins_initialised)
        return &skins[skin][screen].gui_wps;

    if (skins[skin][screen].data.wps_loaded == false)
    {
        char filename[MAX_PATH];
        char *buf = get_skin_filename(filename, MAX_PATH, skin, screen);
        cpu_boost(true);
        skin_load(skin, screen, buf, true);
        cpu_boost(false);
    }
    return &skins[skin][screen].gui_wps;
}

/* This is called to find out if we the screen needs a full update.
 * if true you MUST do a full update as the next call will return false */
bool skin_do_full_update(enum skinnable_screens skin,
                            enum screen_type screen)
{
    struct viewport *vp = *(screens[screen].current_viewport);

    bool vp_is_dirty = ((vp->flags & VP_FLAG_VP_SET_CLEAN) == VP_FLAG_VP_DIRTY) &&
                       get_current_activity() == ACTIVITY_WPS;

    bool ret = (skins[skin][screen].needs_full_update || vp_is_dirty);
    skins[skin][screen].needs_full_update = false;
    return ret;
}

/* tell a skin to do a full update next time */
void skin_request_full_update(enum skinnable_screens skin)
{
    FOR_NB_SCREENS(i)
        skins[skin][i].needs_full_update = true;
}


/* Request skin update for lock state change */
void skin_request_update_locked(bool locked)
{
    if (get_current_activity() == ACTIVITY_WPS)
        return;

    sb_skin_force_next_update();

    /* fix themes that draw on top of the UI viewport when locked */
    if (!locked)
        skin_request_full_update(CUSTOM_STATUSBAR);
#ifdef HAS_BUTTON_HOLD
    button_queue_post(BUTTON_NONE, 0);
#endif
}

bool dbg_skin_engine(void)
{
    struct simplelist_info info;
    int i, total = 0;
#if defined(HAVE_BACKDROP_IMAGE)
    int ref_count;
    char *path;
    size_t bytes;
    int path_prefix_len = strlen(ROCKBOX_DIR "/wps/");
#endif
    simplelist_info_init(&info, "Skin engine usage", 0, NULL);
    simplelist_reset_lines();
    FOR_NB_SCREENS(j) {
#if NB_SCREENS > 1
        simplelist_addline("%s display:",
                           j == 0 ? "Main" : "Remote");
#endif
        for (i = 0; i < skin_get_num_skins(); i++) {
            struct skin_stats *stats = skin_get_stats(i, j);
            if (stats->buflib_handles)
            {
                simplelist_addline("Skin ID: %d, %zd allocations",
                        i, stats->buflib_handles);
                simplelist_addline("\t%s: %zd bytes",
                        "Skin", stats->tree_size);
                simplelist_addline("\t%s: %zd bytes",
                        "Images", stats->images_size);
                simplelist_addline("\t%s: %zd bytes",
                        "Total", stats->tree_size + stats->images_size);
                total += stats->tree_size + stats->images_size;
            }
        }
    }
    simplelist_addline("%s usage: %d bytes", "Skin total", total);
#if defined(HAVE_BACKDROP_IMAGE)
    simplelist_setline("Backdrop Images:");
    i = 0;
    while (skin_backdrop_get_debug(i++, &path, &ref_count, &bytes)) {
        if (ref_count > 0) {

            if (!strncasecmp(path, ROCKBOX_DIR "/wps/", path_prefix_len))
                path += path_prefix_len;
            simplelist_addline("%s", path);
            simplelist_addline("\tref_count: %d", ref_count);
            simplelist_addline("\tsize: %zd", bytes);
            total += bytes;
        }
    }
    simplelist_addline("%s usage: %d bytes", "Total", total);
#endif
    return simplelist_show_list(&info);
}
