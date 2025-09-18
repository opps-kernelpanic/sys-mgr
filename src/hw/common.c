/**
 * @file common.c
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
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "comm/f_comm.h"
#include "hw/common.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/
extern volatile sig_atomic_t g_run;

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
int32_t common_hw_init()
{
    backlight_setup();

    char dev_path[MAX_PATH_LEN];
    als_late_init(ALS_SENSOR_NAME, dev_path, sizeof(dev_path));
    als_read_illuminance(dev_path);
}

int32_t common_hw_deinit()
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
    LOG_INFO("Hardware monitor is running...");
    while (g_run) {
        usleep(200000);
    }
    LOG_INFO("Hardware monitor thread exiting...");

    return NULL;
}
