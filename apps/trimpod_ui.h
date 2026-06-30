/* Trimpod: small reusable UI helpers shared across Trimpod screens. */
#ifndef _TRIMPOD_UI_H
#define _TRIMPOD_UI_H

#include <stdbool.h>

/* ---- Header button-legend (the standard) -----------------------------------
 * A "page" declares the button legend it wants shown in the status-row header
 * (e.g. "B Cancel   A OK").  The SBS skin reads it through the %Tl token and
 * overlays it on the header; pages never draw the header themselves.  Set to
 * NULL to clear.  The string must outlive the period it is set -- pass string
 * literals. */
void        trimpod_set_header_legend(const char *legend);
const char *trimpod_get_header_legend(void);

/* Re-render the status row so a just-set legend appears/clears.  Pages that run
 * their own draw loop need this; the run loop (trimpod_page_run) calls it. */
void        trimpod_header_refresh(void);

/* Draw `detail` (optional, NULL to omit) centered above `question` inside the
 * content viewport -- the themed background and skin-owned header are left
 * intact.  The shared renderer behind trimpod_confirm/_message/_message2; a page
 * with its own draw loop (e.g. the HOLD screen) can call it directly. */
void trimpod_centered_message(const char *question, const char *detail);

/* Centered yes/no confirmation drawn ONLY inside the content viewport (the
 * skin-owned header is left intact).  The button legend (B Cancel / A OK) is
 * shown in the header via the legend standard above, not in the body.
 *   question : the prompt, e.g. "Remove this folder?"
 *   detail   : optional line drawn just above the question (NULL to omit) --
 *              used to show the subject, e.g. the full folder path.
 *   A = OK -> true, B = Cancel -> false.  Blocking. */
bool trimpod_confirm(const char *question, const char *detail);

/* The About screen: a scrollable credits list (up/down scroll, B leaves).
 * Blocking. */
void trimpod_about(void);

/* Pre-fault the About reel's glyphs into the font cache at startup, so the first
 * (possibly mid-playback) About open doesn't stall the codec on disk reads. */
void trimpod_about_prewarm(void);

/* The shared on/off toggle glyph ("[x]"/"[ ]") for list indicator callbacks.
 * One convention for every toggle list (Menu Settings, Visualizers, ...). */
const char *trimpod_toggle_str(bool on);

#endif /* _TRIMPOD_UI_H */
