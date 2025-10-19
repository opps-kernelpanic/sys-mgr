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

int32_t auto_brightness_handler(const char *dev_path)
{
    int32_t ret;
    int32_t light_value = 0;
    int32_t lux_min = 0, pct_min = 1;
    int32_t lux_max = 1000, pct_max = 100;
    int32_t set_pct, actual_brightness;

    ret = als_read_illuminance(dev_path, &light_value);
    if (ret)
        return ret;

    /* Map illuminance (lux) to brightness percent */
    set_pct = (light_value * pct_max) / lux_max;
    if (set_pct < pct_min)
        set_pct = pct_min;
    else if (set_pct > pct_max)
        set_pct = pct_max;

    LOG_TRACE("ALS report %d lux -> target %d%% brightness", \
              light_value, set_pct);

    ret = get_brightness(&actual_brightness);
    if (ret)
        return ret;

    /* Smooth transition to new brightness level (200ms ramp) */
    ret = brightness_ramp(actual_brightness, set_pct, 200000);
    if (ret)
        return ret;

    ret = get_and_res_actual_brightness();
    if (ret) {
        LOG_WARN("Update brightness ui failed");
    }

    return 0;
}
