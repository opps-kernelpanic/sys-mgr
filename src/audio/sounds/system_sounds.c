/**
 * @file system_sounds.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
// #define LOG_LEVEL LOG_LEVEL_TRACE
#if defined(LOG_LEVEL)
#warning "LOG_LEVEL defined locally will override the global setting in this file"
#endif
#include "log.h"

#include "audio/audio.h"
#include "audio/sound.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/
static struct audio_mgr mgr;

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int32_t snd_sys_init()
{
    const char *dev = "default";
    const char *sys_snd = "/usr/share/sounds/sound-icons/prompt.wav";
    struct wav_map first;
    int32_t ret;

    /* 1) mmap+parse first file to obtain its format */
    ret = wav_map_open(sys_snd, &first);
    if (ret < 0) {
        fprintf(stderr, "Failed to parse first WAV\n");
        return 1;
    }

    /* 2) initialize ALSA to match first file */
    ret = audio_mgr_init(&mgr, dev,
                first.fmt.pcm_format,
                first.fmt.channels,
                first.fmt.sample_rate,
                0, 0);
    if (ret < 0) {
        fprintf(stderr, "audio_mgr_init failed\n");
        wav_map_close(&first);
        return 1;
    }

    /* 3) options: enable auto_reinit so we automatically reinit on mismatch */
    audio_mgr_set_auto_reinit(&mgr, 1);

    /* If you are sure all files share same format and want max performance
     * (skip format checks), enable skip_format_check:
     * audio_mgr_set_skip_format_check(&mgr, 1);
     */

    /* 4) set master + per channel gain */
    audio_mgr_set_master_gain(&mgr, 0.8f);
    if (mgr.fmt.channels >= 1) audio_mgr_set_channel_gain(&mgr, 0, 1.0f);
    if (mgr.fmt.channels >= 2) audio_mgr_set_channel_gain(&mgr, 1, 1.0f);

    /* 5) play first file (we already have it mapped) */
    ret = audio_play_wav_map(&mgr, &first, false);
    wav_map_close(&first);
    if (ret < 0) {
        fprintf(stderr, "play 1 failed\n");
        audio_mgr_release(&mgr);
        return 1;
    }

    return ret < 0 ? 1 : 0;
}

void snd_sys_release()
{
    audio_mgr_release(&mgr);
}

int32_t audio_play_sound(const char *snd_file)
{
    int32_t ret;

    /*
     * play sound file by convenience function (open->play->close).
     * If second file has different format:
     * - if auto_reinit == 1: device will be reinitialized to match file
     * - if skip_format_check == 1: format check is skipped (assume match)
     * - otherwise: play fails with error
     */
    ret = audio_play_wav_file(&mgr, snd_file);
    if (ret < 0) {
        LOG_ERROR("Sound play failed: ret %d", ret);
    }

    return ret < 0 ? 1 : 0;
}
