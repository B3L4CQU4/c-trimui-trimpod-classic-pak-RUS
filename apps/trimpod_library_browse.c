/* Trimpod: tag-based Music Library browse -- see trimpod_library_browse.h.
 *
 * ONE persistent trimpod_page over the SQLite index, navigating three levels
 * in place (like the folder browser, not nested blocking screens):
 *   Artists  -> Albums(artist)  ["All Songs" + one row per album]
 *            -> Tracks(album)    [one row per track]
 * A descends a level (or, in Tracks, starts the album at that track and slides
 * to Now Playing); B ascends a level (or leaves at Artists).  On play we stash
 * the navigation position; returning from Now Playing restores the Tracks list
 * highlighting the currently playing song -- so backing out of the WPS lands on
 * the album's track listing, then tracks out album -> artist.  (Rescan Library
 * lives in Settings -> Library.)
 *
 * The enumerators (trimpod_library_artists/_albums/_tracks) hand each row to a
 * callback; we collect them into a growable string list (no fixed cap) that
 * backs a gui_synclist name callback, mirroring the folder picker's listing. */

#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>   /* realloc/free */
#include <string.h>   /* strdup */

#include "kernel.h"    /* HZ (splash timeouts) */
#include "lang.h"
#include "settings.h"
#include "splash.h"
#include "action.h"
#include "gui/list.h"
#include "icon.h"
#include "playlist.h"
#include "misc.h"      /* warn_on_pl_erase, push/pop_current_activity */
#include "pathfuncs.h" /* path_basename (untagged-track fallback) */
#include "root_menu.h" /* GO_TO_WPS / GO_TO_ROOT */
#include "trimpod_page.h"
#include "trimpod_transition.h"
#include "trimpod_folders.h"        /* TP_CAT_MUSIC */
#include "trimpod_library.h"
#include "trimpod_library_browse.h"

/* ---- growable string list (rows collected from the enumerators) ------- */

struct slist { char **v; int n, cap; };

static void slist_add(struct slist *s, const char *str)
{
    if (s->n == s->cap)
    {
        int ncap = s->cap ? s->cap * 2 : 32;
        char **nv = realloc(s->v, ncap * sizeof *nv);
        if (!nv) return;                 /* best effort: drop this row, keep prior */
        s->v = nv; s->cap = ncap;
    }
    char *dup = strdup(str ? str : "");
    if (dup) s->v[s->n++] = dup;
}
static void slist_free(struct slist *s)
{
    for (int i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v); s->v = NULL; s->n = s->cap = 0;
}

static const char *disp_artist(const char *a)
{
    return (a && a[0]) ? a : (const char *)str(LANG_TRIMPOD_UNKNOWN_ARTIST);
}

/* ---- enumerator callbacks (append one row per DB row) ----------------- */

static void artist_cb(const char *browse_artist, int track_count, void *ctx)
{
    (void)track_count;
    slist_add((struct slist *)ctx, browse_artist);   /* "" -> Unknown at display */
}
static void album_cb(const char *album, int year, void *ctx)
{
    (void)year;
    if (album && album[0])               /* skip untagged; reachable via All Songs */
        slist_add((struct slist *)ctx, album);
}
static void track_cb(const char *title, const char *path, void *ctx)
{
    if (title && title[0])               /* tagged: show the title */
    {
        slist_add((struct slist *)ctx, title);
        return;
    }
    const char *leaf = "";               /* untagged: fall back to the filename */
    if (path && path[0])
        path_basename(path, &leaf);
    slist_add((struct slist *)ctx, leaf);
}

/* ---- the page: Artists / Albums / Tracks, one gui_synclist ------------ */

enum lib_level { LVL_ARTISTS = 0, LVL_ALBUMS, LVL_TRACKS, LVL_COUNT };

struct lib_page
{
    struct trimpod_page base;
    struct gui_synclist lists;
    enum lib_level level;
    char  *artist;               /* browse_artist filter (may be ""), owned */
    char  *album;                /* album filter (owned); "" when all_songs   */
    bool   all_songs;            /* Tracks shows the whole artist (album wildcard) */
    struct slist rows;           /* display strings for the current level */
    int    sel_at[LVL_COUNT];    /* per-level highlight, restored on ascent */
    int    result;               /* GO_TO_WPS once a track played, else GO_TO_ROOT */
};

/* Stashed position for the Now Playing round-trip: set when a track plays,
 * consumed on the next entry so B out of the WPS reopens the Tracks list. */
static struct {
    bool  pending;
    char *artist;
    char *album;                 /* NULL == All Songs */
    int   sel_at[LVL_COUNT];
} lib_resume;

static void set_str(char **dst, const char *s)
{
    free(*dst);
    *dst = strdup(s ? s : "");
}

/* row 0 at Albums is the synthetic "All Songs"; every other row indexes rows.v */
static int lib_nb_items(const struct lib_page *p)
{
    return p->level == LVL_ALBUMS ? p->rows.n + 1 : p->rows.n;
}
static const char *lib_get_name(int sel, void *data, char *buf, size_t len)
{
    (void)buf; (void)len;
    struct lib_page *p = data;
    if (p->level == LVL_ALBUMS)
    {
        if (sel == 0)
            return (const char *)str(LANG_TRIMPOD_ALL_SONGS);
        sel--;
    }
    if (sel < 0 || sel >= p->rows.n)
        return "";
    return p->level == LVL_ARTISTS ? disp_artist(p->rows.v[sel]) : p->rows.v[sel];
}

/* (Re)fill rows for the current level, set the title, and restore this level's
 * highlight.  Called on entry and inside every slide's off-screen render pass. */
static void lib_load(struct lib_page *p)
{
    slist_free(&p->rows);
    const char *title;
    switch (p->level)
    {
        case LVL_ALBUMS:
            trimpod_library_albums(TP_CAT_MUSIC, p->artist, album_cb, &p->rows);
            title = disp_artist(p->artist);
            break;
        case LVL_TRACKS:
            trimpod_library_tracks(TP_CAT_MUSIC, p->artist,
                                   p->all_songs ? NULL : p->album, track_cb, &p->rows);
            title = p->all_songs ? (const char *)str(LANG_TRIMPOD_ALL_SONGS) : p->album;
            break;
        case LVL_ARTISTS:
        default:
            trimpod_library_artists(TP_CAT_MUSIC, artist_cb, &p->rows);
            title = (const char *)str(LANG_TRIMPOD_ARTISTS);
            break;
    }
    gui_synclist_set_title(&p->lists, (char *)title, Icon_Audio);
    int n = lib_nb_items(p);
    gui_synclist_set_nb_items(&p->lists, n);
    int sel = p->sel_at[p->level];
    if (sel < 0 || sel >= n) sel = 0;
    gui_synclist_select_item(&p->lists, sel);
}

/* Slide-in plumbing: reload + draw the new level inside the tween's off-screen
 * render pass (as tree.c/pick_slide do), so it slides over the old content. */
static struct lib_page *s_slide_p;
static void lib_slide_render(void *ctx)
{
    (void)ctx;
    lib_load(s_slide_p);
    gui_synclist_draw(&s_slide_p->lists);
}
static void lib_slide(struct lib_page *p, enum trimpod_transition_dir dir)
{
    s_slide_p = p;
    trimpod_transition_animate(dir, lib_slide_render, NULL);
}

static void lib_draw(struct trimpod_page *self)
{
    gui_synclist_draw(&((struct lib_page *)self)->lists);
}
static int lib_poll(struct trimpod_page *self, int timeout)
{
    int action;
    list_do_action(self->context, timeout, &((struct lib_page *)self)->lists, &action);
    return action;
}

/* Build the current playlist for the shown album/all-songs and start it at row
 * `sel`; the row order matches the query, so `sel` is the playlist index. */
static bool play_track(struct lib_page *p, int sel)
{
    if (!warn_on_pl_erase())
        return false;
    if (trimpod_library_build_playlist(TP_CAT_MUSIC, p->artist,
                                       p->all_songs ? NULL : p->album) <= 0)
    {
        splash(HZ, ID2P(LANG_TRIMPOD_NO_MUSIC));
        return false;
    }
    playlist_start(sel, 0, 0);
    /* Stash the navigation position for the return from Now Playing. */
    free(lib_resume.artist);
    free(lib_resume.album);
    lib_resume.artist  = strdup(p->artist ? p->artist : "");
    lib_resume.album   = p->all_songs ? NULL : strdup(p->album ? p->album : "");
    memcpy(lib_resume.sel_at, p->sel_at, sizeof lib_resume.sel_at);
    lib_resume.sel_at[LVL_TRACKS] = sel;
    lib_resume.pending = true;
    return true;
}

static enum trimpod_page_result lib_on_action(struct trimpod_page *self, int action)
{
    struct lib_page *p = (struct lib_page *)self;
    int sel = gui_synclist_get_sel_pos(&p->lists);

    if (action == ACTION_STD_CANCEL)                /* B: up a level, or leave */
    {
        if (p->level == LVL_ARTISTS)                /* back to the root menu */
        {
            trimpod_transition_arm_back();
            return TRIMPOD_PAGE_DONE;
        }
        p->level--;
        lib_slide(p, TRIMPOD_TRANS_BACK);
        return TRIMPOD_PAGE_STAY;
    }

    if (action == ACTION_STD_OK)                    /* A: descend, or play */
    {
        switch (p->level)
        {
            case LVL_ARTISTS:
                if (sel < 0 || sel >= p->rows.n)
                    break;
                p->sel_at[LVL_ARTISTS] = sel;
                set_str(&p->artist, p->rows.v[sel]);
                p->level = LVL_ALBUMS;
                p->sel_at[LVL_ALBUMS] = 0;
                lib_slide(p, TRIMPOD_TRANS_FORWARD);
                break;

            case LVL_ALBUMS:
            {
                bool all = (sel == 0);              /* row 0 == "All Songs" */
                int ai = sel - 1;
                if (!all && (ai < 0 || ai >= p->rows.n))
                    break;
                p->sel_at[LVL_ALBUMS] = sel;
                p->all_songs = all;
                set_str(&p->album, all ? "" : p->rows.v[ai]);
                p->level = LVL_TRACKS;
                p->sel_at[LVL_TRACKS] = 0;
                lib_slide(p, TRIMPOD_TRANS_FORWARD);
                break;
            }

            case LVL_TRACKS:
                if (sel >= 0 && sel < p->rows.n && play_track(p, sel))
                {
                    p->result = GO_TO_WPS;          /* no arm_back: WPS slides in */
                    return TRIMPOD_PAGE_DONE;
                }
                break;

            default:
                break;
        }
    }
    return TRIMPOD_PAGE_STAY;
}

static const struct trimpod_page_vtable lib_vtable =
{
    .legend = NULL, .draw = lib_draw,               /* no button legend */
    .poll = lib_poll, .on_action = lib_on_action,
};

/* only A (descend/play) and B (up/leave) act; list scrolling is consumed in poll */
static const int lib_allowed[] = { ACTION_STD_OK, ACTION_STD_CANCEL, -1 };

/* trimpod_library_browse -- the root-menu "Artists" entry.  A re-entrant,
 * root-dispatched page: fresh opens start at the Artist list; returning from Now
 * Playing restores the Tracks list on the currently playing song. */
int trimpod_library_browse(void *param)
{
    (void)param;
    /* Reflect source-folder edits made since launch (stat-gated -> a blink when
     * nothing changed); the init reconcile already ran at startup. */
    trimpod_library_reconcile(false);

    struct lib_page p =
    {
        /* re-entrant page: honor a pending back on enter (return from Now
         * Playing) and don't auto-arm a back on exit -- the next screen slides
         * itself in.  Matches folder_browse. */
        .base = { .vt = &lib_vtable, .context = CONTEXT_LIST,
                  .allowed = lib_allowed, .no_arm_back = true,
                  .enter_honor_back = true },
        .level = LVL_ARTISTS,
        .result = GO_TO_ROOT,
    };

    if (lib_resume.pending)                          /* returning from Now Playing */
    {
        lib_resume.pending = false;
        p.level = LVL_TRACKS;
        set_str(&p.artist, lib_resume.artist);
        p.all_songs = (lib_resume.album == NULL);
        set_str(&p.album, lib_resume.album ? lib_resume.album : "");
        memcpy(p.sel_at, lib_resume.sel_at, sizeof p.sel_at);
        if (playlist_amount() > 0)                   /* land on the playing song */
        {
            int idx = playlist_get_display_index() - 1;
            if (idx >= 0)
                p.sel_at[LVL_TRACKS] = idx;
        }
    }

    gui_synclist_init(&p.lists, lib_get_name, &p, false, 1, NULL);
    lib_load(&p);

    /* A restored Tracks list that came up empty (library changed since play), or
     * an empty library on a fresh open, both fall back to the Artist list. */
    if (p.level == LVL_TRACKS && p.rows.n == 0)
    {
        p.level = LVL_ARTISTS;
        lib_load(&p);
    }
    if (p.level == LVL_ARTISTS && p.rows.n == 0)
    {
        splash(HZ, ID2P(LANG_TRIMPOD_NO_MUSIC));
        slist_free(&p.rows);
        free(p.artist);
        free(p.album);
        return GO_TO_ROOT;
    }

    push_current_activity(ACTIVITY_FILEBROWSER);
    trimpod_page_run(&p.base);
    pop_current_activity();

    slist_free(&p.rows);
    free(p.artist);
    free(p.album);
    return p.result;
}
