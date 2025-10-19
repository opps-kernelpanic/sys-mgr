/**
 * @file backlight.c
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "comm/f_comm.h"
#include "comm/cmd_payload.h"
#include "sched/workqueue.h"
#include "hw/common.h"

/*********************
 *      DEFINES
 *********************/
#define FS_BRIGHTNESS               "/sys/class/backlight/backlight/brightness"
#define FS_ACTUAL_BRIGHTNESS        "/sys/class/backlight/backlight/actual_brightness"
#define FS_BRIGHTNESS_POWER         "/sys/class/backlight/backlight/bl_power"

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
static int32_t res_actual_brightness_value(int32_t value)
{
    remote_cmd_t *cmd;
    int32_t ret = 0;

    cmd = create_remote_task_data(WORK_PRIO_NORMAL, WORK_DURATION_SHORT, \
                                  OP_GET_BRIGHTNESS);
    if (!cmd) {
        LOG_ERROR("Failed to create remote command payload");
        return -EINVAL;
    }

    ret = remote_cmd_add_int(cmd, "brightness", value);
    if (ret) {
        delete_remote_cmd(cmd);
        return -EIO;
    }

    // NOTE: Command data will be released after the work completes
    ret = create_remote_task(WORK_PRIO_HIGH, cmd);
    return ret;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int32_t backlight_setup()
{
    int32_t ret;
    char str[10];

    ret = fs_file_exists(FS_BRIGHTNESS_POWER);
    if (ret < 0) {
        return ret;
    } else {
        sprintf(str, "%d", 0);
        ret = fs_write_file(FS_BRIGHTNESS_POWER, str, sizeof(str));
    }

    return ret;
}

int32_t set_brightness(uint8_t brightness)
{
    char str_percent[4];
    int32_t ret = 0;

    /* Workaround: kernel cannot set backlight to zero */
    if (brightness == 0) {
        brightness = 1;
    }

    snprintf(str_percent, sizeof(str_percent), "%u", brightness);

    ret = fs_write_file(FS_BRIGHTNESS, str_percent, sizeof(str_percent));
    if (ret < 0) {
        LOG_ERROR("Failed to set brightness to %u, err=%d (%s)", brightness, \
                  ret, strerror(errno));
        return ret;
    }

    LOG_TRACE("Brightness set to %u", brightness);
    return 0;
}

int32_t brightness_ramp(uint8_t from, uint8_t to, uint32_t period_us)
{
    uint32_t step_us;
    uint8_t step;
    int32_t ret = 0;

    if (from == to)
        return set_brightness(to);

    step = (from > to) ? (from - to) : (to - from);
    if (step == 0) {
        LOG_ERROR("Invalid brightness step: from=%u, to=%u", from, to);
        return -EINVAL;
    }

    step_us = period_us / step;

    if (from < to) {
        for (uint8_t i = from; i <= to; i++) {
            ret = set_brightness(i);
            if (ret < 0)
                return ret;
            usleep(step_us);
        }
    } else {
        for (uint8_t i = from; i >= to; i--) {
            ret = set_brightness(i);
            if (ret < 0)
                return ret;
            usleep(step_us);
            if (i == 0)
                break; /* Prevent underflow */
        }
    }

    LOG_DEBUG("Brightness ramp done: from=%u to=%u period=%u us", \
              from, to, period_us);
    return 0;
}

int32_t get_brightness(int32_t *out_val)
{
    char r_buff[10];
    size_t read_len;
    int ret;

    ret = fs_file_exists(FS_ACTUAL_BRIGHTNESS);
    if (ret < 0) {
        return ret;
    } else {
        ret = fs_read_file(FS_ACTUAL_BRIGHTNESS, r_buff, sizeof(r_buff), &read_len);
        if (ret) {
            LOG_ERROR("Actual brightness read failed, ret %d", ret);
        } else {
            *out_val = atoi(r_buff);
        }
    }

    return ret;
}

int32_t get_and_res_actual_brightness()
{
    int32_t brightness_val;
    int ret;

    ret = get_brightness(&brightness_val);
    if (ret < 0) {
        return ret;
    } else {
        LOG_TRACE("Actual brightness value %d, ret %d", brightness_val, ret);
        ret = res_actual_brightness_value(brightness_val);
    }

    return ret;
}

