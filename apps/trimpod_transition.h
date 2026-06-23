/***************************************************************************
 * Trimpod: page slide transitions.
 *
 * A 0.15s ease-out horizontal slide between two full-screen frames.  Forward
 * navigation (one page opening another) slides right->left; going back slides
 * left->right.  ONE entry point -- trimpod_transition_animate() -- used by both
 * the page base (trimpod_page_run) and the Rockbox menu loop (do_menu); no
 * caller duplicates the tween logic.
 ****************************************************************************/
#ifndef _TRIMPOD_TRANSITION_H
#define _TRIMPOD_TRANSITION_H

#include <stdbool.h>

/* the one reusable duration constant */
#define TRIMPOD_TRANSITION_MS  100   /* 0.1 seconds, ease-out */

enum trimpod_transition_dir
{
    TRIMPOD_TRANS_FORWARD,   /* destination enters from the right (R->L) */
    TRIMPOD_TRANS_BACK,      /* destination enters from the left  (L->R) */
};

/* Called to paint the destination screen off-screen during a transition. */
typedef void (*trimpod_render_fn)(void *ctx);

/* Animate a screen change: snapshot the current screen, render the destination
 * off-screen via render(ctx), then slide it in `dir` (ease-out).  This is the
 * single source of the tween. */
void trimpod_transition_animate(enum trimpod_transition_dir dir,
                                trimpod_render_fn render, void *ctx);

/* Cross-call back signal: a page/menu about to return flags that its caller
 * should play a BACK slide on its next redraw.  take() returns and clears it. */
void trimpod_transition_arm_back(void);
bool trimpod_transition_take_back(void);

/* True exactly once -- for the very FIRST screen shown at app launch, which must
 * render with no slide (there is no prior frame to slide from).  Whichever
 * screen comes up first (root menu, or Now Playing via auto-resume) consumes it;
 * every screen after that slides. */
bool trimpod_transition_first_screen(void);

#endif /* _TRIMPOD_TRANSITION_H */
