/**
 * @file audio_effect.c
 *
 * WAV parser (chunk-based RIFF) on mmap'ed file + playback via audio_mgr.
 * - full RIFF chunk scan for "fmt " and "data"
 * - supports PCM and WAVEFORMATEXTENSIBLE (treated as PCM if subtype indicates PCM)
 * - no fread: file is mmap()ed entirely and parsed via pointer arithmetic
 * - playback feeds audio_mgr_write_mmap in chunks
 */

/*********************
 *      INCLUDES
 *********************/
// #define LOG_LEVEL LOG_LEVEL_TRACE
#if defined(LOG_LEVEL)
#warning "LOG_LEVEL defined locally will override the global setting in this file"
#endif
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdbool.h>

#include "audio/audio.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
/* read little-endian helpers from byte pointer */
static inline uint16_t rd_u16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd_u32le(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/
/* parse "fmt " chunk payload (pointer to payload, size) into audio_format */
static int32_t parse_fmt_chunk(const uint8_t *p, uint32_t size, struct audio_format *out)
{
    if (size < 16) return AUDIO_E_INVAL;

    uint16_t audio_format = rd_u16le(p + 0);
    uint16_t channels     = rd_u16le(p + 2);
    uint32_t sample_rate  = rd_u32le(p + 4);
    /* uint32_t byte_rate = rd_u32le(p + 8); */
    uint16_t block_align  = rd_u16le(p + 12);
    uint16_t bits_per_sample = rd_u16le(p + 14);

    p += 16;

    if (size > 16) {
        /* there is an extension size */
        uint16_t cb = 0;
        if ((size - 16) >= 2) {
            cb = rd_u16le(p);
            p += 2;
        } else {
            return AUDIO_E_INVAL;
        }

        /* WAVEFORMATEXTENSIBLE handling */
        if (audio_format == 0xFFFE) { /* extensible */
            /* need at least 22 bytes for extension: valid_bits, channel_mask, subformat[16] */
            if (cb < 22 || (size < 16 + 2 + 22)) return AUDIO_E_INVAL;
            /* For brevity we accept extensible as PCM (you may check GUID if needed) */
            /* skip extension content */
            /* p currently points to extension start */
            /* We don't parse GUID here; assume PCM subtype */
            /* skip 22 bytes */
            /* (valid_bits, channel_mask, subformat[16]) */
            /* no further action required for our use-case */
        }
        /* otherwise, non-extensible additional bytes are ignored/skipped */
    }

    /* accept only PCM */
    if (audio_format != 0x0001 && audio_format != 0xFFFE) return AUDIO_E_NOSUP;

    out->pcm_format = audio_bits_to_sndfmt(bits_per_sample);
    if (out->pcm_format == SND_PCM_FORMAT_UNKNOWN) return AUDIO_E_NOSUP;

    out->channels = channels;
    out->sample_rate = sample_rate;
    out->bits_per_sample = bits_per_sample;
    out->block_align = block_align;

    return AUDIO_OK;
}

/* parse mmap'ed WAV file: find fmt + data chunk */
static int32_t wav_parse_mmap(const uint8_t *base, size_t size, struct wav_map *wm)
{
    /* minimal RIFF header size = 12 bytes (RIFF + size + WAVE) */
    if (size < 12) return AUDIO_E_INVAL;
    if (memcmp(base, "RIFF", 4) != 0) return AUDIO_E_INVAL;
    if (memcmp(base + 8, "WAVE", 4) != 0) return AUDIO_E_INVAL;

    const uint8_t *p = base + 12;
    const uint8_t *end = base + size;
    int32_t have_fmt = 0;
    int32_t have_data = 0;

    while (p + 8 <= end) {
        const uint8_t *chunk_id = p;
        uint32_t chunk_size = rd_u32le(p + 4);
        const uint8_t *payload = p + 8;
        const uint8_t *next = payload + chunk_size + (chunk_size & 1); /* pad */

        if (payload + chunk_size > end) return AUDIO_E_INVAL;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            int32_t ret = parse_fmt_chunk(payload, chunk_size, &wm->fmt);
            if (ret < 0) return ret;
            have_fmt = 1;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            wm->data = payload;
            wm->data_size = chunk_size;
            have_data = 1;
        } else {
            /* ignore unknown chunk */
        }

        p = next;
    }

    return (have_fmt && have_data) ? AUDIO_OK : AUDIO_E_INVAL;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************//* map bits -> snd pcm format */
snd_pcm_format_t audio_bits_to_sndfmt(unsigned bits)
{
    switch (bits) {
    case 8:  return SND_PCM_FORMAT_U8;
    case 16: return SND_PCM_FORMAT_S16_LE;
    case 24: return SND_PCM_FORMAT_S24_LE;
    case 32: return SND_PCM_FORMAT_S32_LE;
    default: return SND_PCM_FORMAT_UNKNOWN;
    }
}

/* public: open+map file and parse */
int32_t wav_map_open(const char *path, struct wav_map *out)
{
    int32_t fd;
    struct stat st;
    void *base;
    int32_t ret;

    if (!path || !out) return AUDIO_E_INVAL;
    memset(out, 0, sizeof(*out));
    out->fd = -1;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("open('%s'): %s", path, strerror(errno));
        return AUDIO_E_IO;
    }

    if (fstat(fd, &st) < 0) {
        LOG_ERROR("fstat('%s'): %s", path, strerror(errno));
        close(fd);
        return AUDIO_E_IO;
    }
    if ((size_t)st.st_size < 12) {
        LOG_ERROR("file too small: %s", path);
        close(fd);
        return AUDIO_E_INVAL;
    }

    base = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        LOG_ERROR("mmap('%s'): %s", path, strerror(errno));
        close(fd);
        return AUDIO_E_IO;
    }

    out->fd = fd;
    out->base = base;
    out->size = (size_t)st.st_size;

    ret = wav_parse_mmap((const uint8_t *)base, out->size, out);
    if (ret < 0) {
        LOG_ERROR("wav parse failed for '%s'", path);
        munmap(base, out->size);
        close(fd);
        memset(out, 0, sizeof(*out));
        return ret;
    }

    LOG_DEBUG("wav_map_open: '%s' ch=%u sr=%u bps=%u data=%zu",
          path, out->fmt.channels, out->fmt.sample_rate,
          out->fmt.bits_per_sample, out->data_size);

    return AUDIO_OK;
}

/* close mapping */
void wav_map_close(struct wav_map *wm)
{
    if (!wm) return;
    if (wm->base && wm->size) munmap(wm->base, wm->size);
    if (wm->fd >= 0) close(wm->fd);
    memset(wm, 0, sizeof(*wm));
}

/*
 * Validate the format and reinitialize if needed.
 * If skip_check is true, the format validation will be skipped.
 */
static int32_t validate_and_reinit(struct audio_mgr *mgr,
                                   const struct wav_map *wm,
                                   bool skip_check)
{
    int32_t ret = 0;

    if (!mgr || !wm)
        return AUDIO_E_INVAL;

    if (skip_check)
        return AUDIO_OK;

    if (mgr->fmt.channels != wm->fmt.channels ||
        mgr->fmt.sample_rate != wm->fmt.sample_rate ||
        mgr->fmt.pcm_format != wm->fmt.pcm_format) {
        LOG_INFO("Format mismatch, reinitializing audio device...");
        ret = audio_mgr_reinit(mgr,
                               wm->fmt.channels,
                               wm->fmt.sample_rate,
                               wm->fmt.pcm_format);
    }

    return ret;
}

/*
 * Playback loop: write audio frames into PCM device using mmap.
 */
static int32_t playback_loop(struct audio_mgr *mgr, const struct wav_map *wm, \
                             size_t frame_size, size_t total_frames)
{
    const size_t chunk_frames = 4096;
    size_t done = 0;
    int32_t started = 0;
    int32_t ret = 0;

    while (done < total_frames) {
        /* Wait until device can accept more frames (up to 1000 ms) */
        int32_t w = snd_pcm_wait(mgr->pcm, 1000);
        if (w < 0) {
            if (w == -EPIPE || w == -ESTRPIPE) {
                /* recover xrun/suspend */
                (void)audio_mgr_prepare(mgr);
                continue;
            }
            LOG_ERROR("snd_pcm_wait: %s", snd_strerror(w));
            return AUDIO_ERR;
        }

        snd_pcm_sframes_t avail = snd_pcm_avail_update(mgr->pcm);
        if (avail < 0) {
            if (avail == -EPIPE || avail == -ESTRPIPE) {
                (void)audio_mgr_prepare(mgr);
                continue;
            }
            LOG_ERROR("snd_pcm_avail_update: %s", snd_strerror((int)avail));
            return AUDIO_ERR;
        }
        if (avail == 0)
            continue; /* nothing right now, loop back */

        size_t to_do = total_frames - done;
        if ((snd_pcm_sframes_t)to_do > avail) to_do = (size_t)avail;
        if (to_do > chunk_frames) to_do = chunk_frames;

        ret = audio_mgr_write_mmap(mgr, wm->data + done * frame_size, to_do);
        if (ret < 0) {
            LOG_ERROR("audio_mgr_write_mmap failed");
            return ret;
        }

        if (!started) {
            /* Kick playback once after priming */
            int rc = snd_pcm_start(mgr->pcm);
            if (rc < 0 && rc != -EPIPE && rc != -ESTRPIPE) {
                LOG_WARN("snd_pcm_start: %s", snd_strerror(rc));
            }
            started = 1;
        }

        done += to_do;
    }

    /* Let remaining frames in ring play out */
    snd_pcm_drain(mgr->pcm);

    return 0;
}

int32_t audio_mgr_get_hw_params(struct audio_mgr *mgr, \
                                snd_pcm_uframes_t *buffer_size, \
                                snd_pcm_uframes_t *period_size)
{
    int32_t ret;

    if (!mgr || !mgr->pcm || !buffer_size || !period_size)
        return -EINVAL;

    ret = snd_pcm_get_params(mgr->pcm, buffer_size, period_size);
    if (ret < 0) {
        LOG_ERROR("snd_pcm_get_params failed: %s", snd_strerror(ret));
        return ret;
    }

    LOG_DEBUG("buffer_size=%lu period_size=%lu",
              (unsigned long)*buffer_size,
              (unsigned long)*period_size);

    return 0;
}

int32_t audio_mgr_apply_sw_params(struct audio_mgr *mgr, \
                                  snd_pcm_uframes_t buffer_size, \
                                  snd_pcm_uframes_t period_size, \
                                  bool auto_start)
{
    snd_pcm_sw_params_t *sw_params;
    int32_t ret;

    if (!mgr || !mgr->pcm)
        return -EINVAL;

    snd_pcm_sw_params_alloca(&sw_params);

    ret = snd_pcm_sw_params_current(mgr->pcm, sw_params);
    if (ret < 0) {
        LOG_ERROR("snd_pcm_sw_params_current failed: %s",
                  snd_strerror(ret));
        return ret;
    }

    if (auto_start) {
        ret = snd_pcm_sw_params_set_start_threshold(mgr->pcm,
                                                    sw_params,
                                                    buffer_size);
        if (ret < 0) {
            LOG_ERROR("set_start_threshold failed: %s",
                      snd_strerror(ret));
            return ret;
        }
    } else {
        /* start manually -> threshold = buffer_size + 1 */
        ret = snd_pcm_sw_params_set_start_threshold(mgr->pcm,
                                                    sw_params,
                                                    buffer_size + 1);
        if (ret < 0) {
            LOG_ERROR("set_start_threshold failed: %s",
                      snd_strerror(ret));
            return ret;
        }
    }

    ret = snd_pcm_sw_params_set_avail_min(mgr->pcm,
                                          sw_params,
                                          period_size);
    if (ret < 0) {
        LOG_ERROR("set_avail_min failed: %s", snd_strerror(ret));
        return ret;
    }

    ret = snd_pcm_sw_params(mgr->pcm, sw_params);
    if (ret < 0) {
        LOG_ERROR("snd_pcm_sw_params apply failed: %s",
                  snd_strerror(ret));
        return ret;
    }

    LOG_DEBUG("SW params applied: auto_start=%d threshold=%lu avail_min=%lu",
              auto_start,
              (unsigned long)(auto_start ? buffer_size : buffer_size + 1),
              (unsigned long)period_size);

    return 0;
}

/*
 * Main API: play a WAV map through the audio manager.
 * skip_format_check = true will bypass format validation for performance.
 */
int32_t audio_play_wav_map(struct audio_mgr *mgr, const struct wav_map *wm, \
                           bool skip_format_check)
{
    int32_t ret = 0;
    size_t frame_size;
    size_t total_frames;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;

    if (!mgr || !wm)
        return AUDIO_E_INVAL;

    ret = validate_and_reinit(mgr, wm, false);
    if (ret < 0)
        return ret;

    frame_size = mgr->frame_size;
    if (frame_size == 0)
        return AUDIO_E_INVAL;

    total_frames = wm->data_size / frame_size;

    ret = audio_mgr_prepare(mgr);
    if (ret < 0)
        return ret;


    // (void)set_sw_params(mgr);
    /* Read buffer_size and period_size from PCM driver */
    ret = snd_pcm_get_params(mgr->pcm, &buffer_size, &period_size);
    if (ret < 0)
        return ret;

    /* Apply software params: start_threshold, avail_min */
    ret = audio_mgr_apply_sw_params(mgr, buffer_size,
                                    period_size, true);
    if (ret < 0)
        return ret;

    ret = playback_loop(mgr, wm, frame_size, total_frames);
    if (ret < 0)
        return ret;

    snd_pcm_drain(mgr->pcm);
    return AUDIO_OK;
}

/* convenience: open->play->close */
int32_t audio_play_wav_file(struct audio_mgr *mgr, const char *path)
{
    struct wav_map wm;
    int32_t ret;

    ret = wav_map_open(path, &wm);
    if (ret < 0) return ret;

    ret = audio_play_wav_map(mgr, &wm, mgr->skip_format_check);
    wav_map_close(&wm);
    return ret;
}
