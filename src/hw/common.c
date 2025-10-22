/**
 * @file common.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#define LOG_LEVEL LOG_LEVEL_TRACE
#if defined(LOG_LEVEL)
#warning "LOG_LEVEL defined locally will override the global setting in this file"
#endif
#include "log.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "comm/f_comm.h"
#include "hw/common.h"
#include "hw/imu.h"
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
int32_t hw_monitor_init()
{
    pthread_t hw_state_handler;
    int32_t ret;

    backlight_setup();


    /* Create hardware monitor thread */
    ret = pthread_create(&hw_state_handler, NULL, hw_monitor_loop, NULL);
    if (ret) {
        LOG_FATAL("Failed to create hardware monitor thread: %s", strerror(ret));
        return -ENOMEM;
    }

    return 0;
}

void hw_monitor_deinit()
{
    ;
}

/*
 * System Manager must keep the hardware monitor loop running through
 * the whole service life. It periodically checks and reports hardware
 * state. This loop must be initialized by System Manager right after
 * hardware initialization is completed at service start.
 */
void *hw_monitor_loop()
{
    char dev_path[MAX_PATH_LEN];
    als_late_init(ALS_SENSOR_NAME, dev_path, sizeof(dev_path));

    LOG_INFO("Hardware monitor is running...");
    while (get_ctx()->run) {
        // TODO: create work
        if (get_ctx()->cfg.als_en == true) {
            auto_brightness_handler(dev_path);
        }
        if (get_ctx()->cfg.imu_en == true) {
            update_imu_state();

        }
        usleep(500000);
    }
    LOG_INFO("Hardware monitor thread exiting...");

    return NULL;
}
