/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2010 Thomas Martitz
 * Copyright (c) 2020 Solomon Peachy
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

/* Trimpod: direct ALSA output with hardware/software split volume, adapted
 * from upstream pcm-alsa.c.
 *
 * The stock path (SDL -> ALSA "default") ends in a dmix bus hardcoded to
 * S16_LE, so any volume attenuation applied to 16-bit samples permanently
 * quantizes away quiet content: at -40 dB everything in the recording below
 * about -56 dBFS rounds to digital silence, which gates quiet passages on
 * dynamic material.  The codec claims S24_LE but renders it silently (BSP
 * driver bug, verified on hardware), so wide output is not an option.
 * Instead the coarse attenuation goes to the codec's 'digital volume'
 * control (0..63, 1.16 dB/step, 0 = 0 dB) where it happens at the codec's
 * internal width, and software applies only the <=1.16 dB remainder between
 * hardware steps (plus the tail beyond the control's -73 dB floor).  A 16-bit
 * multiply that shallow keeps the truncation floor ~90 dB below the music.
 *
 * Uses the async (SIGIO) callback method with an alternative signal stack.
 */

#include "autoconf.h"

#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <alsa/asoundlib.h>

#include "system.h"
#include "debug.h"
#include "kernel.h"
#include "panic.h"

#include "pcm.h"
#include "pcm-internal.h"
#include "pcm_mixer.h"
#include "pcm_sampr.h"
#include "pcm_sink.h"
#include "audiohw.h"
#include "fixedpoint.h"

/*#define LOGF_ENABLE*/
#include "logf.h"

#include <pthread.h>
#include <signal.h>

/* Direct hardware device (by card name, stable across index changes).
 * Bypasses NextUI's softvol/dmix layers; exclusive while we run. */
#define PLAYBACK_DEVICE "hw:audiocodec,0"

#if MIX_FRAME_SAMPLES < 1024
#error "MIX_FRAME_SAMPLES <1024 may cause dropouts!"
#endif

static const snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
static const int channels = 2;
static unsigned int real_sample_rate;
static unsigned int last_sample_rate;

static snd_pcm_t *handle = NULL;
static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static int16_t *frames = NULL;

static const void  *pcm_data = 0;
static size_t       pcm_size = 0;

static snd_async_handler_t *ahandler = NULL;
static pthread_mutex_t pcm_mtx;
/* fixed size: SIGSTKSZ is not a compile-time constant on newer glibc */
static long signal_stack[16384 / sizeof(long)];

/* Codec 'digital volume': 0..63, 1.16 dB attenuation per step, 0 = 0 dB
 * (the ALSA TLV direction flag is wrong; 0 = loudest verified on hardware,
 * matching NextUI's usage and pak/launch.sh). */
#define DV_CTL_NAME   "digital volume"
#define DV_STEP_MDB   1160  /* milli-dB per step */
#define DV_MAX_STEPS  63

static snd_ctl_t *dv_ctl = NULL;
static snd_ctl_elem_value_t *dv_val = NULL;

/* Q16 software fine-trim factors; unity = 1<<16.  Written from the audiohw
 * thread, read in the SIGIO handler: aligned word-sized stores are atomic on
 * aarch64, same assumption as upstream. */
static int32_t trim_mult_l = 1 << 16;
static int32_t trim_mult_r = 1 << 16;

/* last volume request in centibels (tenth-dB); INT_MIN = mute */
static int cur_vol_l = 0, cur_vol_r = 0;

static void hw_dv_open(void)
{
    if (dv_ctl)
        return;
    if (snd_ctl_open(&dv_ctl, "hw:audiocodec", 0) < 0)
    {
        dv_ctl = NULL; /* fall back to software-only volume */
        return;
    }
    snd_ctl_elem_value_malloc(&dv_val);
    snd_ctl_elem_value_set_interface(dv_val, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_value_set_name(dv_val, DV_CTL_NAME);
}

static int32_t mdb_to_factor(int att_mdb)
{
    if (att_mdb <= 0)
        return 1 << 16;
    long f = fp_factor(fp_div(-att_mdb, 1000, 16), 16);
    return f >= (1 << 16) ? (1 << 16) : (int32_t)f;
}

static void apply_volume(void)
{
    int vol_l = cur_vol_l, vol_r = cur_vol_r;

    /* attenuations in milli-dB (volumes are <= 0) */
    int att_l = vol_l == INT_MIN ? INT_MAX : -vol_l * 100;
    int att_r = vol_r == INT_MIN ? INT_MAX : -vol_r * 100;

    /* the shared part goes to the codec, in whole hardware steps */
    int common = MIN(att_l, att_r);
    int steps = common == INT_MAX ? DV_MAX_STEPS : common / DV_STEP_MDB;
    if (steps > DV_MAX_STEPS)
        steps = DV_MAX_STEPS;
    if (!dv_ctl)
        steps = 0; /* no mixer access: software carries everything */

    /* software covers the remainder (per-channel balance + sub-step fine
     * trim + any tail beyond the control's range); mute is a zero factor */
    trim_mult_l = att_l == INT_MAX ? 0 : mdb_to_factor(att_l - steps * DV_STEP_MDB);
    trim_mult_r = att_r == INT_MAX ? 0 : mdb_to_factor(att_r - steps * DV_STEP_MDB);

    if (dv_ctl)
    {
        snd_ctl_elem_value_set_integer(dv_val, 0, steps);
        snd_ctl_elem_write(dv_ctl, dv_val);
    }
}

/* Volume in centibels (tenth-dB), INT_MIN mutes; called by audiohw */
void pcm_set_mixer_volume(int vol_l, int vol_r)
{
    cur_vol_l = vol_l;
    cur_vol_r = vol_r;
    apply_volume();
}

static int set_hwparams(snd_pcm_t *handle, unsigned long sampr)
{
    int err;
    unsigned int srate;
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_malloc(&params);

    /* Sized in FRAMES.  8 periods = the hardware maximum, ~186 ms @44.1k:
     * app startup (resume) saturates the CPU and the SIGIO callback can be
     * delayed ~100 ms, which underruns a smaller buffer audibly. */
    if (sampr > SAMPR_48) {
        buffer_size = MIX_FRAME_SAMPLES * 2 * 8;
        period_size = MIX_FRAME_SAMPLES * 2;
    } else {
        buffer_size = MIX_FRAME_SAMPLES * 8;
        period_size = MIX_FRAME_SAMPLES;
    }

    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0)
    {
        panicf("Broken configuration for playback: no configurations available: %s", snd_strerror(err));
        goto error;
    }
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        panicf("Access type not available for playback: %s", snd_strerror(err));
        goto error;
    }
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0)
    {
        logf("Sample format not available for playback: %s", snd_strerror(err));
        goto error;
    }
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0)
    {
        logf("Channels count (%i) not available for playback: %s", channels, snd_strerror(err));
        goto error;
    }
    srate = sampr;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &srate, 0);
    if (err < 0)
    {
        logf("Rate %luHz not available for playback: %s", sampr, snd_strerror(err));
        goto error;
    }
    real_sample_rate = srate;
    if (real_sample_rate != sampr)
    {
        logf("Rate doesn't match (requested %luHz, get %dHz)", sampr, real_sample_rate);
        err = -EINVAL;
        goto error;
    }

    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
    if (err < 0)
    {
        logf("Unable to set buffer size %ld for playback: %s", buffer_size, snd_strerror(err));
        goto error;
    }

    err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, NULL);
    if (err < 0)
    {
        logf("Unable to set period size %ld for playback: %s", period_size, snd_strerror(err));
        goto error;
    }

    if (frames) free(frames);
    frames = calloc(1, period_size * channels * sizeof(int16_t));

    err = snd_pcm_hw_params(handle, params);
    if (err < 0)
    {
        logf("Unable to set hw params for playback: %s", snd_strerror(err));
        goto error;
    }

    err = 0; /* success */
error:
    snd_pcm_hw_params_free(params);
    return err;
}

/* Set sw params: playback start threshold and low buffer watermark */
static int set_swparams(snd_pcm_t *handle)
{
    int err;

    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_malloc(&swparams);

    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0)
    {
        logf("Unable to determine current swparams for playback: %s", snd_strerror(err));
        goto error;
    }
    /* start only once the whole buffer is primed; partially-filled starts
     * (source dry during priming) go through the explicit snd_pcm_start */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size);
    if (err < 0)
    {
        logf("Unable to set start threshold mode for playback: %s", snd_strerror(err));
        goto error;
    }
    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
    if (err < 0)
    {
        logf("Unable to set avail min for playback: %s", snd_strerror(err));
        goto error;
    }
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0)
    {
        logf("Unable to set sw params for playback: %s", snd_strerror(err));
        goto error;
    }

    err = 0; /* success */
error:
    snd_pcm_sw_params_free(swparams);
    return err;
}

/* --- Trimpod visualizer PCM tap -----------------------------------------
 * The spectrum and Milkdrop visualizer (projectM) need the audio waveform to
 * react to the beat.  We mirror the PRE-volume Rockbox S16 stereo source (see
 * copy_frames) into a lock-free ring buffer (single producer = the SIGIO audio
 * callback, single consumer = the render loop) so the visuals are independent
 * of the volume setting.  Torn reads are harmless for a visualizer. */
#define VIZ_PCM_FRAMES 8192            /* must be a power of two */
static int16_t viz_pcm_ring[VIZ_PCM_FRAMES * 2];
static volatile unsigned viz_pcm_w;    /* total stereo frames ever written */

static void viz_pcm_push(const int16_t *src, unsigned frames_in)
{
    unsigned w = viz_pcm_w;
    for (unsigned i = 0; i < frames_in; i++)
    {
        unsigned idx = (w & (VIZ_PCM_FRAMES - 1)) * 2;
        viz_pcm_ring[idx]     = src[2 * i];
        viz_pcm_ring[idx + 1] = src[2 * i + 1];
        w++;
    }
    viz_pcm_w = w;
}

/* Copy the most recent up-to max_frames stereo frames into out (LRLR).
 * Returns the number of frames written. Called by trimpod_visualizer.c and
 * trimpod_spectrum.c. */
unsigned pcm_viz_latest(int16_t *out, unsigned max_frames)
{
    unsigned w = viz_pcm_w;
    unsigned n = max_frames < VIZ_PCM_FRAMES ? max_frames : VIZ_PCM_FRAMES;
    if (n > w)
        n = w;
    unsigned start = w - n;
    for (unsigned i = 0; i < n; i++)
    {
        unsigned idx = ((start + i) & (VIZ_PCM_FRAMES - 1)) * 2;
        out[2 * i]     = viz_pcm_ring[idx];
        out[2 * i + 1] = viz_pcm_ring[idx + 1];
    }
    return n;
}

/* copy pcm samples to a spare buffer, suitable for snd_pcm_writei() */
/* After an underrun the BSP DMA can audibly replay stale ring contents;
 * overwrite the ring head with one silent period before real data. */
static void write_silence_period(void)
{
    memset(frames, 0, period_size * channels * sizeof(int16_t));
    snd_pcm_writei(handle, frames, period_size);
}

/* true while the pcm engine wants data; false = idle, feed silence */
static bool sink_running = false;

/* Fill the period buffer with source data, silence-filling whenever idle or
 * the source runs dry, so the stream NEVER stops: every snd_pcm_start powers
 * the codec DAC/amp back up (DAPM), which is audible as a buzz on the
 * speaker.  The stream is started once and kept running for the app's
 * lifetime, like the always-running dmix bus the stock OS uses. */
static void copy_frames(void)
{
    ssize_t nframes, frames_left = period_size;
    bool new_buffer = false;

    while (frames_left > 0)
    {
        if (!pcm_size)
        {
            new_buffer = true;
            if (!sink_running ||
                !pcm_play_dma_complete_callback(PCM_DMAST_OK, &pcm_data,
                                                &pcm_size))
            {
                memset(&frames[2*(period_size-frames_left)], 0,
                       frames_left * channels * sizeof(int16_t));
                return;
            }
        }

        /* S16 stereo source */
        if (pcm_size % 4)
            panicf("Wrong pcm_size");
        nframes = MIN((ssize_t)pcm_size/4, frames_left);

        /* Apply the software fine trim (<=1.16 dB below unity in normal
         * operation, see apply_volume); rounded Q16 multiply. */
        const int16_t *pcm_ptr = pcm_data;
        int16_t *sample_ptr = &frames[2*(period_size-frames_left)];
        for (int i = 0; i < nframes; i++)
        {
            *sample_ptr++ = ((int32_t)*pcm_ptr++ * trim_mult_l + (1 << 15)) >> 16;
            *sample_ptr++ = ((int32_t)*pcm_ptr++ * trim_mult_r + (1 << 15)) >> 16;
        }

        viz_pcm_push(pcm_data, nframes);

        pcm_data += nframes*4;
        pcm_size -= nframes*4;
        frames_left -= nframes;

        if (new_buffer)
        {
            new_buffer = false;
            pcm_play_dma_status_callback(PCM_DMAST_STARTED);
        }
    }
}

static void async_callback(snd_async_handler_t *ahandler)
{
    int err;

    if (!ahandler) return;

    snd_pcm_t *handle = snd_async_handler_get_pcm(ahandler);

    if (!handle) return;

    if (pthread_mutex_trylock(&pcm_mtx) != 0)
        return;

    snd_pcm_state_t state = snd_pcm_state(handle);

    if (state == SND_PCM_STATE_XRUN)
    {
        logf("initial underrun!");
        err = snd_pcm_recover(handle, -EPIPE, 0);
        if (err < 0) {
            logf("XRUN Recovery error: %s", snd_strerror(err));
            goto abort;
        }
        write_silence_period();
    }
    else if (state == SND_PCM_STATE_DRAINING)
    {
        logf("draining...");
        goto abort;
    }
    else if (state == SND_PCM_STATE_SETUP)
    {
        goto abort;
    }

    while (snd_pcm_avail_update(handle) >= period_size)
    {
        copy_frames();
    retry:
        err = snd_pcm_writei(handle, frames, period_size);
        if (err == -EPIPE)
        {
            err = snd_pcm_recover(handle, -EPIPE, 0);
            if (err < 0) {
               logf("XRUN Recovery error: %s", snd_strerror(err));
               goto abort;
            }
            write_silence_period();
            goto retry;
        }
        else if (err != period_size)
        {
            logf("Write error: written %i expected %li", err, period_size);
            break;
        }
    }

    if (snd_pcm_state(handle) == SND_PCM_STATE_PREPARED)
    {
        err = snd_pcm_start(handle);
        if (err < 0) {
            logf("cb start error: %s", snd_strerror(err));
            /* Depending on the error we might be SOL */
        }
    }

abort:
    pthread_mutex_unlock(&pcm_mtx);
}

static void close_hwdev(void)
{
    logf("closedev (%p)", handle);

    if (handle) {
        snd_pcm_drain(handle);
        if (ahandler) {
            snd_async_del_handler(ahandler);
            ahandler = NULL;
        }
        snd_pcm_close(handle);

        handle = NULL;
    }
}

static void alsadev_cleanup(void)
{
    free(frames);
    frames = NULL;
    close_hwdev();
}

static void open_hwdev(const char *device)
{
    int err;

    logf("opendev %s (%p)", device, handle);

    if (handle)
        return;

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        panicf("%s(): Cannot open device %s: %s", __func__, device, snd_strerror(err));
    }
    last_sample_rate = 0;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&pcm_mtx, &attr);

    /* assign alternative stack for the signal handlers */
    stack_t ss = {
        .ss_sp = signal_stack,
        .ss_size = sizeof(signal_stack),
        .ss_flags = 0
    };
    struct sigaction sa;

    err = sigaltstack(&ss, NULL);
    if (err < 0)
    {
        panicf("Unable to install alternative signal stack: %s", strerror(err));
    }

    err = snd_async_add_pcm_handler(&ahandler, handle, async_callback, NULL);
    if (err < 0)
    {
        panicf("Unable to register async handler: %s", snd_strerror(err));
    }

    /* only modify the stack the handler runs on */
    sigaction(SIGIO, NULL, &sa);
    sa.sa_flags |= SA_ONSTACK;
    err = sigaction(SIGIO, &sa, NULL);
    if (err < 0)
    {
        panicf("Unable to install alternative signal stack: %s", strerror(err));
    }

    atexit(alsadev_cleanup);
}

static void sink_dma_init(void)
{
    logf("PCM DMA Init");

    open_hwdev(PLAYBACK_DEVICE);
    hw_dv_open();
    apply_volume(); /* re-split anything set before the mixer was open */
}

static void ensure_running(void);

static void sink_lock(void)
{
    pthread_mutex_lock(&pcm_mtx);
}

static void sink_unlock(void)
{
    pthread_mutex_unlock(&pcm_mtx);
}

static void sink_set_freq(uint16_t freq)
{
    unsigned int sampr = hw_freq_sampr[freq];

    sink_lock();

    logf("PCM DMA Settings %d %u", last_sample_rate, sampr);

    if (last_sample_rate != sampr)
    {
        last_sample_rate = sampr;

        snd_pcm_drop(handle);

        int e1 = set_hwparams(handle, sampr);
        int e2 = set_swparams(handle);
        if (e1 == 0 && e2 == 0)
            ensure_running(); /* new rate, new stream: restart immediately */
    }

    sink_unlock();
}

/* Bring the stream to RUNNING, priming with source data or silence */
static void ensure_running(void)
{
    while (1)
    {
        snd_pcm_state_t state = snd_pcm_state(handle);

        switch (state)
        {
            case SND_PCM_STATE_RUNNING:
                return;
            case SND_PCM_STATE_XRUN:
            {
                int err = snd_pcm_recover(handle, -EPIPE, 0);
                if (err < 0)
                {
                    logf("Recovery failed: %s", snd_strerror(err));
                    return;
                }
                write_silence_period();
                continue;
            }
            case SND_PCM_STATE_SETUP:
            {
                int err = snd_pcm_prepare(handle);
                if (err < 0)
                {
                    logf("Prepare error: %s", snd_strerror(err));
                    return;
                }
            }
                /* fall through */
            case SND_PCM_STATE_PREPARED:
            {
                while (snd_pcm_avail_update(handle) >= period_size)
                {
                    copy_frames();
                    int err = snd_pcm_writei(handle, frames, period_size);
                    if (err < 0 && err != -EAGAIN)
                        break;
                }
                /* auto-starts at the full-buffer threshold; explicit start
                 * covers partial priming */
                if (snd_pcm_state(handle) == SND_PCM_STATE_PREPARED)
                {
                    int err = snd_pcm_start(handle);
                    if (err < 0)
                        return; /* retried on the next play/period */
                }
                break;
            }
            case SND_PCM_STATE_DRAINING:
                /* run until drained */
                continue;
            default:
                logf("Unhandled state: %s", snd_pcm_state_name(state));
                return;
        }
    }
}

static void sink_dma_stop(void)
{
    /* Keep the hardware stream hot (see copy_frames); just go idle.  Caller
     * holds the sink lock, so the SIGIO callback can't race these. */
    sink_running = false;
    pcm_data = NULL;
    pcm_size = 0;
}

static void sink_dma_start(const void *addr, size_t size)
{

    pcm_data = addr;
    pcm_size = size;
    sink_running = true;

    /* Normally already RUNNING (data is picked up within one period);
     * (re)starts the stream on the first play or after an error. */
    ensure_running();
}

static void sink_dma_postinit(void)
{
    /* Configure the default rate and start the always-on silence stream so
     * the codec power-up transient happens here, at app startup, and never
     * again at a track boundary. */
    sink_set_freq(HW_FREQ_DEFAULT);
    ensure_running();
}

struct pcm_sink builtin_pcm_sink = {
    .caps = {
        .samprs       = hw_freq_sampr,
        .num_samprs   = HW_NUM_FREQ,
        .default_freq = HW_FREQ_DEFAULT,
    },
    .ops = {
        .init     = sink_dma_init,
        .postinit = sink_dma_postinit,
        .set_freq = sink_set_freq,
        .lock     = sink_lock,
        .unlock   = sink_unlock,
        .play     = sink_dma_start,
        .stop     = sink_dma_stop,
    },
};
