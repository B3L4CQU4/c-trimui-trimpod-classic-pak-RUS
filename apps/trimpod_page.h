/***************************************************************************
 * Trimpod: the page base.
 *
 * A "page" is a screen with a header button-legend, content drawn inside the
 * content viewport, and an action handler.  trimpod_page_run() owns the common
 * loop -- it publishes the page's legend to the skin-owned header, draws the
 * content, polls for input, enforces the page's key whitelist, routes system
 * events, and exits when the page says so.
 *
 * Concrete pages "inherit" by embedding `struct trimpod_page` as their FIRST
 * member and supplying a vtable; the run loop is shared, never re-written.
 ****************************************************************************/
#ifndef _TRIMPOD_PAGE_H
#define _TRIMPOD_PAGE_H

#include <stdbool.h>

enum trimpod_page_result
{
    TRIMPOD_PAGE_STAY = 0,   /* keep the page open */
    TRIMPOD_PAGE_DONE,       /* close the page */
};

struct trimpod_page;

struct trimpod_page_vtable
{
    /* Header button-legend (or NULL for none); re-read every frame. */
    const char *(*legend)(struct trimpod_page *self);
    /* Draw the content viewport.  The header is the skin's, never drawn here. */
    void (*draw)(struct trimpod_page *self);
    /* Fetch the next action (timeout in ticks).  NULL -> get_action(context). */
    int (*poll)(struct trimpod_page *self, int timeout);
    /* Handle one action -> STAY (keep running) or DONE (close). */
    enum trimpod_page_result (*on_action)(struct trimpod_page *self, int action);
};

struct trimpod_page
{
    const struct trimpod_page_vtable *vt;
    int        context;   /* get_action() context for the default poll */
    const int *allowed;   /* NULL = all keys; else a (-1)-terminated whitelist of
                           * allowed actions (others are swallowed) */

    /* Transition behaviour.  The defaults (both false) suit a nested page that
     * is opened by a parent and returns to it: slide in forward, arm a back
     * slide on exit.  A re-entrant page driven by an external dispatch loop
     * (the root menu) wants the opposite handling -- see the fields below. */
    bool enter_honor_back;  /* on enter, slide BACK if a back is pending (like
                             * do_menu) instead of always sliding forward */
    bool no_arm_back;       /* on exit, do NOT arm a back slide (the page is not
                             * "returning to a parent"; the next screen slides
                             * itself in) */
    bool no_enter_anim;     /* on enter, render with no slide (e.g. the very
                             * first screen at app launch) */
    bool no_header_refresh; /* skip the per-frame status-bar refresh in the run
                             * loop -- for animated pages that redraw at a high
                             * rate (e.g. Now Playing) and refresh the header
                             * themselves only when its content changes.  The
                             * bar persists in the framebuffer between frames. */
    bool no_theme;          /* full-screen page: disable the theme (SBS status
                             * bar) for the page's lifetime via
                             * viewportmanager_theme_enable(false).  Stops the
                             * status bar being drawn or periodically repainted
                             * (GUI_EVENT_ACTIONREDRAW) over the page, and makes
                             * the slide treat the whole screen as content
                             * (header_h()==0) so enter/exit clear fully.  Pair
                             * with no_header_refresh (the run loop must not draw
                             * the bar either). */
    bool animated;          /* repaint every loop tick (Now Playing, keyboard).
                             * Static pages leave false: a per-frame list redraw
                             * runs scroll_stop each frame and kills the marquee.
                             * Static pages redraw on enter/change/child-return. */
};

/* Run the page until its on_action returns DONE (or USB is connected). */
void trimpod_page_run(struct trimpod_page *page);

#endif /* _TRIMPOD_PAGE_H */
