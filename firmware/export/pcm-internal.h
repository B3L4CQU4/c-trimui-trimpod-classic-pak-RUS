/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2005 by Linus Nielsen Feltzing
 * Copyright (C) 2011 by Michael Sevakis
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
#ifndef PCM_INTERNAL_H
#define PCM_INTERNAL_H

#include <stdbool.h>

#include "config.h"
#include "pcm.h"
#include "pcm_sink.h"
#include "gcc_extensions.h" /* for FORCE_INLINE */

#define PCM_SAMPLE_SIZE     (2 * sizeof (int16_t))
/* Cheapo buffer align macro to align to the 16-16 PCM size */
#define ALIGN_AUDIOBUF(start, size) \
    ({ (start) = (void *)(((uintptr_t)(start) + 3) & ~3); \
       (size) &= ~3; })

/* Internal PCM API calls for playback */
void pcm_play_data(pcm_play_callback_type get_more,
                   pcm_status_callback_type status_cb,
                   const void *start, size_t size);

void pcm_play_stop(void);
void pcm_play_stop_int(void); /* requires PCM lock held */
bool pcm_is_playing(void);

void pcm_set_frequency(unsigned int samplerate);
unsigned int pcm_get_frequency(void);
/* apply settings to hardware immediately */
void pcm_apply_settings(void);

void pcm_do_peak_calculation(struct pcm_peaks *peaks, bool active,
                             const void *addr, int count);

/** The following are for internal use between pcm.c and target-
    specific portion **/
/* Call registered callback to obtain next buffer */
static inline bool pcm_get_more_int(const void **addr, size_t *size)
{
    extern volatile pcm_play_callback_type pcm_callback_for_more;
    pcm_play_callback_type get_more = pcm_callback_for_more;

    if (UNLIKELY(!get_more))
        return false;

    *addr = NULL;
    *size = 0;
    get_more(addr, size);
    ALIGN_AUDIOBUF(*addr, *size);

    return *addr && *size;
}

static FORCE_INLINE enum pcm_dma_status pcm_call_status_cb(
    pcm_status_callback_type callback, enum pcm_dma_status status)
{
    if (!callback)
        return status;

    return callback(status);
}

static FORCE_INLINE enum pcm_dma_status pcm_play_call_status_cb(
    enum pcm_dma_status status)
{
    extern enum pcm_dma_status
        (* volatile pcm_play_status_callback)(enum pcm_dma_status);
    return pcm_call_status_cb(pcm_play_status_callback, status);
}

static FORCE_INLINE enum pcm_dma_status
pcm_play_dma_status_callback(enum pcm_dma_status status)
{
    return pcm_play_call_status_cb(status);
}

/* Called by the bottom layer ISR when more data is needed. Returns true
 * if a new buffer is available, false otherwise. */
bool pcm_play_dma_complete_callback(enum pcm_dma_status status,
                                    const void **addr, size_t *size);


extern volatile bool pcm_playing;
struct pcm_sink* pcm_get_current_sink(void);


#endif /* PCM_INTERNAL_H */
