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
    case OP_ENA_ALS:
        get_ctx()->cfg.als_en = true;
        break;
    case OP_DIS_ALS:
        get_ctx()->cfg.als_en = false;
        break;
    case OP_ENA_BACKLIGHT:
        ret = enable_backlight();
        break;
    case OP_DIS_BACKLIGHT:
        ret = disable_backlight();
        break;
    case OP_ADJUST_BRIGHTNESS:
        ret = set_brightness( (*((remote_cmd_t *)data)).entries[0].value.i32);
        break;
    case OP_BACKLIGHT_STATE:
        ret = report_backlight_state(false, 0);
        break;
    case OP_LEFT_VIBRATOR:
        ret = rumble_trigger(2, 80, 150);
        break;
    case OP_RIGHT_VIBRATOR:
        ret = rumble_trigger(3, 80, 150);
        break;
    case OP_ENABLE_IMU:
        ret = enable_imu_fn();
        break;
    case OP_DISABLE_IMU:
        disable_imu_fn();
        break;
    case OP_IMU_STATE:
        update_imu_state();
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
    case OP_WIFI_ENABLE:
        ret = enable_wifi_device();
        break;
    case OP_WIFI_DISABLE:
        ret = disable_wifi_device();
        break;
    case OP_WIFI_STATE:
        ret = report_wifi_state();
        break;
    case OP_WIFI_AP_LIST:
        ret = report_cached_ap_list();
        break;
    default:
        LOG_ERROR("Opcode [%d] is invalid", opcode);
        break;
    }

    return ret;
}

