/**
 * @file common.h
 *
 */
#ifndef G_COMMON_H
#define G_COMMON_H

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/
#define ALS_SENSOR_NAME                 "opt3001"
#define IMU_SENSOR_NAME                 "mpu6500"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/*=====================
 * Setter functions
 *====================*/
int32_t backlight_setup();
int32_t set_brightness(uint8_t bl_peretent);
int32_t brightness_ramp(uint8_t from, uint8_t to, uint32_t period_us);
int32_t rumble_trigger(uint32_t event_id, uint32_t ff_type, uint32_t duration);

/*=====================
 * Getter functions
 *====================*/

/*=====================
 * Other functions
 *====================*/
int32_t als_late_init(const char *sensor_name, char *dev_path, size_t path_len);
int32_t als_read_illuminance(const char *dev_path);

int32_t common_hw_init();
int32_t common_hw_deinit();
void *hw_monitor_loop();
/**********************
 *      MACROS
 **********************/

#endif /* G_COMMON_H */
