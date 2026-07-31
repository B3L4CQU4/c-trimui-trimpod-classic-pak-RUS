/***************************************************************************
 * Trimpod: page slide transitions (see trimpod_transition.h).
 *
 * Two full-screen frame snapshots ('from' = the screen we leave, 'to' = the
 * screen we arrive at) are composited into the live framebuffer at sliding
 * offsets over TRIMPOD_TRANSITION_MS with an ease-out curve.  The destination
 * is rendered off-screen first (lcd updates suppressed) so it never flashes.
 *
 * The SBS status row (header, above the content viewport) and the now-playing
 * row (footer, below it) are PINNED: only the content band between them slides.
 * Both bands are painted once with the DESTINATION frame before the slide and
 * never touched again, so they stay put while the pages slide.  The boundaries
 * are the content viewport's top and bottom y, constant for the single SBS skin.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "system.h"
#include "kernel.h"
#include "lcd.h"
#include "gui/viewport.h"
#include "window-sdl.h"   /* trimpod_viz_active: projectM owns the GL window */
#include "trimpod_transition.h"

/* full-screen frame snapshots (off the stack: ~225KB each at 320x240x3) */
static fb_data s_from[LCD_FBWIDTH * LCD_FBHEIGHT];
static fb_data s_to[LCD_FBWIDTH * LCD_FBHEIGHT];
static bool    s_back_pending;
static bool    s_suppress_next;   /* skip the next screen-entry slide entirely */

/* base of the on-screen framebuffer (force the default fullscreen viewport so
 * FBADDR(0,0) is the real top-left, not some sub-viewport's origin) */
static fb_data *fb_base(void)
{
    lcd_set_viewport(NULL);
    return FBADDR(0, 0);
}

/* Height of the pinned header band (= the content viewport's top y).  The SBS
 * status row occupies rows [0, header_h); pages draw below it.  Recomputed each
 * transition (cheap) so a theme/status-bar change can't leave a stale height. */
static int header_h(void)
{
    struct viewport vp = {0};
    viewport_set_defaults(&vp, SCREEN_MAIN);
    return (vp.y > 0 && vp.y < LCD_HEIGHT) ? vp.y : 0;
}

/* Top of the pinned footer band (= the content viewport's bottom y).  The SBS
 * now-playing row sits in [footer_y0, LCD_HEIGHT); pages draw above it. */
static int footer_y0(void)
{
    struct viewport vp = {0};
    viewport_set_defaults(&vp, SCREEN_MAIN);
    int y1 = vp.y + vp.height;
    return (y1 > 0 && y1 < LCD_HEIGHT) ? y1 : LCD_HEIGHT;
}

/* copy the header band [0, h) of `src` into the framebuffer, unshifted */
static void blit_header(fb_data *fb, const fb_data *src, int h)
{
    for (int y = 0; y < h; y++)
        memcpy(fb + y*LCD_FBWIDTH, src + y*LCD_FBWIDTH,
               (size_t)LCD_WIDTH * sizeof(fb_data));
}

/* copy the footer band [y0, LCD_HEIGHT) of `src` into the framebuffer, unshifted */
static void blit_footer(fb_data *fb, const fb_data *src, int y0)
{
    for (int y = y0; y < LCD_HEIGHT; y++)
        memcpy(fb + y*LCD_FBWIDTH, src + y*LCD_FBWIDTH,
               (size_t)LCD_WIDTH * sizeof(fb_data));
}

/* copy a frame into the framebuffer shifted by xoff pixels, for the content
 * band [y0, y1) only -- the pinned header above y0 and footer below y1 are
 * left untouched */
static void blit_shifted(fb_data *fb, const fb_data *src, int xoff,
                         int y0, int y1)
{
    int aoff = xoff < 0 ? -xoff : xoff;
    int wpx  = LCD_WIDTH - aoff;
    if (wpx <= 0)
        return;
    int sx = xoff < 0 ? aoff : 0;
    int dx = xoff > 0 ? xoff : 0;
    for (int y = y0; y < y1; y++)
        memcpy(fb + y*LCD_FBWIDTH + dx, src + y*LCD_FBWIDTH + sx,
               (size_t)wpx * sizeof(fb_data));
}

/* slide from s_from to s_to (already captured) in `dir` */
static void slide(enum trimpod_transition_dir dir)
{
    fb_data *fb = fb_base();

    /* No CPU boost: the slide runs at the user's configured clock; the fast
     * single-blit present (lcd-sdl.c) keeps the tween smooth without pinning. */

    /* Paced by the vsync'd GL swap inside lcd_update() -- NO sleep.  Position
     * comes from a real ms clock so the tween spans exactly
     * TRIMPOD_TRANSITION_MS. */
    int d = (dir == TRIMPOD_TRANS_FORWARD) ? 1 : -1;
    int hh = header_h();                  /* pinned header band: rows [0, hh)   */
    int fy = footer_y0();                 /* pinned footer band: rows [fy, end) */
    unsigned long start = sdl_get_ms();

    for (;;)
    {
        unsigned long el = sdl_get_ms() - start;
        if (el >= TRIMPOD_TRANSITION_MS)
            break;
        int t   = (int)(el * 1000 / TRIMPOD_TRANSITION_MS);  /* 0..1000 linear */
        int inv = 1000 - t;
        int p   = 1000 - inv * inv / 1000;                   /* ease-out quad */
        int W   = LCD_WIDTH;
        int off_from = -d * p * W / 1000;            /* old slides off  */
        int off_to   =  d * (1000 - p) * W / 1000;   /* new slides in   */
        /* only the content band slides; header and footer stay pinned */
        blit_shifted(fb, s_from, off_from, hh, fy);
        blit_shifted(fb, s_to,   off_to,   hh, fy);
        lcd_update();
    }

    /* settle exactly on the destination (header already matches) */
    memcpy(fb, s_to, FRAMEBUFFER_SIZE);
    lcd_update();

}

void trimpod_transition_animate(enum trimpod_transition_dir dir,
                                trimpod_render_fn render, void *ctx)
{
    s_back_pending = false;                 /* consumed by this animate */

    /* A screen aborted before it ever rendered (e.g. an empty facet that only
     * showed a splash over the current menu) asked us not to slide.  Render the
     * destination and present it with NO motion.  The lcd_update is essential:
     * it commits the frame and clears the splash -- a bare draw with no present
     * leaves the splash up and the menu loop spinning on it (a hang). */
    if (s_suppress_next)
    {
        s_suppress_next = false;
        if (render)
            render(ctx);
        if (!trimpod_viz_active)
            lcd_update();
        return;
    }

    /* The visualizer owns the GL context and suppresses LCD presents, so a
     * slide would be invisible and just waste cycles.  Render the destination
     * (so it's ready when the visualizer releases the screen) and skip the
     * animation. */
    if (trimpod_viz_active)
    {
        if (render)
            render(ctx);
        return;
    }

    /* snapshot the screen we leave, then render the destination off-screen */
    memcpy(s_from, fb_base(), FRAMEBUFFER_SIZE);
    lcd_set_update_suppressed(true);
    if (render)
        render(ctx);
    /* grab the off-screen destination, put 'from' back, then overlay the
     * destination header band so the pinned header swaps to the new text
     * instantly (the content below it slides during slide()). */
    memcpy(s_to, fb_base(), FRAMEBUFFER_SIZE);
    memcpy(fb_base(), s_from, FRAMEBUFFER_SIZE);
    blit_header(fb_base(), s_to, header_h());
    blit_footer(fb_base(), s_to, footer_y0());
    lcd_set_update_suppressed(false);
    lcd_update();

    slide(dir);
}

bool trimpod_transition_first_screen(void)
{
    static bool first = true;     /* the first screen at app launch */
    bool f = first;
    first = false;
    return f;
}

void trimpod_transition_arm_back(void)
{
    s_back_pending = true;
}

bool trimpod_transition_take_back(void)
{
    bool b = s_back_pending;
    s_back_pending = false;
    return b;
}
void trimpod_transition_suppress_next(void)
{
    s_suppress_next = true;
}
