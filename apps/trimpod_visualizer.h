/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Trimpod: Milkdrop-style music visualizer (projectM), started from the Now
 * Playing menu or auto-started after idle while music plays (any screen).
 * See apps/trimpod_visualizer.c.
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
#ifndef TRIMPOD_VISUALIZER_H
#define TRIMPOD_VISUALIZER_H

#include <stdbool.h>

/* Run the fullscreen Milkdrop visualizer (auto-cycling presets) until the user
 * presses Back (B). Blocks the calling (UI) thread; audio keeps playing. */
void trimpod_visualizer_run(void);

/* Fade the current screen to black (visualizer entry), then arm a fade-in so the
 * loaded visualizer fades up from black -- hides the load.  Returns true if a
 * keypress cancelled the fade (caller should NOT launch the visualizer). */
bool trimpod_visualizer_fade_to_black(void);

/* True when the idle auto-start timer has expired: a delay is configured, the
 * display is on, and music is actively playing (not paused). */
bool trimpod_visualizer_autostart_due(void);

/* Idle auto-start for a UI loop's timeout path: when due, fade to black and
 * run the visualizer.  Returns true if the screen needs a repaint (the fade
 * ran, cancelled or not); false = not due.  The WPS runs the sequence inline
 * instead -- it must restore the skin around the run. */
bool trimpod_visualizer_maybe_autostart(void);

/* Settings -> Visualizers: a flat on/off toggle list of the shipped presets.
 * A toggles the highlighted preset (only enabled presets are auto-cycled on Now
 * Playing); Y previews it fullscreen (B exits the preview); B leaves the list.
 * MENUITEM_FUNCTION callback (returns GO_TO_PREVIOUS). */
int trimpod_visualizer_menu(void);

#endif /* TRIMPOD_VISUALIZER_H */
