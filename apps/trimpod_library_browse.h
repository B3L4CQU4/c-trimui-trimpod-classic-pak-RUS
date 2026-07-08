/* Trimpod: tag-based Music Library browse over the SQLite index.
 *
 * The root-menu "Artists" entry: a three-level in-place navigator -- Artists ->
 * Albums (with an "All Songs" row) -> Tracks.  Selecting a track builds the
 * playlist from the index (trimpod_library_build_playlist) and starts it at that
 * track; returning from Now Playing reopens the Tracks list on the playing song.
 * Music category only -- podcasts/audiobooks stay folder-based.  (Rescan Library
 * lives in Settings -> Library.)  See trimpod_library.h for the backend. */
#ifndef _TRIMPOD_LIBRARY_BROWSE_H
#define _TRIMPOD_LIBRARY_BROWSE_H

/* Root-menu handler (items[] signature).  Returns GO_TO_WPS when a selection
 * started playback, else GO_TO_ROOT. */
int trimpod_library_browse(void *param);

#endif /* _TRIMPOD_LIBRARY_BROWSE_H */
