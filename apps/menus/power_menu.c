/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Trimpod: the Power (Device) settings menu -- CPU Frequency, Brightness,
 * Colour, Auto Screen Off, Idle Power Off -- on one do_menu page. CPU and
 * Colour are inline value knobs (LEFT/RIGHT cycle, applied live); the others
 * are plain settings rows.
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
#include "config.h"
#include "lang.h"
#include "settings.h"
#include "menu.h"
#include "screen_access.h" /* screens[].set_background for the bg colour presets */
#include "viewport.h"       /* viewportmanager_theme_changed: full redraw on colour change */
#include "exported_menus.h"

/* Colour: pick the UI background from the iPod-family colour lineup (iPod mini
 * + nano tones; Apple never published official hexes, so these are the
 * recognised approximations). Kept light/medium so the dark theme foreground
 * stays readable. Written straight into the bg_color theme setting. */
static const int trimpod_bg_presets[] = {
    0xD8DCD0, /* Default (classic mono LCD) */
    0xC4C7CC, /* Silver                     */
    0x637D8C, /* Slate                      */
    0x0094E1, /* Blue                       */
    0xA0CB3B, /* Green                      */
    0xFFD400, /* Yellow                     */
    0xFAB71F, /* Gold                       */
    0xF08A1C, /* Orange                     */
    0xEC5298, /* Pink                       */
    0x9B72B0, /* Purple                     */
    0xBFCBB0, /* Mono Green                 */
};
#define TRIMPOD_BG_NPRESETS \
    ((int)(sizeof(trimpod_bg_presets)/sizeof(trimpod_bg_presets[0])))

static void trimpod_bg_apply(int idx)
{
    if (idx >= 0 && idx < TRIMPOD_BG_NPRESETS)
    {
        unsigned old_bg = global_settings.bg_color;
        global_settings.bg_color = trimpod_bg_presets[idx];
        screens[SCREEN_MAIN].set_background(global_settings.bg_color);
        /* Recolour the loaded skins' cached default background in place, then
         * force the UI viewports to re-fetch it and fully redraw.  A full skin
         * reload would apply the same change but stop playback. */
        skin_update_bg_color(old_bg, global_settings.bg_color);
        viewportmanager_theme_changed(THEME_UI_VIEWPORT | THEME_STATUSBAR |
                                      THEME_LISTS);
    }
}

static const char *const trimpod_bg_names[TRIMPOD_BG_NPRESETS] = {
    "Default", "Silver", "Slate", "Blue", "Green", "Yellow", "Gold", "Orange",
    "Pink", "Purple", "Mono Green",
};

/* Colour: cycle the iPod bg-colour presets; applies live app-wide + saves. */
static void trimpod_color_changed(int idx, void *ctx)
{
    (void)ctx;
    trimpod_bg_apply(idx);
    settings_save();               /* self-persisting: no page-close hook needed */
}

/* CPU Frequency: inline selector over the A133's cpufreq steps; applied live,
 * persisted to cpu_freq.txt on close (launch.sh re-applies next boot). */
extern void retrohh_cpu_set_freq(int khz);
extern void retrohh_cpu_set_dynamic(void);
extern int  retrohh_cpu_get_freq(void);
extern bool retrohh_cpu_is_dynamic(void);
extern void retrohh_cpu_save_choice(int khz);   /* persistence lives in power-target.c */

static const int trimpod_cpu_freqs[] = {
    408000, 600000, 816000, 1008000, 1200000, 1416000, 1608000, 1800000, 2000000
};
/* Display labels (MHz) paired 1:1 with trimpod_cpu_freqs (kHz). */
static const char *const trimpod_cpu_labels[] = {
    "408 MHz", "600 MHz", "816 MHz", "1008 MHz", "1200 MHz",
    "1416 MHz", "1608 MHz", "1800 MHz", "2000 MHz"
};
#define TRIMPOD_CPU_NFREQS \
    ((int)(sizeof(trimpod_cpu_freqs)/sizeof(trimpod_cpu_freqs[0])))

static int trimpod_cpu_nearest_index(int khz)
{
    int idx = TRIMPOD_CPU_NFREQS - 1;
    for (int i = 0; i < TRIMPOD_CPU_NFREQS; i++)
        if (trimpod_cpu_freqs[i] >= khz) { idx = i; break; }
    return idx;
}

static void trimpod_cpu_changed(int khz, void *ctx)
{
    (void)ctx;
    if (khz <= 0)
        retrohh_cpu_set_dynamic();       /* "Dynamic": tuned interactive, full range */
    else
        retrohh_cpu_set_freq(khz);       /* pin to a fixed step, live */
    retrohh_cpu_save_choice(khz);        /* persists (khz<=0 -> "dynamic") */
}

/* ---- do_menu inline value-row callbacks (struct menu_value_cb) ----------- *
 * get() returns the current label (static strings, so no copy), cycle() steps
 * the value and applies + persists it live.  Stateless -- the current index is
 * derived from the live freq / bg_color each call. */
static const char *tp_cpu_value_get(void *ctx, char *buf, int len)
{
    (void)ctx; (void)buf; (void)len;
    if (retrohh_cpu_is_dynamic())
        return "Dynamic";
    return trimpod_cpu_labels[trimpod_cpu_nearest_index(retrohh_cpu_get_freq())];
}
static void tp_cpu_value_cycle(void *ctx, int dir)
{
    (void)ctx;
    /* Virtual list: index 0 = "Dynamic" (first), 1..N = the fixed steps. */
    int vidx = retrohh_cpu_is_dynamic()
                 ? 0
                 : trimpod_cpu_nearest_index(retrohh_cpu_get_freq()) + 1;
    vidx += (dir < 0 ? -1 : 1);
    if (vidx < 0) vidx = 0;
    if (vidx > TRIMPOD_CPU_NFREQS) vidx = TRIMPOD_CPU_NFREQS;
    trimpod_cpu_changed(vidx == 0 ? 0 : trimpod_cpu_freqs[vidx - 1], NULL);
}
static const struct menu_value_cb trimpod_cpu_value =
    { tp_cpu_value_get, tp_cpu_value_cycle, NULL };

static int tp_color_cur_index(void)
{
    for (int i = 0; i < TRIMPOD_BG_NPRESETS; i++)
        if (trimpod_bg_presets[i] == global_settings.bg_color)
            return i;
    return 0;
}
static const char *tp_color_value_get(void *ctx, char *buf, int len)
{
    (void)ctx; (void)buf; (void)len;
    return trimpod_bg_names[tp_color_cur_index()];
}
static void tp_color_value_cycle(void *ctx, int dir)
{
    (void)ctx;
    int idx = tp_color_cur_index() + (dir < 0 ? -1 : 1);
    if (idx < 0) idx = 0;
    if (idx >= TRIMPOD_BG_NPRESETS) idx = TRIMPOD_BG_NPRESETS - 1;
    trimpod_color_changed(idx, NULL);
}
static const struct menu_value_cb trimpod_color_value =
    { tp_color_value_get, tp_color_value_cycle, NULL };

/* The page: CPU Frequency, Brightness, Colour, Auto Screen Off, Idle Power Off.
 * Setting rows label from their setting lang_id. */
MENUITEM_VALUE(tp_pw_cpu, ID2P(LANG_TRIMPOD_CPU), &trimpod_cpu_value, Icon_NOICON);
MENUITEM_SETTING(tp_pw_brightness, &global_settings.brightness, NULL);
MENUITEM_VALUE(tp_pw_colour, ID2P(LANG_TRIMPOD_COLOR), &trimpod_color_value, Icon_NOICON);
MENUITEM_SETTING(tp_pw_screenoff, &global_settings.backlight_timeout, NULL);
MENUITEM_SETTING(tp_pw_idlepoweroff, &global_settings.poweroff, NULL);
MAKE_MENU(trimpod_power_menu, ID2P(LANG_TRIMPOD_DEVICE), NULL, Icon_Submenu_Entered,
          &tp_pw_cpu, &tp_pw_brightness, &tp_pw_colour, &tp_pw_screenoff,
          &tp_pw_idlepoweroff);

int trimpod_power_page(void)
{
    do_menu(&trimpod_power_menu, NULL, NULL, false);
    return 0;
}
