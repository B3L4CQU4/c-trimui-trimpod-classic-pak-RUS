/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 by Jonathan Gordon
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

/* This file contains the code to draw the list widget on BITMAP LCDs. */

#include "config.h"
#include "system.h"
#include "lcd.h"
#include "font.h"
#include "button.h"
#include "string.h"
#include "settings.h"
#include "kernel.h"
#include "file.h"

#include "action.h"
#include "screen_access.h"
#include "list.h"
#include "scrollbar.h"
#include "lang.h"
#include "sound.h"
#include "misc.h"
#include "viewport.h"
#include "statusbar-skinned.h"
#include "debug.h"
#include "line.h"

#define ICON_PADDING 1
#define ICON_PADDING_S "1"


/* these are static to make scrolling work */
static struct viewport list_text[NB_SCREENS], title_text[NB_SCREENS];

/* list-private helpers from the generic list.c (move to header?) */
int gui_list_get_item_offset(struct gui_synclist * gui_list, int item_width,
                             int text_pos, struct screen * display,
                             struct viewport *vp);
bool list_display_title(struct gui_synclist *list, enum screen_type screen);
int list_get_nb_lines(struct gui_synclist *list, enum screen_type screen);

void gui_synclist_scroll_stop(struct gui_synclist *lists)
{
    FOR_NB_SCREENS(i)
    {
        screens[i].scroll_stop_viewport(&list_text[i]);
        screens[i].scroll_stop_viewport(&title_text[i]);
        screens[i].scroll_stop_viewport(lists->parent[i]);
    }
}

/* Draw the list...
    internal screen layout:
        -----------------
        |TI|  title     |   TI is title icon
        -----------------
        | | |            |
        |S|I|            |   S - scrollbar
        | | | items      |   I - icons
        | | |            |
        ------------------

        Note: This image is flipped horizontally when the language is a
        right-to-left one (Hebrew, Arabic)
*/


/* True for a bracketed toggle indicator like "[x]"/"[ ]". */
static bool is_bracket_toggle(const unsigned char *s)
{
    return s && s[0] == '[' && s[1] && s[2] == ']' && s[3] == '\0';
}

/* Width of an indicator. A toggle is sized with its middle as 'x' so "[x]" and
 * "[ ]" measure identically (no width jitter on toggle). */
static int list_indicator_width(struct screen *d, const unsigned char *s)
{
    int wl, wx, wr, h;
    if (is_bracket_toggle(s))
    {
        d->getstringsize((const unsigned char *)"[", &wl, &h);
        d->getstringsize((const unsigned char *)"x", &wx, &h);
        d->getstringsize((const unsigned char *)"]", &wr, &h);
        return wl + wx + wr;
    }
    d->getstringsize(s, &wl, &h);
    return wl;
}

static void _default_listdraw_fn(struct list_putlineinfo_t *list_info)
{
    struct screen *display = list_info->display; 
    int x = list_info->x;
    int y = list_info->y;
    int item_indent = list_info->item_indent;
    int item_offset = list_info->item_offset;
    bool is_selected = list_info->is_selected;
    bool is_title = list_info->is_title;
    bool show_cursor = list_info->show_cursor;
    struct line_desc *linedes = list_info->linedes;
    const char *dsp_text = list_info->dsp_text;
    struct viewport *vp = list_info->vp;

    /* Trimpod: no list icons.  Only the selection cursor (a cursor-style list,
     * not a show_icons icon) draws a glyph. */
    if (is_title)
        display->put_line(x, y, linedes, "$t", dsp_text);
    else if (show_cursor)
        display->put_line(x, y, linedes, "$*s$"ICON_PADDING_S"I$*t", item_indent,
                is_selected ? Icon_Cursor : Icon_NOICON, item_offset, dsp_text);
    else
        display->put_line(x, y, linedes, "$*s$*t", item_indent, item_offset, dsp_text);

    /* Trimpod: [x]/[ ] toggle or ">" arrow in the gutter list_draw reserved on
     * the right.  The name was drawn in the narrowed name-column viewport (so the
     * scroll marquee clips there); widen back over the gutter, extend the row's
     * bar across it (an empty put_line == style_line, so the highlight spans the
     * whole row), then draw the glyph on top.  The scroll engine only repaints the
     * name column, so the gutter bar+glyph persist under the marquee.  A toggle
     * draws its brackets at fixed positions (middle 'x'-wide) so [x]/[ ] don't
     * jitter. */
    if ((list_info->indicator || list_info->show_chevron) && !is_title)
    {
        const unsigned char *g = (const unsigned char *)
                    (list_info->indicator ? list_info->indicator : ">");
        int w, h;
        display->getstringsize(g, &w, &h);
        int lh = (linedes->height > 0) ? linedes->height : h;
        int yy = y + (lh - h) / 2;
        vp->width += list_info->indic_gutter;
        display->put_line(vp->width - list_info->indic_gutter, y, linedes, "");
        int x0 = vp->width - list_indicator_width(display, g) - 2;
        /* inverse-video on the inverse selection bar so the glyph stays visible */
        display->set_drawmode((linedes->style & STYLE_INVERT) ?
                              (DRMODE_SOLID | DRMODE_INVERSEVID) : DRMODE_FG);
        if (is_bracket_toggle(g))
        {
            int wl, wx, hh;
            display->getstringsize((const unsigned char *)"[", &wl, &hh);
            display->getstringsize((const unsigned char *)"x", &wx, &hh);
            char mid[2] = { (char)g[1], '\0' };
            display->putsxy(x0, yy, (const unsigned char *)"[");
            display->putsxy(x0 + wl, yy, (const unsigned char *)mid);
            display->putsxy(x0 + wl + wx, yy, (const unsigned char *)"]");
        }
        else
            display->putsxy(x0, yy, g);
        display->set_drawmode(DRMODE_SOLID);
        vp->width -= list_info->indic_gutter;
    }
}

static bool draw_title(struct screen *display,
                       struct gui_synclist *list,
                       list_draw_item *callback_draw_item)
{
    const int screen = display->screen_type;
    struct viewport *title_text_vp = &title_text[screen];
    struct line_desc linedes = LINE_DESC_DEFINIT;

    if (sb_set_title_text(list->title, list->title_icon, screen))
        return false; /* the sbs is handling the title */
    display->scroll_stop_viewport(title_text_vp);
    if (!list_display_title(list, screen))
        return false;
    *title_text_vp = *(list->parent[screen]);
    linedes.height = list->line_height[screen];
    title_text_vp->height = linedes.height;

#if LCD_DEPTH > 1
    /* XXX: Do we want to support the separator on remote displays? */
    if (display->screen_type == SCREEN_MAIN && global_settings.list_separator_height != 0)
        linedes.separator_height = abs(global_settings.list_separator_height)
                                + (lcd_get_dpi() > 200 ? 2 : 1);
#endif

#ifdef HAVE_LCD_COLOR
    if (list->title_color >= 0)
        linedes.style |= (STYLE_COLORED|list->title_color);
#endif
    linedes.scroll = true;

    display->set_viewport(title_text_vp);

    struct list_putlineinfo_t list_info =
    {
        .x = 0, .y = 0, .item_indent = 0, .item_offset = 0, .line = -1,
        .display = display, .vp = title_text_vp, .linedes = &linedes, .list = list,
        .dsp_text = list->title,
        .is_selected = false, .is_title = true, .show_cursor = false,
    };
    callback_draw_item(&list_info);

    return true;
}

void list_draw(struct screen *display, struct gui_synclist *list)
{
    int start, end, item_offset, i;
    const int screen = display->screen_type;
    list_draw_item *callback_draw_item;

    const int list_start_item = list->start_item[screen];
    const bool scrollbar_in_left = (list->scrollbar == SCROLLBAR_LEFT);
    const bool scrollbar_in_right = (list->scrollbar == SCROLLBAR_RIGHT);
    const bool show_cursor = (list->cursor_style == SYNCLIST_CURSOR_NOSTYLE);

    struct viewport *parent = (list->parent[screen]);
    struct line_desc linedes = LINE_DESC_DEFINIT;
    bool show_title;
    struct viewport *list_text_vp = &list_text[screen];
    int indent = 8; /* small left margin so entries aren't flush-left
                     * (offsets only the text; the selection bar stays full width) */

    if (list->callback_draw_item != NULL)
        callback_draw_item = list->callback_draw_item;
    else
        callback_draw_item = _default_listdraw_fn;

    struct viewport * last_vp = display->set_viewport(parent);
    display->clear_viewport();
    if (!list->scroll_all)
        display->scroll_stop_viewport(list_text_vp);
    *list_text_vp = *parent;
    if ((show_title = draw_title(display, list, callback_draw_item)))
    {
        int title_height = title_text[screen].height;
        list_text_vp->y += title_height;
        list_text_vp->height -= title_height;
    }

    /* Trimpod: reserve a right gutter for the [x]/[ ] toggle or ">" arrow so the
     * name column (and the scroll-engine marquee) stops before it. Persisting it
     * on list_text_vp keeps the marquee inside the name column. */
    int indic_gutter = 0;
    if (list->callback_get_item_indicator || list->callback_get_item_chevron)
    {
        const char *s = list->callback_get_item_indicator ?
            list->callback_get_item_indicator(list->selected_item, list->data) : ">";
        if (!s || !*s)
            s = ">";
        indic_gutter = list_indicator_width(display, (const unsigned char *)s) + 6;
        list_text_vp->width -= indic_gutter;
    }

    const int nb_lines = list_get_nb_lines(list, screen);

    linedes.height = list->line_height[screen];
    linedes.nlines = list->selected_size;
#if LCD_DEPTH > 1
    /* XXX: Do we want to support the separator on remote displays? */
    if (display->screen_type == SCREEN_MAIN)
        linedes.separator_height = abs(global_settings.list_separator_height);
#endif
    start = list_start_item;
    end = start + nb_lines;

    #define draw_offset 0

    /* draw the scrollbar if its needed */
    if (list->scrollbar != SCROLLBAR_OFF)
    {
        /* if the scrollbar is shown the text viewport needs to shrink */
        if (nb_lines < list->nb_items)
        {
            struct viewport vp = *list_text_vp;
            vp.width = SCROLLBAR_WIDTH;
            /* touchscreens must use full viewport height
             * due to pixelwise rendering */
            vp.height = linedes.height * nb_lines;
            list_text_vp->width -= SCROLLBAR_WIDTH;
            if (scrollbar_in_right)
                vp.x += list_text_vp->width;
            else /* left */
                list_text_vp->x += SCROLLBAR_WIDTH;
            struct viewport *last = display->set_viewport(&vp);

            /* button targets go itemwise */
            int scrollbar_items = list->nb_items;
            int scrollbar_min = list_start_item;
            int scrollbar_max = list_start_item + nb_lines;
            gui_scrollbar_draw(display,
                    (scrollbar_in_left? 0: 1), 0, SCROLLBAR_WIDTH-1, vp.height,
                    scrollbar_items, scrollbar_min, scrollbar_max, VERTICAL);
            display->set_viewport(last);
        }
        /* shift everything a bit in relation to the title */
        else if (!VP_IS_RTL(list_text_vp) && scrollbar_in_left)
            indent += SCROLLBAR_WIDTH;
        else if (VP_IS_RTL(list_text_vp) && scrollbar_in_right)
            indent += SCROLLBAR_WIDTH;
    }

    display->set_viewport(list_text_vp);
    int character_width = display->getcharwidth();

    struct list_putlineinfo_t list_info =
    {
        .x = 0, .y = 0, .vp = list_text_vp, .list = list,
        .is_title = false, .show_cursor = show_cursor,
        .linedes = &linedes, .display = display,
        .indic_gutter = indic_gutter
    };

    for (i=start; i<end && i<list->nb_items; i++)
    {
        /* do the text */
        unsigned const char *s;
        extern char simplelist_buffer[SIMPLELIST_MAX_LINES * SIMPLELIST_MAX_LINELENGTH];
        /*char entry_buffer[MAX_PATH]; use the buffer from gui/list.c instead */
        unsigned char *entry_name;
        int line = i - start;
        int line_indent = 0;
        int style = STYLE_DEFAULT;
        bool is_selected = false;
        s = list->callback_get_item_name(i, list->data, simplelist_buffer,
                                         sizeof(simplelist_buffer));
        if (P2ID((unsigned char *)s) > VOICEONLY_DELIMITER)
            entry_name = "";
        else
            entry_name = P2STR(s);

        while (*entry_name == '\t')
        {
            line_indent++;
            entry_name++;
        }
        if (line_indent)
            line_indent *= character_width;
        line_indent += indent;

        /* position the string at the correct offset place */
        int item_width,h;
        display->getstringsize(entry_name, &item_width, &h);
        item_offset = gui_list_get_item_offset(list, item_width, indent,
                display, list_text_vp);

        /* draw the selected line */
        if(
                i >= list->selected_item
                && i <  list->selected_item + list->selected_size)
        {/* The selected item must be displayed scrolling */
#ifdef HAVE_LCD_COLOR
            if (list->selection_color)
            {
                /* Display gradient line selector */
                style = STYLE_GRADIENT;
                linedes.text_color = list->selection_color->text_color;
                linedes.line_color = list->selection_color->line_color;
                linedes.line_end_color = list->selection_color->line_end_color;
            }
            else
#endif
            if (list->cursor_style == SYNCLIST_CURSOR_INVERT
            )
            {
                /* Display inverted-line-style */
                style = STYLE_INVERT;
            }
#ifdef HAVE_LCD_COLOR
            else if (list->cursor_style == SYNCLIST_CURSOR_COLOR)
            {
                /* Display colour line selector */
                style = STYLE_COLORBAR;
                linedes.text_color = global_settings.lst_color;
                linedes.line_color = global_settings.lss_color;
            }
            else if (list->cursor_style == SYNCLIST_CURSOR_GRADIENT)
            {
                /* Display gradient line selector */
                style = STYLE_GRADIENT;
                linedes.text_color = global_settings.lst_color;
                linedes.line_color = global_settings.lss_color;
                linedes.line_end_color = global_settings.lse_color;
            }
#endif
            is_selected = true;
        }
        
#ifdef HAVE_LCD_COLOR
        /* if the list has a color callback */
        if (list->callback_get_item_color)
        {
            int c = list->callback_get_item_color(i, list->data);
            if (c >= 0)
            {   /* if color selected */
                linedes.text_color = c;
                style |= STYLE_COLORED;
            }
        }
#endif
        linedes.style = style;
        linedes.scroll = is_selected ? true : list->scroll_all;
        linedes.line = i % list->selected_size;

        list_info.y = line * linedes.height + draw_offset;
        list_info.is_selected = is_selected;
        list_info.item_indent = line_indent;
        list_info.line = i;
        list_info.dsp_text = entry_name;
        list_info.item_offset = item_offset;
        list_info.show_chevron = list->callback_get_item_chevron ?
                    list->callback_get_item_chevron(i, list->data) : false;
        list_info.indicator = list->callback_get_item_indicator ?
                    list->callback_get_item_indicator(i, list->data) : NULL;

        callback_draw_item(&list_info);
    }
    display->set_viewport(last_vp);
}

