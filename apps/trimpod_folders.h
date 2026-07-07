/* Trimpod: user-managed virtual folders -- Music, Podcasts, Audiobooks
 * (see trimpod_folders.c).  Each is a virtual folder merging a user-managed set
 * of source folders plus an unremovable default; all three share one engine. */
#ifndef _TRIMPOD_FOLDERS_H
#define _TRIMPOD_FOLDERS_H

/* Settings -> *Folders pages (MENUITEM_FUNCTION targets) */
int trimpod_music_settings(void);
int trimpod_podcast_settings(void);
int trimpod_audiobook_settings(void);

/* root menu virtual-folder entries (root_menu items[] functions) */
int trimpod_music_browse(void *param);
int trimpod_podcast_browse(void *param);
int trimpod_audiobook_browse(void *param);

/* root menu "Shuffle Songs": build one recursive playlist of every track in the
 * virtual Music folder, shuffle it and start playing.  Returns GO_TO_WPS. */
int trimpod_shuffle_all(void *param);

#endif /* _TRIMPOD_FOLDERS_H */
