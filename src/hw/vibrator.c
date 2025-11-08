/**
 * @file vibrator.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "hw/common.h"

/*********************
 *      DEFINES
 *********************/
#define MAX_PATH_LEN 64

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

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
static int32_t open_event_device(int32_t id)
{
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/dev/input/event%d", id);
    return open(path, O_RDWR);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int32_t rumble_trigger(uint32_t event_id, uint32_t ff_type, uint32_t duration)
{
    int32_t fd;
    struct ff_effect effect;
    struct input_event play;

    fd = open_event_device(event_id);
    if (fd < 0) {
        LOG_TRACE("open error");
        return 1;
    }

    memset(&effect, 0, sizeof(effect));
    effect.type = ff_type;
    effect.id = -1;
    effect.replay.length = duration;
    effect.replay.delay = 0;

    switch (ff_type) {
    case FF_RUMBLE:
        effect.u.rumble.strong_magnitude = 0xffff;
        effect.u.rumble.weak_magnitude = 0x8000;
        break;

    case FF_PERIODIC:
    case FF_SINE:
    case FF_SQUARE:
    case FF_TRIANGLE:
    case FF_SAW_UP:
    case FF_SAW_DOWN:
        effect.u.periodic.waveform = ff_type;
        effect.u.periodic.magnitude = 0x7fff;
        effect.u.periodic.period = 100; // ms
        effect.u.periodic.offset = 0;
        effect.u.periodic.phase = 0;
        effect.u.periodic.envelope.attack_length = 50;
        effect.u.periodic.envelope.attack_level = 0x1000;
        effect.u.periodic.envelope.fade_length = 50;
        effect.u.periodic.envelope.fade_level = 0x1000;
        break;

    case FF_CONSTANT:
        effect.u.constant.level = 0x5000;
        effect.u.constant.envelope.attack_length = 100;
        effect.u.constant.envelope.attack_level = 0x2000;
        effect.u.constant.envelope.fade_length = 100;
        effect.u.constant.envelope.fade_level = 0x2000;
        break;

    default:
        fprintf(stderr, "Unsupported ff_type: %d\n", ff_type);
        close(fd);
        return 1;
    }

    if (ioctl(fd, EVIOCSFF, &effect) < 0) {
        perror("EVIOCSFF");
        close(fd);
        return 1;
    }

    play.type = EV_FF;
    play.code = effect.id;
    play.value = 1;

    if (write(fd, &play, sizeof(play)) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    usleep(duration * 1000);

    ioctl(fd, EVIOCRMFF, effect.id);

    close(fd);
    return 0;
}
