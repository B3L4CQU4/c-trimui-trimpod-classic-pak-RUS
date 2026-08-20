/***************************************************************************
 * Minimal synchronized-lyrics support for TrimPod(RUS).
 ***************************************************************************/
#ifndef TRIMPOD_LYRICS_H
#define TRIMPOD_LYRICS_H

struct mp3entry;

enum trimpod_lyrics_status
{
    TRIMPOD_LYRICS_IDLE = 0,
    TRIMPOD_LYRICS_LOADING,
    TRIMPOD_LYRICS_READY,
    TRIMPOD_LYRICS_NOT_FOUND,
    TRIMPOD_LYRICS_ERROR,
};

struct trimpod_lyric_line
{
    long time_ms;
    char text[256];
};

/* Local sidecar/cache lookup is synchronous.  A missing local file starts a
 * non-blocking LRCLIB request; call trimpod_lyrics_poll() from the UI loop. */
void trimpod_lyrics_request(const struct mp3entry *id3);
void trimpod_lyrics_poll(void);
void trimpod_lyrics_clear(void);

enum trimpod_lyrics_status trimpod_lyrics_get_status(void);
int trimpod_lyrics_count(void);
const struct trimpod_lyric_line *trimpod_lyrics_line(int index);
int trimpod_lyrics_current(unsigned long elapsed_ms);

#endif /* TRIMPOD_LYRICS_H */
