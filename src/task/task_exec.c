/**
 * @file task_exec.c
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

#include <stdlib.h>
#include <stdint.h>

#include "comm/dbus_comm.h"
#include "comm/cmd_payload.h"
#include "sched/workqueue.h"
#include "hw/imu.h"
#include "hw/common.h"
#include "audio/sound.h"
#include "main.h"

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
int32_t process_opcode(uint32_t opcode, void *data)
{
    int32_t ret = 0;

    switch (opcode) {
    case OP_DBUS_SENT_CMD:
        ret = dbus_method_call_with_data((remote_cmd_t *)data);
        break;
    case OP_BACKLIGHT_ON:
        ret = brightness_ramp(0, 100, 500000);
        break;
    case OP_BACKLIGHT_OFF:
        ret = brightness_ramp(100, 0, 500000);
        break;
    case OP_SET_BRIGHTNESS:
        ret = set_brightness( (*((remote_cmd_t *)data)).entries[0].value.i32);
        break;
    case OP_GET_BRIGHTNESS:
        ret = get_and_res_actual_brightness();
        break;
    case OP_LEFT_VIBRATOR:
        ret = rumble_trigger(2, 80, 150);
        break;
    case OP_RIGHT_VIBRATOR:
        ret = rumble_trigger(3, 80, 150);
        break;
    case OP_STOP_IMU:
        imu_fn_thread_stop();
        break;
    case OP_READ_IMU:
        struct imu_angles a = imu_get_angles();
        LOG_DEBUG("roll=%.2f pitch=%.2f yaw=%.2f\n", a.roll, a.pitch, a.yaw);
        break;
    case OP_AUDIO_INIT:
        ret = snd_sys_init();
        break;
    case OP_AUDIO_RELEASE:
        snd_sys_release();
        break;
    case OP_SOUND_PLAY:
        // TODO: support sound file path
        ret = audio_play_sound("/usr/share/sounds/sound-icons/percussion-10.wav");
        break;

    default:
        LOG_ERROR("Opcode [%d] is invalid", opcode);
        break;
    }

    return ret;
}

