/***************************************************************************
 * Trimpod: a minimal on-screen keyboard, iPod aesthetic, ChicagoFLF.
 *
 * A fixed grid -- numbers, A-Z, hyphen, then a row of wide special keys
 * (capslock / space / del / enter).  Capslock swaps the letter case in place.
 * Monochrome 1st-gen look: white field, black keys outlined, the highlighted
 * key inverted (black fill, white glyph).
 *
 * A trimpod_page like every other Trimpod screen: trimpod_page_run owns the
 * loop, the enter/exit slide transitions and the key whitelist; this page just
 * draws the full-screen grid and reacts to actions.
 *
 * Button map: D-pad moves the highlight, A engages the highlighted key
 * (letter/number/hyphen/space/caps/del/enter), Y backspaces, B always cancels.
 * Returns 0 when enter is engaged, -1 on cancel.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "action.h"
#include "screens.h"       /* screens[], SCREEN_MAIN */
#include "lcd.h"           /* DRMODE_*, lcd_set_drawmode */
#include "font.h"          /* font_load / font_unload, FONT_UI */
#include "gui/viewport.h"
#include "rbpaths.h"       /* FONT_DIR */
#include "settings.h"      /* global_settings.fg_color / bg_color (theme colours) */
#include "trimpod_page.h"
#include "trimpod_keyboard.h"

/* ChicagoFLF size that fits a 5-row grid + field on the 320x240 logical panel.
 * Loaded explicitly so the keyboard is ChicagoFLF regardless of the UI font. */
#define KBD_FONT_PATH   FONT_DIR "/20-ChicagoFLF.fnt"

/* Character rows (lowercase base; capslock upcases letters at draw/insert).
 * Row 4 is the special keys, handled separately. */
static const char *const kbd_rows[4] =
{
    "1234567890",
    "abcdefghij",
    "klmnopqrst",
    "uvwxyz-",
};
#define KBD_NROWS       5            /* 4 char rows + 1 special row */
#define KBD_SPECIAL_ROW 4
#define KBD_NSPECIAL    4            /* caps, space, del, enter */

enum special { SP_CAPS = 0, SP_SPACE, SP_DEL, SP_ENTER };

static int row_count(int row)
{
    return (row == KBD_SPECIAL_ROW) ? KBD_NSPECIAL : (int)strlen(kbd_rows[row]);
}

static char upcase(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* The label shown on a key; for a letter it reflects the current caps state. */
static const char *key_label(int row, int col, bool caps, char *buf)
{
    if (row == KBD_SPECIAL_ROW)
    {
        switch (col)
        {
            case SP_CAPS:  return caps ? "CAPS" : "caps";
            case SP_SPACE: return "space";
            case SP_DEL:   return "del";
            default:       return "enter";
        }
    }
    char c = kbd_rows[row][col];
    buf[0] = caps ? upcase(c) : c;
    buf[1] = '\0';
    return buf;
}

/* Keep the cursor roughly in the same horizontal position when changing rows. */
static int remap_col(int col, int from_count, int to_count)
{
    if (from_count <= 1)
        return 0;
    int nc = (col * (to_count - 1) + (from_count - 1) / 2) / (from_count - 1);
    if (nc < 0) nc = 0;
    if (nc > to_count - 1) nc = to_count - 1;
    return nc;
}

/* The slice of `text` whose pixel width fits in `maxw` (drop from the front so
 * the end -- where typing happens -- stays visible). */
static const char *visible_tail(struct screen *s, const char *text, int maxw)
{
    const char *p = text;
    int w, h;
    s->getstringsize((const unsigned char *)p, &w, &h);
    while (*p && w > maxw)
    {
        p++;
        s->getstringsize((const unsigned char *)p, &w, &h);
    }
    return p;
}

static void draw(struct screen *s, struct viewport *vp,
                 const char *text, int sel_row, int sel_col, bool caps)
{
    const int W = vp->width;
    const int margin = 6, gap = 3;
    const unsigned fg = global_settings.fg_color;   /* theme text colour */
    const unsigned bg = global_settings.bg_color;   /* theme LCD ("backlight") colour */

    s->set_viewport(vp);
    s->set_background(bg);
    s->clear_viewport();

    /* ---- input field ------------------------------------------------- */
    const int fx = margin, fy = 6, fw = W - 2 * margin, fh = 28;
    s->set_foreground(fg);
    s->drawrect(fx, fy, fw, fh);

    int tw, th;
    const char *vis = visible_tail(s, text, fw - 12);
    s->getstringsize((const unsigned char *)vis, &tw, &th);
    lcd_set_drawmode(DRMODE_FG);
    s->putsxy(fx + 6, fy + (fh - th) / 2, (const unsigned char *)vis);
    /* caret just after the text */
    s->fillrect(fx + 6 + tw + 1, fy + 4, 2, fh - 8);

    /* ---- key grid ---------------------------------------------------- */
    const int grid_top = fy + fh + 8;
    const int key_h = 34, row_step = key_h + gap;
    const int key_w = (W - 2 * margin - 9 * gap) / 10;          /* 10-col rows */
    const int sp_w  = (W - 2 * margin - (KBD_NSPECIAL - 1) * gap) / KBD_NSPECIAL;

    for (int r = 0; r < KBD_NROWS; r++)
    {
        const int n = row_count(r);
        const int kw = (r == KBD_SPECIAL_ROW) ? sp_w : key_w;
        const int row_w = n * kw + (n - 1) * gap;
        const int start_x = (W - row_w) / 2;
        const int y = grid_top + r * row_step;

        for (int c = 0; c < n; c++)
        {
            const int x = start_x + c * (kw + gap);
            const bool selected = (r == sel_row && c == sel_col);
            char cbuf[2];
            const char *label = key_label(r, c, caps, cbuf);

            /* cell: selected -> solid black; otherwise white with a border */
            lcd_set_drawmode(DRMODE_SOLID);
            if (selected)
            {
                s->set_foreground(fg);
                s->fillrect(x, y, kw, key_h);
            }
            else
            {
                s->set_foreground(fg);
                s->drawrect(x, y, kw, key_h);
            }

            int lw, lh;
            s->getstringsize((const unsigned char *)label, &lw, &lh);
            s->set_foreground(selected ? bg : fg);
            lcd_set_drawmode(DRMODE_FG);
            s->putsxy(x + (kw - lw) / 2, y + (key_h - lh) / 2,
                      (const unsigned char *)label);
        }
    }

    lcd_set_drawmode(DRMODE_SOLID);
    s->update_viewport();
}

static void append_char(char *buffer, int buflen, char c)
{
    int len = strlen(buffer);
    if (len < buflen - 1)
    {
        buffer[len] = c;
        buffer[len + 1] = '\0';
    }
}

static void backspace(char *buffer)
{
    int len = strlen(buffer);
    if (len > 0)
        buffer[len - 1] = '\0';
}

/* ---- the keyboard as a trimpod_page ----------------------------------- */

struct kbd_page
{
    struct trimpod_page base;
    struct screen *s;
    char  *buffer;
    int    buflen;
    int    row, col;
    bool   caps;
    int    fid;
    int    result;       /* 0 = accepted (enter), -1 = cancelled */
};

static void kbd_page_draw(struct trimpod_page *self)
{
    struct kbd_page *p = (struct kbd_page *)self;
    struct viewport vp = {0};  /* zero-init: viewport_set_fullscreen bus-errors otherwise */
    viewport_set_fullscreen(&vp, SCREEN_MAIN);
    p->s->setfont(p->fid);
    draw(p->s, &vp, p->buffer, p->row, p->col, p->caps);
    p->s->set_viewport(NULL);
}

static int kbd_page_poll(struct trimpod_page *self, int timeout)
{
    (void)self;
    return get_action(CONTEXT_KEYBOARD, timeout);
}

static enum trimpod_page_result kbd_page_on_action(struct trimpod_page *self,
                                                   int action)
{
    struct kbd_page *p = (struct kbd_page *)self;

    switch (action)
    {
        case ACTION_KBD_LEFT:
            p->col = (p->col - 1 + row_count(p->row)) % row_count(p->row);
            break;
        case ACTION_KBD_RIGHT:
            p->col = (p->col + 1) % row_count(p->row);
            break;
        case ACTION_KBD_UP:
        {
            int from = row_count(p->row);
            p->row = (p->row - 1 + KBD_NROWS) % KBD_NROWS;
            p->col = remap_col(p->col, from, row_count(p->row));
            break;
        }
        case ACTION_KBD_DOWN:
        {
            int from = row_count(p->row);
            p->row = (p->row + 1) % KBD_NROWS;
            p->col = remap_col(p->col, from, row_count(p->row));
            break;
        }

        case ACTION_KBD_SELECT:            /* A: engage the highlighted key */
            if (p->row == KBD_SPECIAL_ROW)
            {
                switch (p->col)
                {
                    case SP_CAPS:  p->caps = !p->caps; break;
                    case SP_SPACE: append_char(p->buffer, p->buflen, ' '); break;
                    case SP_DEL:   backspace(p->buffer); break;
                    case SP_ENTER: p->result = 0; return TRIMPOD_PAGE_DONE;
                }
            }
            else
            {
                char c = kbd_rows[p->row][p->col];
                append_char(p->buffer, p->buflen, p->caps ? upcase(c) : c);
            }
            break;

        case ACTION_KBD_BACKSPACE:         /* Y: backspace */
            backspace(p->buffer);
            break;
        case ACTION_KBD_ABORT:             /* B: always cancel / back */
            p->result = -1;
            return TRIMPOD_PAGE_DONE;
    }
    return TRIMPOD_PAGE_STAY;
}

static const struct trimpod_page_vtable kbd_vtable =
{
    .legend    = NULL,                 /* full-screen page owns the whole screen */
    .draw      = kbd_page_draw,
    .poll      = kbd_page_poll,
    .on_action = kbd_page_on_action,
};

/* only A/Y/B + D-pad act; the run loop swallows everything else */
static const int kbd_allowed[] =
{
    ACTION_KBD_LEFT, ACTION_KBD_RIGHT, ACTION_KBD_UP, ACTION_KBD_DOWN,
    ACTION_KBD_SELECT, ACTION_KBD_BACKSPACE, ACTION_KBD_ABORT, -1
};

int trimpod_kbd_input(char *buffer, int buflen)
{
    struct screen *s = &screens[SCREEN_MAIN];
    int fid = font_load(KBD_FONT_PATH);
    if (fid < 0)
        fid = FONT_UI;                 /* the UI font is ChicagoFLF too */

    struct kbd_page p =
    {
        .base = { .vt = &kbd_vtable, .context = CONTEXT_KEYBOARD,
                  .allowed = kbd_allowed,
                  .no_theme = true,          /* own the whole screen, no status bar */
                  .no_header_refresh = true,
                  .animated = true },        /* repaints the grid/cursor each tick */
        .s = s, .buffer = buffer, .buflen = buflen,
        .row = 1, .col = 0, .caps = false, .fid = fid, .result = -1,
    };

    trimpod_page_run(&p.base);

    s->setfont(FONT_UI);
    if (fid != FONT_UI)
        font_unload(fid);
    return p.result;
}
