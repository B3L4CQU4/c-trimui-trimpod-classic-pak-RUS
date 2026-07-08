/***************************************************************************
 * Trimpod: the Playlists screen.
 *
 * A trimpod_page that lists the playlists in the catalog directory plus a
 * "+ Add Playlist" row, mirroring the Music Folders settings page:
 *   A on "+ Add Playlist" -> the Rockbox keyboard (catalog_pick_new_playlist_name);
 *     a confirmed name creates an empty .m3u8 and the list refreshes.
 *   A on a playlist row    -> a Play/Shuffle/Rename/Edit/Delete action chooser;
 *     Play/Shuffle start playback at once (file list sits behind Now Playing).
 *   B                      -> leave, back to the main menu.
 *
 * NOTE: this first cut exists mainly to surface the stock Rockbox keyboard from
 * an "Add Playlist" action so its suitability can be evaluated.  The run loop,
 * header and key whitelist are owned by trimpod_page_run; this page only
 * describes the list and what A/B do.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>   /* qsort */
#include <string.h>
#include "string-extra.h"
#include "strnatcmp.h"   /* natural alphabetical sort (2 before 10) */
#include "file.h"
#include "dir.h"
#include "pathfuncs.h"
#include "settings.h"
#include "lang.h"
#include "splash.h"
#include "action.h"
#include "gui/list.h"
#include "menu.h"          /* do_menu, MENUITEM_STRINGLIST (Play/Edit/Delete) */
#include "icon.h"
#include "misc.h"        /* push/pop_current_activity, ACTIVITY_PLAYLISTBROWSER */
#include "root_menu.h"
#include "playlist_catalog.h"
#include "playlist.h"          /* playlist_load/get_track_info/delete/save (Edit) */
#include "kernel.h"            /* current_tick (shuffle seed) */
#include "scratch_buf.h"       /* scratch_buffer_get -- playlist_load buffers */
#include "trimpod_page.h"
#include "trimpod_ui.h"        /* trimpod_confirm */
#include "trimpod_keyboard.h"  /* the on-screen keyboard */
#include "trimpod_playlists.h"

#define PL_MAX        256
#define PL_NAMEBUF    (PL_MAX * 64)   /* packed NUL-terminated names */

/* one playlists page at a time -> the listing lives in file-scope statics */
static char pl_dir[MAX_PATH];         /* the catalog directory */
static char pl_namebuf[PL_NAMEBUF];   /* packed NUL-terminated playlist names */
static int  pl_off[PL_MAX];           /* offset of each name in pl_namebuf */
static int  pl_count;

/* Remembered selection for the standalone screen, so re-entering it (incl.
 * returning from Now Playing) re-highlights the playlist you were on.  Empty
 * string = the "+ Add Playlist" row.  Session-only. */
static char pl_last_sel[MAX_PATH];
static bool pl_have_last;

/* A pending "Play Playlist" / "Shuffle Playlist" request handed to the root
 * dispatch: the action menu records the pick and returns GO_TO_PLAYLIST_VIEWER,
 * then playlist_view() calls trimpod_playlists_start_pending() to begin playback
 * so the file list sits *behind* Now Playing (backing out of the WPS reveals
 * it).  Filename only -- the catalog dir is the stable pl_dir. */
static char pl_pending_file[MAX_PATH];
static bool pl_pending_active;
static bool pl_pending_shuffle;

static bool is_playlist(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".m3u") || !strcasecmp(dot, ".m3u8"));
}

/* qsort comparator: natural alphabetical order of the names at two offsets */
static int pl_off_cmp(const void *a, const void *b)
{
    return strnatcasecmp(pl_namebuf + *(const int *)a,
                         pl_namebuf + *(const int *)b);
}

/* (Re)read the catalog directory into the packed name table, sorted. */
static void pl_load(void)
{
    pl_count = 0;
    int used = 0;
    catalog_get_directory(pl_dir, sizeof(pl_dir));   /* also creates it if absent */
    DIR *d = opendir(pl_dir);
    if (d)
    {
        struct dirent *e;
        while ((e = readdir(d)))
        {
            if (e->d_name[0] == '.' || !is_playlist(e->d_name))
                continue;
            int len = strlen(e->d_name) + 1;
            if (pl_count >= PL_MAX || used + len > (int)sizeof(pl_namebuf))
                break;                               /* listing full (rare) */
            pl_off[pl_count++] = used;
            memcpy(pl_namebuf + used, e->d_name, len);
            used += len;
        }
        closedir(d);
    }
    qsort(pl_off, pl_count, sizeof pl_off[0], pl_off_cmp);
}

struct playlists_page
{
    struct trimpod_page base;
    struct gui_synclist lists;
    int result;                       /* GO_TO_* handed back to the root dispatch */

    /* "pick" mode (from a folder browser's Y = Add to Playlist): the chosen or
     * newly created playlist receives `pick_sel` via the stock Rockbox
     * catalog_insert_into() -- a file is appended, a directory is expanded into
     * its tracks.  Normal mode (pick=false) is the standalone Playlists screen. */
    bool        pick;
    const char *pick_sel;
    int         pick_attr;
};

/* Copy playlist row `sel`'s filename with its .m3u8/.m3u extension stripped --
 * the display name, reused for the list rows and the delete confirmation. */
static void pl_display_name(int sel, char *out, size_t out_len)
{
    strlcpy(out, pl_namebuf + pl_off[sel], out_len);
    char *dot = strrchr(out, '.');
    if (dot && (!strcasecmp(dot, ".m3u8") || !strcasecmp(dot, ".m3u")))
        *dot = '\0';
}

/* rows: [0, pl_count) playlists, then the "+ Add Playlist" row at pl_count */
static const char *pl_get_name(int sel, void *data, char *buf, size_t buf_len)
{
    (void)data;
    if (sel == pl_count)
        return "+ Add Playlist";
    if (sel < 0 || sel > pl_count)
        return "";
    pl_display_name(sel, buf, buf_len);   /* no extension shown */
    return buf;
}

static void playlists_draw(struct trimpod_page *self)
{
    gui_synclist_draw(&((struct playlists_page *)self)->lists);
}

static int playlists_poll(struct trimpod_page *self, int timeout)
{
    int action;
    list_do_action(self->context, timeout,
                   &((struct playlists_page *)self)->lists, &action);
    return action;
}

/* Name a new playlist with the on-screen keyboard and create an empty .m3u8.
 * The keyboard yields a bare name (its glyph set can't produce path separators),
 * so the file is just <catalog>/<name>.m3u8.  Cancel/empty leaves the list as-is. */
static void add_playlist(struct playlists_page *p)
{
    char name[MAX_PATH];
    name[0] = '\0';
    if (trimpod_kbd_input(name, sizeof(name)) != 0 || name[0] == '\0')
        return;

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s.m3u8", pl_dir, name);

    if (file_exists(path) && !trimpod_confirm("Overwrite playlist?", name))
        return;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
        close(fd);
    else
        splashf(HZ, "Could not create playlist");
    pl_load();
    gui_synclist_set_nb_items(&p->lists, pl_count + 1);
}

/* ---- Edit Playlist: track list, Y removes the highlighted track --------- *
 * Taps the stock playlist API (playlist_load / playlist_delete / playlist_save)
 * on the separate on-disk playlist_info, so it never disturbs playback -- the
 * same path the playlist viewer uses.  Deletions apply in memory and are written
 * back once on exit. */

struct editor_page
{
    struct trimpod_page   base;
    struct gui_synclist   lists;
    struct playlist_info *pl;
    char  path[MAX_PATH];        /* full .m3u8 path, for the save-on-exit */
    bool  dirty;
};

/* track display name = the file's basename */
static void editor_track_name(struct editor_page *p, int sel, char *out, size_t len)
{
    struct playlist_track_info info;
    if (p->pl && playlist_get_track_info(p->pl, sel, &info) == 0)
    {
        const char *base = strrchr(info.filename, PATH_SEPCH);
        strlcpy(out, base ? base + 1 : info.filename, len);
    }
    else
        out[0] = '\0';
}

static const char *editor_get_name(int sel, void *data, char *buf, size_t buf_len)
{
    editor_track_name((struct editor_page *)data, sel, buf, buf_len);
    return buf;
}

static const char *editor_legend(struct trimpod_page *self)
{
    (void)self;
    return "Y Remove   B Back";
}

static void editor_draw(struct trimpod_page *self)
{
    gui_synclist_draw(&((struct editor_page *)self)->lists);
}

static int editor_poll(struct trimpod_page *self, int timeout)
{
    int action;
    list_do_action(self->context, timeout,
                   &((struct editor_page *)self)->lists, &action);
    return action;
}

static enum trimpod_page_result editor_on_action(struct trimpod_page *self, int action)
{
    struct editor_page *p = (struct editor_page *)self;
    int sel = gui_synclist_get_sel_pos(&p->lists);
    int n   = p->pl ? playlist_amount_ex(p->pl) : 0;

    if (action == ACTION_STD_CANCEL)             /* B: leave (saved on exit) */
        return TRIMPOD_PAGE_DONE;

    if (action == ACTION_STD_MENU && sel >= 0 && sel < n)   /* Y: remove track */
    {
        char name[MAX_PATH];
        editor_track_name(p, sel, name, sizeof(name));
        if (trimpod_confirm("Remove from playlist?", name) &&
            playlist_delete(p->pl, sel) == 0)
        {
            p->dirty = true;
            int left = playlist_amount_ex(p->pl);
            gui_synclist_set_nb_items(&p->lists, left);
            if (sel >= left && left > 0)
                gui_synclist_select_item(&p->lists, left - 1);
        }
    }
    return TRIMPOD_PAGE_STAY;
}

static const struct trimpod_page_vtable editor_vtable =
{
    .legend    = editor_legend,
    .draw      = editor_draw,
    .poll      = editor_poll,
    .on_action = editor_on_action,
};

/* only Y (remove) and B (back) act; scrolling is consumed in poll */
static const int editor_allowed[] = { ACTION_STD_MENU, ACTION_STD_CANCEL, -1 };

/* Edit playlist `sel`: load it, let the user remove tracks, save if changed. */
static void edit_playlist(int sel)
{
    size_t bufsz;
    char *buf = scratch_buffer_get(&bufsz);
    if (!buf || bufsz < 8192)
    {
        splashf(HZ, "Cannot edit playlist");
        return;
    }

    struct editor_page p =
    {
        .base = { .vt = &editor_vtable, .context = CONTEXT_LIST,
                  .allowed = editor_allowed },
    };
    path_append(p.path, pl_dir, pl_namebuf + pl_off[sel], sizeof(p.path));

    int index_sz = (int)playlist_get_index_bufsz(bufsz - 4096);
    p.pl = playlist_load(pl_dir, pl_namebuf + pl_off[sel],
                         buf, index_sz, buf + index_sz, (int)(bufsz - index_sz));
    if (!p.pl)
    {
        splashf(HZ, "Cannot load playlist");
        return;
    }

    char title[MAX_PATH];
    pl_display_name(sel, title, sizeof(title));
    gui_synclist_init(&p.lists, editor_get_name, &p, false, 1, NULL);
    gui_synclist_set_title(&p.lists, title, Icon_Playlist);
    gui_synclist_set_nb_items(&p.lists, playlist_amount_ex(p.pl));
    gui_synclist_select_item(&p.lists, 0);

    push_current_activity(ACTIVITY_PLAYLISTBROWSER);
    trimpod_page_run(&p.base);
    pop_current_activity();

    if (p.dirty)
        playlist_save(p.pl, p.path);
    playlist_close(p.pl);
}

/* The per-playlist action chooser.  do_menu returns the row index
 * (0 Play, 1 Rename, 2 Edit, 3 Delete) or GO_TO_PREVIOUS on cancel. */
enum { PL_ACT_PLAY = 0, PL_ACT_SHUFFLE, PL_ACT_RENAME, PL_ACT_EDIT, PL_ACT_DELETE };
MENUITEM_STRINGLIST(playlist_action_menu, ID2P(LANG_PLAYLISTS), NULL,
                    "Play Playlist", "Shuffle Playlist", "Rename Playlist",
                    "Edit Playlist", "Delete Playlist");

/* Open the action chooser for playlist `sel`; returns TRIMPOD_PAGE_DONE when a
 * chosen action leaves the page (play -> Now Playing), else STAY. */
static enum trimpod_page_result playlist_action(struct playlists_page *p, int sel)
{
    char path[MAX_PATH];
    path_append(path, pl_dir, pl_namebuf + pl_off[sel], sizeof(path));

    int act = do_menu(&playlist_action_menu, NULL, NULL, false);
    switch (act)
    {
        case PL_ACT_PLAY:
        case PL_ACT_SHUFFLE:
        {
            /* Record the pick and hand off to the root dispatch: playback starts
             * immediately (from the top, or shuffled) and slides to Now Playing,
             * with the playlist's file list shown *behind* it -- backing out of
             * the WPS reveals it.  Routed via GO_TO_PLAYLIST_VIEWER; the play
             * happens in trimpod_playlists_start_pending() (root_menu.c). */
            if (!warn_on_pl_erase())            /* about to replace the playlist */
                break;                          /* declined: stay on the list */
            strlcpy(pl_pending_file, pl_namebuf + pl_off[sel], sizeof pl_pending_file);
            pl_pending_shuffle = (act == PL_ACT_SHUFFLE);
            pl_pending_active = true;
            p->result = GO_TO_PLAYLIST_VIEWER;
            return TRIMPOD_PAGE_DONE;
        }
        case PL_ACT_RENAME:
        {
            /* Pre-fill the keyboard with the current name, then rename the
             * .m3u8 file -- the same operation fileop.c's rename_file does. */
            char name[MAX_PATH];
            pl_display_name(sel, name, sizeof(name));
            if (trimpod_kbd_input(name, sizeof(name)) == 0 && name[0])
            {
                char newpath[MAX_PATH];
                snprintf(newpath, sizeof(newpath), "%s/%s.m3u8", pl_dir, name);
                if (strcmp(newpath, path) == 0)
                    break;                              /* unchanged */
                if (file_exists(newpath))
                    splashf(HZ, "Name already exists");
                else if (rename(path, newpath) == 0)
                {
                    pl_load();
                    gui_synclist_set_nb_items(&p->lists, pl_count + 1);
                }
                else
                    splashf(HZ, "Rename failed");
            }
            break;
        }
        case PL_ACT_EDIT:
            edit_playlist(sel);
            break;
        case PL_ACT_DELETE:
        {
            char name[MAX_PATH];
            pl_display_name(sel, name, sizeof(name));
            if (trimpod_confirm("Delete this playlist?", name))
            {
                remove(path);
                pl_load();
                gui_synclist_set_nb_items(&p->lists, pl_count + 1);
                if (sel > pl_count)
                    gui_synclist_select_item(&p->lists, pl_count);
            }
            break;
        }
        default:                                /* GO_TO_PREVIOUS / cancel */
            break;
    }
    return TRIMPOD_PAGE_STAY;
}

/* ---- pick mode: add the pending file/dir to a chosen / new playlist ----- */

/* Insert pick_sel into an existing playlist `sel` (Rockbox expands a directory
 * into its tracks); the screen closes afterwards. */
static enum trimpod_page_result pick_into_existing(struct playlists_page *p, int sel)
{
    char path[MAX_PATH];
    path_append(path, pl_dir, pl_namebuf + pl_off[sel], sizeof(path));
    catalog_insert_into(path, false, p->pick_sel, p->pick_attr);
    splashf(HZ, "Added to playlist");
    return TRIMPOD_PAGE_DONE;
}

/* Standalone screen only: remember row `sel` so the next entry re-highlights it
 * (empty string = the "+ Add Playlist" row). */
static void pl_save_pos(struct playlists_page *p, int sel)
{
    if (p->pick)
        return;
    if (sel >= 0 && sel < pl_count)
        strlcpy(pl_last_sel, pl_namebuf + pl_off[sel], sizeof(pl_last_sel));
    else
        pl_last_sel[0] = '\0';
    pl_have_last = true;
}

static enum trimpod_page_result playlists_on_action(struct trimpod_page *self,
                                                    int action)
{
    struct playlists_page *p = (struct playlists_page *)self;
    int sel = gui_synclist_get_sel_pos(&p->lists);

    if (action == ACTION_STD_CANCEL)            /* B: leave */
    {
        pl_save_pos(p, sel);
        return TRIMPOD_PAGE_DONE;
    }

    if (action == ACTION_STD_OK)                /* A */
    {
        if (sel == pl_count)                    /* "+ Add Playlist": create empty */
            add_playlist(p);                    /* (both modes) then stay, so a
                                                 * pick-mode user can now select it */
        else if (sel >= 0 && sel < pl_count)    /* a playlist row */
        {
            enum trimpod_page_result r =
                p->pick ? pick_into_existing(p, sel) : playlist_action(p, sel);
            if (r == TRIMPOD_PAGE_DONE)          /* leaving (e.g. play -> WPS) */
                pl_save_pos(p, sel);
            return r;
        }
    }
    return TRIMPOD_PAGE_STAY;                    /* the runner redraws each frame */
}

static const struct trimpod_page_vtable playlists_vtable =
{
    .legend    = NULL,                  /* normal header (no legend) */
    .draw      = playlists_draw,
    .poll      = playlists_poll,
    .on_action = playlists_on_action,
};

/* only A (add/open) and B (back) act; list scrolling is consumed in poll */
static const int playlists_allowed[] = { ACTION_STD_OK, ACTION_STD_CANCEL, -1 };

/* Shared driver: (re)read the catalog, run the page with the given title. */
static int run_playlists(struct playlists_page *p, const char *title)
{
    pl_load();
    gui_synclist_init(&p->lists, pl_get_name, NULL, false, 1, NULL);
    gui_synclist_set_title(&p->lists, (char *)title, Icon_Playlist);
    gui_synclist_set_nb_items(&p->lists, pl_count + 1);

    /* Standalone screen: restore the remembered selection (incl. on return from
     * Now Playing); pick mode always starts at the top. */
    int sel = 0;
    if (!p->pick && pl_have_last)
    {
        if (pl_last_sel[0])
        {
            for (int i = 0; i < pl_count; i++)
                if (strcmp(pl_namebuf + pl_off[i], pl_last_sel) == 0)
                {
                    sel = i;
                    break;
                }
        }
        else
            sel = pl_count;          /* the "+ Add Playlist" row */
    }
    gui_synclist_select_item(&p->lists, sel);

    push_current_activity(ACTIVITY_PLAYLISTBROWSER);
    trimpod_page_run(&p->base);
    pop_current_activity();
    return p->result;
}

/* Consume a pending Play/Shuffle Playlist request (set by the action menu) and
 * start playback, mirroring the root Shuffle action: physically shuffle the
 * track order when requested, then play from the top; global_settings.playlist_
 * shuffle is left untouched.  Returns 1 when playback started (caller -> Now
 * Playing), 0 when nothing was pending (caller -> show the file list), or -1 on
 * an empty/unloadable playlist (caller -> back to the Playlists list). */
int trimpod_playlists_start_pending(void)
{
    if (!pl_pending_active)
        return 0;
    pl_pending_active = false;

    if (playlist_create(pl_dir, pl_pending_file) == -1 || playlist_amount() <= 0)
    {
        splashf(HZ, "Playlist is empty");
        return -1;
    }
    if (pl_pending_shuffle)
        playlist_shuffle(current_tick, -1);
    playlist_start(0, 0, 0);
    return 1;
}

int trimpod_playlists_screen(void *param)
{
    (void)param;
    struct playlists_page p =
    {
        /* enter_honor_back: returning from Now Playing (which armed a back)
         * slides this screen in L->R; a fresh open from the root enters forward. */
        .base = { .vt = &playlists_vtable, .context = CONTEXT_LIST,
                  .allowed = playlists_allowed, .enter_honor_back = true },
        /* Back to the root menu via GO_TO_ROOT (NOT GO_TO_PREVIOUS): the root
         * dispatch keeps last_screen == our entry, so the menu re-highlights the
         * "Playlists" row on return -- the same contract Settings uses. */
        .result = GO_TO_ROOT,
    };
    return run_playlists(&p, str(LANG_PLAYLISTS));
}

void trimpod_playlists_pick(const char *sel, int sel_attr)
{
    struct playlists_page p =
    {
        .base = { .vt = &playlists_vtable, .context = CONTEXT_LIST,
                  .allowed = playlists_allowed },
        .result = GO_TO_ROOT,
        .pick = true, .pick_sel = sel, .pick_attr = sel_attr,
    };
    run_playlists(&p, "Add to Playlist");
}
