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
    ret = gf_fs_file_exists(f_path);
    if (ret < 0) {
        return ret;
    } else {
        snprintf(w_value, sizeof(w_value), "%f", 0.1);
        ret = gf_fs_write_file(f_path, w_value, sizeof(w_value));
        if (ret) {
            LOG_ERROR("ALS sample configure failed, ret %d", ret);
        } else {

            LOG_INFO("ALS sample configured as %s, ret %d", w_value, ret);
        }
    }

    return 0;
}

int32_t als_read_illuminance(const char *dev_path)
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
    ret = gf_fs_file_exists(f_path);
    if (ret < 0) {
        return ret;
    } else {
        ret = gf_fs_read_file(f_path, r_buff, sizeof(r_buff), &read_len);
        if (ret) {
            LOG_ERROR("ALS read failed, ret %d", ret);
        } else {

            LOG_INFO("ALS current value %s, ret %d", r_buff, ret);
        }
    }

    return 0;
}
