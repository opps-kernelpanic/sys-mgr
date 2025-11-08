/**
 * @file als.c
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

#include <comm/f_comm.h>
#include <hw/common.h>

/*********************
 *      DEFINES
 *********************/
#define ALS_SAMPLE_TIME_CFG             "in_illuminance_integration_time"
#define ALS_VALUE                       "in_illuminance_input"

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
#define clamp(val, min, max) \
    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int32_t als_late_init(const char *sensor_name, char *dev_path, size_t path_len)
{
    char w_value[10];
    char f_path[128];
    int32_t ret;

    if (!sensor_name) {
        LOG_ERROR("Invalid argument\n");
        return -EINVAL;
    }

    ret = iio_dev_get_path_by_name(sensor_name, dev_path, path_len);
    if (ret) {
        return ret;
    }

    snprintf(f_path, sizeof(f_path), "%s/%s", dev_path, ALS_SAMPLE_TIME_CFG);
    ret = fs_file_exists(f_path);
    if (ret < 0) {
        return ret;
    } else {
        snprintf(w_value, sizeof(w_value), "%f", 0.1);
        ret = fs_write_file(f_path, w_value, sizeof(w_value));
        if (ret) {
            LOG_ERROR("ALS sample configure failed, ret %d", ret);
        } else {

            LOG_INFO("ALS sample configured as %s, ret %d", w_value, ret);
        }
    }

    return 0;
}

int32_t als_read_illuminance(const char *dev_path, int32_t *out_val)
{
    char r_buff[10];
    char f_path[128];
    size_t read_len;
    int ret;

    if (!dev_path) {
        LOG_ERROR("Invalid argument\n");
        return -EINVAL;
    }

    snprintf(f_path, sizeof(f_path), "%s/%s", dev_path, ALS_VALUE);
    ret = fs_file_exists(f_path);
    if (ret < 0) {
        return ret;
    } else {
        ret = fs_read_file(f_path, r_buff, sizeof(r_buff), &read_len);
        if (ret) {
            LOG_ERROR("ALS read failed, ret %d", ret);
        } else {
            *out_val = atoi(r_buff);
            LOG_TRACE("ALS current value %d, ret %d", *out_val, ret);
        }
    }

    return 0;
}

int32_t handle_auto_brightness(const char *dev_path)
{
    int32_t ret;
    int32_t lux, brightness, target;
    const int32_t lux_max = 500;
    const int32_t pct_min = 0, pct_max = 100;
    const int32_t step_thresh = 5;

    ret = als_read_illuminance(dev_path, &lux);
    if (ret)
        return ret;

    /* Map illuminance (lux) to brightness percent */
    target = (lux * pct_max) / lux_max;
    target = clamp(target, pct_min, pct_max);

    LOG_TRACE("ALS: %d lux -> target %d%%", lux, target);

    ret = get_brightness(&brightness);
    if (ret)
        return ret;

    /* Skip small variations to avoid flicker */
    if (abs(target - brightness) < step_thresh)
        return 0;

    ret = report_backlight_state(true, target);
    if (ret)
        LOG_WARN("Report backlight target state failed, ret %d", ret);

    /* Smoothly ramp to new brightness (500ms) */
    ret = brightness_ramp(brightness, target, 500000);
    if (ret)
        return ret;

    ret = report_backlight_state(false, 0);
    if (ret)
        LOG_WARN("Report backlight state failed, ret %d", ret);

    return 0;
}
