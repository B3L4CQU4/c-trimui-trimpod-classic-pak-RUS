/***************************************************************************
 * Trimpod: equalizer response curve drawn on the Now Playing screen.
 *
 * A polyline through each EQ band's gain at its cutoff frequency (log x-axis,
 * 0 dB at the viewport centre), overlaid on the spectrum.  It reflects the live
 * EQ settings; editing stays in Rockbox's graphical EQ editor.  Drawn from the
 * %pm hook in apps/gui/skin_engine/skin_display.c, right after the spectrum, so
 * it does NOT clear the viewport.
 ****************************************************************************/
#include "config.h"
#include "trimpod_eq.h"
#include <stdbool.h>
#include <math.h>
#include "screen_access.h"
#include "lcd.h"
#include "viewport.h"
#include "settings.h"           /* global_settings + struct eq_band_setting / EQ_NUM_BANDS */

#define TRIMPOD_EQ_MAX_GAIN 240 /* dB x10 (the editor's +/-24.0 dB range) */

void trimpod_eq_draw(struct screen *display, struct viewport *vp)
{
    if (!global_settings.eq_enabled)
        return;

    const struct eq_band_setting *b = global_settings.eq_band_settings;
    int W = vp->width, H = vp->height;
    if (W < 4 || H < 4)
        return;

    /* log-frequency span from the band cutoffs */
    float fmin = 1.0e9f, fmax = 1.0f;
    for (int i = 0; i < EQ_NUM_BANDS; i++)
        if (b[i].cutoff > 0)
        {
            float f = (float)b[i].cutoff;
            if (f < fmin) fmin = f;
            if (f > fmax) fmax = f;
        }
    if (fmax <= fmin)
        return;
    float lmin = log10f(fmin), lspan = log10f(fmax) - lmin;
    if (lspan <= 0.0f)
        return;

    int cy  = H / 2;            /* 0 dB baseline */
    int amp = H / 2 - 1;        /* +/- full-scale pixel amplitude */

    display->set_drawmode(DRMODE_SOLID);
    display->hline(0, W - 1, cy);              /* 0 dB reference line */

    int px = -1, py = -1;
    for (int i = 0; i < EQ_NUM_BANDS; i++)
    {
        if (b[i].cutoff <= 0)
            continue;
        int x = (int)((log10f((float)b[i].cutoff) - lmin) / lspan * (W - 1) + 0.5f);
        int g = b[i].gain;
        if (g >  TRIMPOD_EQ_MAX_GAIN) g =  TRIMPOD_EQ_MAX_GAIN;
        if (g < -TRIMPOD_EQ_MAX_GAIN) g = -TRIMPOD_EQ_MAX_GAIN;
        int y = cy - (g * amp) / TRIMPOD_EQ_MAX_GAIN;
        if (y < 0) y = 0;
        if (y > H - 1) y = H - 1;
        if (px >= 0)
            display->drawline(px, py, x, y);
        display->fillrect(x - 1, y - 1, 3, 3);  /* node marker */
        px = x;
        py = y;
    }
}
