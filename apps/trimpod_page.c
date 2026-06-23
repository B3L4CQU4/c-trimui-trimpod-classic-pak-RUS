/***************************************************************************
 * Trimpod: the page run loop (see trimpod_page.h).
 *
 * One driver for every Trimpod page.  It owns the things every page shares so
 * no page re-implements them:
 *   - publishes the page's button-legend to the header (and restores the
 *     previous one on exit, so nested pages stack cleanly),
 *   - refreshes the skin header so the legend actually appears,
 *   - draws the page content,
 *   - enforces the page's key whitelist (non-allowed buttons do nothing),
 *   - lets system events (USB, poweroff) through to default_event_handler.
 ****************************************************************************/
#include "config.h"
#include "kernel.h"
#include "system.h"
#include "action.h"
#include "misc.h"
#include "screens.h"          /* FOR_NB_SCREENS */
#include "gui/viewport.h"     /* viewportmanager_theme_enable / _undo */
#include "trimpod_ui.h"
#include "trimpod_page.h"
#include "trimpod_transition.h"

static bool action_allowed(const int *allowed, int action)
{
    if (!allowed)
        return true;                 /* no whitelist: everything is allowed */
    for (int i = 0; allowed[i] != -1; i++)
        if (allowed[i] == action)
            return true;
    return false;
}

/* draw the page: its header legend + content (no on-screen present is forced
 * here; the caller suppresses updates around this for transitions) */
static void page_render(struct trimpod_page *page)
{
    trimpod_set_header_legend(page->vt->legend ? page->vt->legend(page) : NULL);
    if (!page->no_header_refresh)
        trimpod_header_refresh();
    if (page->vt->draw)
        page->vt->draw(page);
}

/* render callback for the (shared) transition animator */
static void page_render_cb(void *ctx)
{
    page_render((struct trimpod_page *)ctx);
}

void trimpod_page_run(struct trimpod_page *page)
{
    const char *prev = trimpod_get_header_legend();

    /* swallow the keypress that opened this page */
    action_wait_for_release();

    /* A full-screen page disables the theme (SBS status bar) for its lifetime:
     * the bar is then neither drawn nor periodically repainted over the page,
     * and the slide treats the whole screen as content (header_h()==0) so the
     * enter/exit transitions clear it fully.  Restored before we return, so the
     * parent's back-slide pins its status bar again. */
    if (page->no_theme)
        FOR_NB_SCREENS(i)
            viewportmanager_theme_enable(i, false, NULL);

    /* Slide this page in.  A nested page always enters forward; a re-entrant
     * page (the root menu, driven by an external dispatch loop) honors a
     * pending back so returning from a child slides it back in.  The first
     * screen at app launch renders with no slide. */
    if (page->no_enter_anim)
    {
        trimpod_transition_take_back();   /* consume any stale pending back */
        page_render(page);
    }
    else
    {
        enum trimpod_transition_dir enter_dir =
            (page->enter_honor_back && trimpod_transition_take_back())
                ? TRIMPOD_TRANS_BACK : TRIMPOD_TRANS_FORWARD;
        trimpod_transition_animate(enter_dir, page_render_cb, page);
    }

    bool done = false, shutting_down = false;
    while (!done)
    {
        int action = page->vt->poll ? page->vt->poll(page, HZ)
                                    : get_action(page->context, HZ);

        /* key whitelist: swallow non-allowed buttons; SYS events still pass */
        if (!(action & SYS_EVENT) && !action_allowed(page->allowed, action))
            action = ACTION_NONE;

        if (page->vt->on_action &&
            page->vt->on_action(page, action) == TRIMPOD_PAGE_DONE)
            done = true;
        else
        {
            int handled = default_event_handler(action);
            if (handled == SYS_USB_CONNECTED)
                done = true;
            else if (handled == SYS_POWEROFF || handled == SYS_REBOOT)
                shutting_down = true;   /* exit is deferred; stop drawing */
        }

        if (done)
            break;

        /* Once shutdown has begun, never redraw: clean_shutdown drew "Shutting
         * Down" and the real exit is deferred (an SDL event), so a page redraw
         * would clobber it.  Keep looping so get_action keeps pumping events
         * until the exit fires.  (do_menu likewise only redraws on changes.) */
        if (shutting_down)
            continue;

        if (trimpod_transition_take_back())
            /* a nested page just exited: slide it out, us back in (L->R) */
            trimpod_transition_animate(TRIMPOD_TRANS_BACK, page_render_cb, page);
        else if (page->animated)   /* static pages redraw on change, not per tick */
            page_render(page);
    }

    /* re-enable the theme a full-screen page disabled, before the parent's
     * back-slide runs (it needs its status bar pinned again). */
    if (page->no_theme)
        FOR_NB_SCREENS(i)
            viewportmanager_theme_undo(i, false);

    /* restore the header the parent page had, then (unless told not to) arm a
     * back slide so the screen we return to slides us away and itself back in.
     * A re-entrant page suppresses this: it isn't returning to a parent -- the
     * next screen the dispatch loads will slide itself in. */
    trimpod_set_header_legend(prev);
    if (!page->no_arm_back)
        trimpod_transition_arm_back();
}
