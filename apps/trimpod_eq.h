/***************************************************************************
 * Trimpod: equalizer response curve for the Now Playing screen.
 ****************************************************************************/
#ifndef _TRIMPOD_EQ_H
#define _TRIMPOD_EQ_H

struct screen;
struct viewport;

/* Draw the EQ curve -- each band's gain across a log-frequency x-axis, 0 dB at
 * the viewport centre -- into vp, overlaid on whatever is already there (it does
 * NOT clear).  No-op when the equalizer is disabled.  Drawn from the same %pm
 * hook as the spectrum, so the curve sits over the spectrum bars. */
void trimpod_eq_draw(struct screen *display, struct viewport *vp);

#endif /* _TRIMPOD_EQ_H */
