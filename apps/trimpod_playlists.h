/* Trimpod: the Playlists screen (see trimpod_playlists.c).  A trimpod_page that
 * lists the catalog's playlists plus a "+ Add Playlist" row, mirroring the
 * Music Folders settings page. */
#ifndef _TRIMPOD_PLAYLISTS_H
#define _TRIMPOD_PLAYLISTS_H

/* root menu Playlists entry (a root_menu items[] function) */
int trimpod_playlists_screen(void *param);

/* Show the Playlists screen in "pick" mode: the chosen (or newly created)
 * playlist receives `sel` (a file or directory) via the stock Rockbox
 * catalog_insert_into() -- a directory is expanded into its tracks.  Used by the
 * folder browsers' Y = "Add to Playlist". */
void trimpod_playlists_pick(const char *sel, int sel_attr);

#endif /* _TRIMPOD_PLAYLISTS_H */
