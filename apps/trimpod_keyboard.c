/***************************************************************************
 * Trimpod: a minimal on-screen keyboard, iPod aesthetic, ChicagoFLF.
 *
 * A fixed grid -- numbers, letters, hyphen, then a row of wide special keys
 * (RU/EN / capslock / space / del / enter). Capslock swaps letter case.
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
#include "lang.h"
#include "rbunicode.h"
#include "trimpod_page.h"
#include "trimpod_ui.h"
#include "trimpod_keyboard.h"

/* The keyboard follows the interface language: unchanged Chicago in English,
 * all-Mulmaru TrimpodRus (including digits/symbols) in Russian. */
#define KBD_FONT_EN   FONT_DIR "/20-ChicagoFLF.fnt"
#define KBD_FONT_RU   FONT_DIR "/20-TrimpodRus.fnt"

/* Unicode character rows. Russian uses eleven columns so all 33 letters,
 * digits and hyphen fit into the same four rows as the Latin layout. */
static const ucschar_t en_row_0[] = {'1','2','3','4','5','6','7','8','9','0','-'};
static const ucschar_t en_row_1[] = {'q','w','e','r','t','y','u','i','o','p'};
static const ucschar_t en_row_2[] = {'a','s','d','f','g','h','j','k','l'};
static const ucschar_t en_row_3[] = {'z','x','c','v','b','n','m'};
static const ucschar_t ru_row_0[] = {'1','2','3','4','5','6','7','8','9','0','-'};
static const ucschar_t ru_row_1[] = {0x0439,0x0446,0x0443,0x043a,0x0435,0x043d,
                                     0x0433,0x0448,0x0449,0x0437,0x0445};
static const ucschar_t ru_row_2[] = {0x044a,0x0444,0x044b,0x0432,0x0430,0x043f,
                                     0x0440,0x043e,0x043b,0x0434,0x0436};
static const ucschar_t ru_row_3[] = {0x044d,0x044f,0x0447,0x0441,0x043c,0x0438,
                                     0x0442,0x044c,0x0431,0x044e,0x0451};

struct kbd_layout
{
    const ucschar_t *rows[4];
    unsigned char counts[4];
};

static const struct kbd_layout layouts[] =
{
    { {en_row_0, en_row_1, en_row_2, en_row_3}, {11, 10, 9, 7} },
    { {ru_row_0, ru_row_1, ru_row_2, ru_row_3}, {11, 11, 11, 11} },
};

#define KBD_NROWS       5            /* 4 char rows + 1 special row */
#define KBD_SPECIAL_ROW 4
#define KBD_NSPECIAL    5            /* language, caps, space, del, enter */

enum kbd_language { KBD_EN = 0, KBD_RU };
enum special { SP_LANG = 0, SP_CAPS, SP_SPACE, SP_DEL, SP_ENTER };

static int row_count(int row, int language)
{
    return (row == KBD_SPECIAL_ROW) ? KBD_NSPECIAL : layouts[language].counts[row];
}

static ucschar_t upcase(ucschar_t c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    if (c >= 0x0430 && c <= 0x044f)
        return c - 0x20;
    if (c == 0x0451)
        return 0x0401;
    return c;
}

/* The label shown on a key; for a letter it reflects the current caps state. */
static const char *key_label(int row, int col, bool caps, int language,
                             unsigned char *buf)
{
    if (row == KBD_SPECIAL_ROW)
    {
        switch (col)
        {
            case SP_LANG:  return language == KBD_RU ? "РУС" : "EN";
            case SP_CAPS:  return str(LANG_TRIMPOD_KBD_CAPS);
            case SP_SPACE: return str(LANG_TRIMPOD_KBD_SPACE);
            case SP_DEL:   return str(LANG_TRIMPOD_KBD_DELETE);
            default:       return str(LANG_TRIMPOD_KBD_ENTER);
        }
    }
    ucschar_t c = layouts[language].rows[row][col];
    unsigned char *end = utf8encode(caps ? upcase(c) : c, buf);
    *end = '\0';
    return (const char *)buf;
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
        ucschar_t ignored;
        p = (const char *)utf8decode((const unsigned char *)p, &ignored);
        s->getstringsize((const unsigned char *)p, &w, &h);
    }
    return p;
}

static void draw(struct screen *s, struct viewport *vp,
                 const char *text, int sel_row, int sel_col, bool caps,
                 int language)
{
    const int W = vp->width;
    const int margin = 12, grid_margin = 28, gap = 4;
    const unsigned fg = global_settings.fg_color;   /* theme text colour */
    const unsigned bg = global_settings.bg_color;   /* theme LCD ("backlight") colour */

    s->set_viewport(vp);
    s->set_background(bg);
    s->clear_viewport();

    /* ---- input field ------------------------------------------------- */
    const int fx = margin, fy = 8, fw = W - 2 * margin, fh = 38;
    s->set_foreground(fg);
    s->drawrect(fx, fy, fw, fh);

    int tw, th;
    const char *vis = visible_tail(s, text, fw - 16);
    s->getstringsize((const unsigned char *)vis, &tw, &th);
    lcd_set_drawmode(DRMODE_FG);
    s->putsxy(fx + 8, fy + (fh - th) / 2, (const unsigned char *)vis);
    /* caret just after the text */
    s->fillrect(fx + 8 + tw + 1, fy + 5, 2, fh - 10);

    /* ---- key grid ---------------------------------------------------- */
    const int grid_top = fy + fh + 16;
    const int key_h = 48, row_step = key_h + gap;
    const int key_w = (W - 2 * grid_margin - 10 * gap) / 11;    /* 11-col rows */
    const int sp_w  = (W - 2 * grid_margin - (KBD_NSPECIAL - 1) * gap) /
                      KBD_NSPECIAL;

    for (int r = 0; r < KBD_NROWS; r++)
    {
        const int n = row_count(r, language);
        const int kw = (r == KBD_SPECIAL_ROW) ? sp_w : key_w;
        const int row_w = n * kw + (n - 1) * gap;
        const int start_x = (W - row_w) / 2;
        const int y = grid_top + r * row_step;

        for (int c = 0; c < n; c++)
        {
            const int x = start_x + c * (kw + gap);
            const bool selected = (r == sel_row && c == sel_col);
            unsigned char cbuf[5];
            const char *label = key_label(r, c, caps, language, cbuf);

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

static void append_char(char *buffer, int buflen, ucschar_t c)
{
    int len = strlen(buffer);
    unsigned char encoded[5];
    unsigned char *end = utf8encode(c, encoded);
    int bytes = end - encoded;
    if (len + bytes < buflen)
    {
        memcpy(buffer + len, encoded, bytes);
        buffer[len + bytes] = '\0';
    }
}

static void backspace(char *buffer)
{
    int len = strlen(buffer);
    if (len > 0)
    {
        int chars = utf8length((const unsigned char *)buffer);
        buffer[utf8seek((const unsigned char *)buffer, chars - 1)] = '\0';
    }
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
    int    language;
    int    fid;
    int    result;       /* 0 = accepted (enter), -1 = cancelled */
};

static void kbd_page_draw(struct trimpod_page *self)
{
    struct kbd_page *p = (struct kbd_page *)self;
    struct viewport vp = {0};  /* zero-init: viewport_set_fullscreen bus-errors otherwise */
    viewport_set_fullscreen(&vp, SCREEN_MAIN);
    p->s->setfont(p->fid);
    draw(p->s, &vp, p->buffer, p->row, p->col, p->caps, p->language);
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
            p->col = (p->col - 1 + row_count(p->row, p->language)) %
                     row_count(p->row, p->language);
            break;
        case ACTION_KBD_RIGHT:
            p->col = (p->col + 1) % row_count(p->row, p->language);
            break;
        case ACTION_KBD_UP:
        {
            int from = row_count(p->row, p->language);
            p->row = (p->row - 1 + KBD_NROWS) % KBD_NROWS;
            p->col = remap_col(p->col, from, row_count(p->row, p->language));
            break;
        }
        case ACTION_KBD_DOWN:
        {
            int from = row_count(p->row, p->language);
            p->row = (p->row + 1) % KBD_NROWS;
            p->col = remap_col(p->col, from, row_count(p->row, p->language));
            break;
        }

        case ACTION_KBD_SELECT:            /* A: engage the highlighted key */
            if (p->row == KBD_SPECIAL_ROW)
            {
                switch (p->col)
                {
                    case SP_LANG:  p->language = 1 - p->language; break;
                    case SP_CAPS:  p->caps = !p->caps; break;
                    case SP_SPACE: append_char(p->buffer, p->buflen, ' '); break;
                    case SP_DEL:   backspace(p->buffer); break;
                    case SP_ENTER: p->result = 0; return TRIMPOD_PAGE_DONE;
                }
            }
            else
            {
                ucschar_t c = layouts[p->language].rows[p->row][p->col];
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
    int fid = font_load(trimpod_russian_ui() ? KBD_FONT_RU : KBD_FONT_EN);
    if (fid < 0)
        fid = FONT_UI;

    struct kbd_page p =
    {
        .base = { .vt = &kbd_vtable, .context = CONTEXT_KEYBOARD,
                  .allowed = kbd_allowed,
                  .no_theme = true,          /* own the whole screen, no status bar */
                  .no_header_refresh = true,
                  .animated = true },        /* repaints the grid/cursor each tick */
        .s = s, .buffer = buffer, .buflen = buflen,
        .row = 1, .col = 0, .caps = false,
        .language = trimpod_russian_ui() ? KBD_RU : KBD_EN,
        .fid = fid, .result = -1,
    };

    trimpod_page_run(&p.base);

    s->setfont(FONT_UI);
    if (fid != FONT_UI)
        font_unload(fid);
    return p.result;
}
