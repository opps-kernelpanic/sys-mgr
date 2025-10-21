/**
 * @file imu.h
 *
 */
#ifndef G_IMU_H
#define G_IMU_H

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/
struct imu_angles {
    float roll;   /* degrees */
    float pitch;  /* degrees */
    float yaw;    /* degrees */
};

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

/*=====================
 * Getter functions
 *====================*/

/*=====================
 * Other functions
 *====================*/
/*
 * Initialize module.
 *  - path: sysfs base (must end with '/'), e.g. "/sys/bus/iio/devices/iio:device1/"
 *          if NULL -> "/sys/bus/iio/devices/iio:device0/"
 *  - sample_hz: desired sampling frequency (Hz)
 *  - q_angle,q_bias,r_measure: Kalman tuning parameters (typical 0.001,0.003,0.03)
 * Returns 0 on success, negative on error.
 */
int32_t imu_kalman_init(const char *path, int32_t sample_hz,
            float q_angle, float q_bias, float r_measure);

/* Start acquisition thread (non-blocking) */
int32_t imu_kalman_start(void);

/* Stop acquisition thread (blocks until joined) */
void imu_kalman_stop(void);

/* Blocking calibration: device must be still. returns 0 on success */
int32_t imu_kalman_calibrate(void);

/* Reset yaw. If isnan(yaw_deg) -> reset to 0.0 */
void imu_kalman_reset_yaw(float yaw_deg);

/* Snapshot latest fused angles (thread-safe) */
struct imu_angles imu_get_angles(void);

/* Runtime tuning (pass >0 to change parameter) */
void imu_kalman_set_tuning(float q_angle, float q_bias, float r_measure);

/* Enable state */
bool is_imu_enabled(void);

/*
 * Low-level read: read sysfs raw -> apply scale and mount matrix and convert units.
 * Outputs: ax,ay,az in g ; gx,gy,gz in deg/s
 * Return 0 on success, negative on error.
 */
int32_t imu_kalman_read_raw(float *ax, float *ay, float *az,
            float *gx, float *gy, float *gz);

int32_t enable_imu_fn();
void disable_imu_fn(void);
/**********************
 *      MACROS
 **********************/

#endif /*  G_IMU_H */
