/**
 * @file imu.c
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include "comm/f_comm.h"
#include "hw/common.h"
#include "hw/imu.h"
#include "kalman.h"
#include "main.h"

/*********************
 *      DEFINES
 *********************/
#define SYSFS_PATH_MAX 256
#define CALIB_SAMPLES 300
#define DEFAULT_SAMPLE_HZ 100
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**********************
 *      MACROS
 **********************/
#define RAD2DEG (180.0f / (float)M_PI)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static const char *DEFAULT_IIO_BASE = "/sys/bus/iio/devices/iio:device0/";

/* module state */
static char iio_base[SYSFS_PATH_MAX];
static int32_t sample_hz = DEFAULT_SAMPLE_HZ;

/* Kalman filters */
static struct kalman k_roll, k_pitch;
static float yaw_integral = 0.0f; /* degrees */

/* thread + sync */
// static int32_t imu_running = 0;
static pthread_mutex_t angles_lock = PTHREAD_MUTEX_INITIALIZER;
static struct imu_angles shared_angles = {0.0f, 0.0f, 0.0f};

/* offsets (stored in final units: accel -> g, gyro -> deg/s)
 * NOTE: accel offsets chosen so that after subtracting offsets:
 *   axc = ax - ax_off
 *   ayc = ay - ay_off
 *   azc = az - az_off  (we will use az_off = mean_az - 1.0 so azc≈1 when static)
 */
static struct {
    float ax_off;
    float ay_off;
    float az_off;
    float gx_off;
    float gy_off;
    float gz_off;
} offs;

/* scales read from sysfs (raw LSB -> physical unit)
 * accel_scale: multiply raw -> ??? (either g or m/s^2 depending on driver)
 * gyro_scale: multiply raw -> ??? (either rad/s or deg/s)
 *
 * We will auto-detect and convert to desired final units:
 *   accel_to_g_factor: multiply earlier result to get g (1g = 9.80665 m/s^2)
 *   gyro_to_deg_factor: multiply earlier result to get deg/s
 */
static struct {
    float accel_scale;
    int32_t have_accel_scale;
    float gyro_scale;
    int32_t have_gyro_scale;

    /* conversion factors decided at calibration */
    float accel_to_g_factor;  /* multiply scaled accel to get g */
    float gyro_to_deg_factor; /* multiply scaled gyro to get deg/s */
} scales;

/* mount matrices (row-major 3x3). If not present -> identity */
static float accel_mount[9];
static float gyro_mount[9];

/* adaptive params */
static const float IDLE_GYRO_THRESHOLD = 1.0f;   /* deg/s threshold to consider idle */
static const float IDLE_ACC_MAG_TOL    = 0.03f;  /* g tolerance around 1.0 */
static const float BIAS_ADAPT_ALPHA    = 0.0008f;/* slow adapt rate */

/**********************
 *   STATIC FUNCTIONS
 **********************/
static int32_t read_sysfs_float_file(const char *path, float *out)
{
    FILE *f;
    double tmp;
    int32_t ret = -1;
    if (!path || !out) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%lf", &tmp) == 1) {
        *out = (float)tmp;
        ret = 0;
    } else ret = -1;
    fclose(f);
    return ret;
}

static int32_t read_sysfs_name(const char *name, float *out)
{
    char path[SYSFS_PATH_MAX];
    if (!name || !out) return -1;
    snprintf(path, sizeof(path), "%s%s", iio_base, name);
    return read_sysfs_float_file(path, out);
}

/* read mount matrix: file contains 9 numbers (row-major), or return identity */
static void load_mount_matrix(const char *name, float m_out[9])
{
    char path[SYSFS_PATH_MAX];
    snprintf(path, sizeof(path), "%s%s", iio_base, name);

    FILE *f = fopen(path, "r");
    if (!f) {
        /* identity */
        m_out[0]=1; m_out[1]=0; m_out[2]=0;
        m_out[3]=0; m_out[4]=1; m_out[5]=0;
        m_out[6]=0; m_out[7]=0; m_out[8]=1;
        LOG_DEBUG("mount matrix %s not found, using identity", name);
        return;
    }

    int32_t cnt = 0;
    for (int32_t i = 0; i < 9; i++) {
        double tmp;
        if (fscanf(f, "%lf", &tmp) == 1) {
            m_out[cnt++] = (float)tmp;
        } else break;
    }
    fclose(f);

    if (cnt != 9) {
        /* fallback to identity */
        m_out[0]=1; m_out[1]=0; m_out[2]=0;
        m_out[3]=0; m_out[4]=1; m_out[5]=0;
        m_out[6]=0; m_out[7]=0; m_out[8]=1;
        LOG_WARN("mount matrix %s incomplete (%d values) -> using identity", name, cnt);
    } else {
        LOG_INFO("loaded mount matrix %s", name);
    }
}

/* apply 3x3 matrix (row-major) to vector in[3] -> out[3] */
static void apply_mount(const float m[9], const float in[3], float out[3])
{
    out[0] = m[0]*in[0] + m[1]*in[1] + m[2]*in[2];
    out[1] = m[3]*in[0] + m[4]*in[1] + m[5]*in[2];
    out[2] = m[6]*in[0] + m[7]*in[1] + m[8]*in[2];
}

/* detect scales (sysfs). Called at init */
static void detect_scales(void)
{
    float v;
    scales.have_accel_scale = 0;
    scales.have_gyro_scale = 0;
    scales.accel_scale = 1.0f;
    scales.gyro_scale  = 1.0f;
    scales.accel_to_g_factor = 1.0f; /* unknown yet; will be set in calibration */
    scales.gyro_to_deg_factor = 1.0f;

    if (read_sysfs_name("in_accel_scale", &v) == 0) {
        scales.accel_scale = v;
        scales.have_accel_scale = 1;
        LOG_INFO("detected in_accel_scale=%.9f", scales.accel_scale);
    } else {
        LOG_WARN("in_accel_scale not found; assuming accel raw*scale already in g or units you expect");
    }

    if (read_sysfs_name("in_anglvel_scale", &v) == 0) {
        scales.gyro_scale = v;
        scales.have_gyro_scale = 1;
        LOG_INFO("detected in_anglvel_scale=%.9f", scales.gyro_scale);
    } else {
        LOG_WARN("in_anglvel_scale not found; assuming gyro raw*scale already in deg/s");
    }
}

/*
 * read_raw_scaled_no_unit_convert:
 *  - read raw values and multiply by per-axis scale from sysfs
 *  - apply mount matrix
 *  - DO NOT convert accel to g or gyro to deg/s here (we detect units in calibration)
 *  returns 0 on success
 */
static int32_t read_raw_scaled_no_unit_convert(float *ax, float *ay, float *az,
                       float *gx, float *gy, float *gz)
{
    float v;
    float a_raw[3] = {0.0f,0.0f,0.0f};
    float g_raw[3] = {0.0f,0.0f,0.0f};
    int32_t ret;

    /* accel */
    ret = read_sysfs_name("in_accel_x_raw", &v);
    a_raw[0] = (ret==0) ? v * scales.accel_scale : 0.0f;
    ret = read_sysfs_name("in_accel_y_raw", &v);
    a_raw[1] = (ret==0) ? v * scales.accel_scale : 0.0f;
    ret = read_sysfs_name("in_accel_z_raw", &v);
    a_raw[2] = (ret==0) ? v * scales.accel_scale : 0.0f;

    /* gyro */
    ret = read_sysfs_name("in_anglvel_x_raw", &v);
    g_raw[0] = (ret==0) ? v * scales.gyro_scale : 0.0f;
    ret = read_sysfs_name("in_anglvel_y_raw", &v);
    g_raw[1] = (ret==0) ? v * scales.gyro_scale : 0.0f;
    ret = read_sysfs_name("in_anglvel_z_raw", &v);
    g_raw[2] = (ret==0) ? v * scales.gyro_scale : 0.0f;

    /* apply mount matrices */
    float a_rot[3], g_rot[3];
    apply_mount(accel_mount, a_raw, a_rot);
    apply_mount(gyro_mount, g_raw, g_rot);

    /* return */
    *ax = a_rot[0]; *ay = a_rot[1]; *az = a_rot[2];
    *gx = g_rot[0]; *gy = g_rot[1]; *gz = g_rot[2];

    return 0;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
/*
 * Public read: returns accel in g and gyro in deg/s
 * If calibration hasn't set accel_to_g_factor/gyro_to_deg_factor yet, values may be raw*scale
 */
int32_t imu_kalman_read_raw(float *ax, float *ay, float *az,
            float *gx, float *gy, float *gz)
{
    float a0,a1,a2,g0,g1,g2;
    if (read_raw_scaled_no_unit_convert(&a0,&a1,&a2,&g0,&g1,&g2) != 0)
        return -1;

    /* convert accel -> g if needed */
    a0 *= scales.accel_to_g_factor;
    a1 *= scales.accel_to_g_factor;
    a2 *= scales.accel_to_g_factor;

    /* convert gyro -> deg/s if needed */
    g0 *= scales.gyro_to_deg_factor;
    g1 *= scales.gyro_to_deg_factor;
    g2 *= scales.gyro_to_deg_factor;

    *ax = a0; *ay = a1; *az = a2;
    *gx = g0; *gy = g1; *gz = g2;
    return 0;
}

/* ---------- calibration implementation ---------- */
int32_t imu_kalman_calibrate(void)
{
    int32_t i, ret;
    double ax_sum = 0.0, ay_sum = 0.0, az_sum = 0.0;
    double gx_sum = 0.0, gy_sum = 0.0, gz_sum = 0.0;
    double mag_sum = 0.0;
    float axs, ays, azs, gxs, gys, gzs;

    if (get_ctx()->cfg.imu_en) {
        LOG_WARN("calibrate: cannot calibrate while running");
        return -1;
    }

    LOG_INFO("imu_kalman_calibrate: keep device still for %d samples", CALIB_SAMPLES);

    /* collect samples using raw scaled values (not yet converted to final units) */
    for (i = 0; i < CALIB_SAMPLES; i++) {
        ret = read_raw_scaled_no_unit_convert(&axs, &ays, &azs, &gxs, &gys, &gzs);
        if (ret != 0) {
            LOG_ERROR("calibrate: read_raw_scaled_no_unit_convert failed");
            return -1;
        }
        ax_sum += axs; ay_sum += ays; az_sum += azs;
        gx_sum += gxs; gy_sum += gys; gz_sum += gzs;
        mag_sum += sqrtf(axs*axs + ays*ays + azs*azs);
        usleep(5000);
    }

    /* means in 'scaled' units */
    double mean_ax = ax_sum / CALIB_SAMPLES;
    double mean_ay = ay_sum / CALIB_SAMPLES;
    double mean_az = az_sum / CALIB_SAMPLES;
    double mean_gx = gx_sum / CALIB_SAMPLES;
    double mean_gy = gy_sum / CALIB_SAMPLES;
    double mean_gz = gz_sum / CALIB_SAMPLES;
    double mean_mag = mag_sum / CALIB_SAMPLES;

    /* Decide accel unit: if magnitude >> 2 -> likely m/s^2 (gravity ~9.8) */
    if (mean_mag > 2.0) {
        /* scaled accel currently in m/s^2 -> convert to g */
        scales.accel_to_g_factor = 1.0f / 9.80665f;
        LOG_INFO("calibrate: accel appears to be in m/s^2 (mag~%.3f) -> converting to g", mean_mag);
    } else {
        /* likely already in g */
        scales.accel_to_g_factor = 1.0f;
        LOG_INFO("calibrate: accel appears to be in g (mag~%.3f)", mean_mag);
    }

    /* Decide gyro unit: if gyro_scale present and small -> rad/s */
    if (scales.have_gyro_scale && scales.gyro_scale < 0.02f) {
        scales.gyro_to_deg_factor = RAD2DEG;
        LOG_INFO("calibrate: gyro appears to be in rad/s (scale %.9f) -> converting to deg/s", scales.gyro_scale);
    } else {
        /* assume deg/s already */
        scales.gyro_to_deg_factor = 1.0f;
        LOG_INFO("calibrate: gyro assumed in deg/s (scale %.9f)", scales.gyro_scale);
    }

    /* apply conversions to means */
    float mean_ax_g = (float)(mean_ax * scales.accel_to_g_factor);
    float mean_ay_g = (float)(mean_ay * scales.accel_to_g_factor);
    float mean_az_g = (float)(mean_az * scales.accel_to_g_factor);
    float mean_gx_deg = (float)(mean_gx * scales.gyro_to_deg_factor);
    float mean_gy_deg = (float)(mean_gy * scales.gyro_to_deg_factor);
    float mean_gz_deg = (float)(mean_gz * scales.gyro_to_deg_factor);

    /* store offsets: note az_off subtracts gravity baseline so later azc ≈ 1 when static */
    offs.ax_off = mean_ax_g;
    offs.ay_off = mean_ay_g;
    offs.az_off = mean_az_g - 1.0f;

    offs.gx_off = mean_gx_deg;
    offs.gy_off = mean_gy_deg;
    offs.gz_off = mean_gz_deg;

    LOG_INFO("calibrate done ax_mean=%.6f ay_mean=%.6f az_mean=%.6f mag_mean=%.4f",
         mean_ax_g, mean_ay_g, mean_az_g, mean_mag * scales.accel_to_g_factor);
    LOG_INFO("calibrate done gx_mean=%.6f gy_mean=%.6f gz_mean=%.6f",
         mean_gx_deg, mean_gy_deg, mean_gz_deg);

    /* initialize Kalman filters using the mean orientation */
    {
        float roll_init  = atan2f(mean_ay_g, mean_az_g) * RAD2DEG;
        float pitch_init = atan2f(-mean_ax_g, sqrtf(mean_ay_g*mean_ay_g + mean_az_g*mean_az_g)) * RAD2DEG;

        kalman_reset(&k_roll, roll_init);
        kalman_reset(&k_pitch, pitch_init);

        pthread_mutex_lock(&angles_lock);
        shared_angles.roll = roll_init;
        shared_angles.pitch = pitch_init;
        shared_angles.yaw = 0.0f;
        pthread_mutex_unlock(&angles_lock);

        yaw_integral = 0.0f;

        LOG_INFO("calibrate init roll=%.3f pitch=%.3f", roll_init, pitch_init);
    }

    return 0;
}

/* ---------- adaptive r factor ---------- */
static float compute_adaptive_r_factor(float accel_mag_g, float innovation_deg)
{
    float mag_dev = fabsf(accel_mag_g - 1.0f);
    float mag_factor = 1.0f + (mag_dev / 0.6f) * 4.0f;
    if (mag_factor < 1.0f) mag_factor = 1.0f;

    float inn_factor = 1.0f;
    if (innovation_deg > 5.0f)
        inn_factor = 1.0f + (innovation_deg - 5.0f) / 20.0f;

    return mag_factor * inn_factor;
}

/* ---------- IMU thread ---------- */
static int32_t imu_fn_handler()
{
    struct timespec t_prev, t_now;
    float axs, ays, azs, gxs, gys, gzs;
    float axc, ayc, azc, gxc, gyc, gzc;
    float roll_acc, pitch_acc;
    float roll_f, pitch_f;
    float dt;
    int32_t sleep_us;
    float last_roll = 0.0f, last_pitch = 0.0f;

    LOG_INFO("start path=%s hz=%d", iio_base, sample_hz);

    clock_gettime(CLOCK_MONOTONIC, &t_prev);
    sleep_us = (int)(1000000.0f / (float)sample_hz);

    while (get_ctx()->cfg.imu_en && get_ctx()->run) {
        /* read raw scaled (no unit conversion), then convert here */
        if (read_raw_scaled_no_unit_convert(&axs, &ays, &azs, &gxs, &gys, &gzs) != 0) {
            LOG_ERROR("read_raw_scaled_no_unit_convert failed");
            usleep(sleep_us);
            continue;
        }

        /* convert to final units (g and deg/s) */
        float ax = axs * scales.accel_to_g_factor;
        float ay = ays * scales.accel_to_g_factor;
        float az = azs * scales.accel_to_g_factor;

        float gx = gxs * scales.gyro_to_deg_factor;
        float gy = gys * scales.gyro_to_deg_factor;
        float gz = gzs * scales.gyro_to_deg_factor;

        /* apply offsets (calibration) */
        axc = ax - offs.ax_off;
        ayc = ay - offs.ay_off;
        azc = az - offs.az_off;

        gxc = gx - offs.gx_off;
        gyc = gy - offs.gy_off;
        gzc = gz - offs.gz_off;

        /* dt */
        clock_gettime(CLOCK_MONOTONIC, &t_now);
        dt = (t_now.tv_sec - t_prev.tv_sec) + (t_now.tv_nsec - t_prev.tv_nsec) / 1e9f;
        if (dt <= 0.0f || dt > 1.0f) dt = 1.0f / (float)sample_hz;
        t_prev = t_now;

        /* clamp gyro spikes */
        if (fabsf(gxc) > 2000.0f) gxc = copysignf(2000.0f, gxc);
        if (fabsf(gyc) > 2000.0f) gyc = copysignf(2000.0f, gyc);
        if (fabsf(gzc) > 2000.0f) gzc = copysignf(2000.0f, gzc);

        /* accel magnitude (g) */
        float mag = sqrtf(axc*axc + ayc*ayc + azc*azc);

        /* dynamic gyro bias adaptation when device idle */
        if (fabsf(gxc) < IDLE_GYRO_THRESHOLD &&
            fabsf(gyc) < IDLE_GYRO_THRESHOLD &&
            fabsf(gzc) < IDLE_GYRO_THRESHOLD &&
            fabsf(mag - 1.0f) < IDLE_ACC_MAG_TOL) {
            /* adapt biases slowly toward latest raw (pre-conversion) gyro values */
            /* Note: adapt using converted deg/s values (gx,gy,gz) */
            offs.gx_off = offs.gx_off * (1.0f - BIAS_ADAPT_ALPHA) + gx * BIAS_ADAPT_ALPHA;
            offs.gy_off = offs.gy_off * (1.0f - BIAS_ADAPT_ALPHA) + gy * BIAS_ADAPT_ALPHA;
            offs.gz_off = offs.gz_off * (1.0f - BIAS_ADAPT_ALPHA) + gz * BIAS_ADAPT_ALPHA;
        }

        /* compute accel-based angles (deg) */
        roll_acc  = atan2f(ayc, azc) * RAD2DEG;
        pitch_acc = atan2f(-axc, sqrtf(ayc*ayc + azc*azc)) * RAD2DEG;

        /* innovation estimate */
        float inn_roll  = fabsf(roll_acc  - last_roll);
        float inn_pitch = fabsf(pitch_acc - last_pitch);

        /* adaptive r multiplier */
        float rfac_roll  = compute_adaptive_r_factor(mag, inn_roll);
        float rfac_pitch = compute_adaptive_r_factor(mag, inn_pitch);

        /* temporarily scale r_measure for robustness */
        float orig_r_roll  = k_roll.r_measure;
        float orig_r_pitch = k_pitch.r_measure;

        k_roll.r_measure  = orig_r_roll  * rfac_roll;
        k_pitch.r_measure = orig_r_pitch * rfac_pitch;

        /* Kalman update (gyro in deg/s, accel angles in deg) */
        roll_f  = kalman_update(&k_roll,  roll_acc,  gxc, dt);
        pitch_f = kalman_update(&k_pitch, pitch_acc, gyc, dt);

        /* restore r_measure */
        k_roll.r_measure  = orig_r_roll;
        k_pitch.r_measure = orig_r_pitch;

        /* integrate yaw */
        yaw_integral += gzc * dt;

        /* publish */
        pthread_mutex_lock(&angles_lock);
        shared_angles.roll  = roll_f;
        shared_angles.pitch = pitch_f;
        shared_angles.yaw   = yaw_integral;
        pthread_mutex_unlock(&angles_lock);

        last_roll = roll_f;
        last_pitch = pitch_f;

        LOG_TRACE("dt=%.4f ax=%.4f ay=%.4f az=%.4f gx=%.4f gy=%.4f gz=%.4f mag=%.3f",
                  dt, axc, ayc, azc, gxc, gyc, gzc, mag);
        LOG_TRACE("acc_roll=%.3f acc_pitch=%.3f kal_roll=%.3f kal_pitch=%.3f yaw=%.3f rfac=%.3f/%.3f inn=%.3f/%.3f",
                  roll_acc, pitch_acc, roll_f, pitch_f, yaw_integral, rfac_roll, rfac_pitch, inn_roll, inn_pitch);

        usleep((int)(1000000.0f / (float)sample_hz));
    }

    return 0;
}

/* ---------- Public API ---------- */

int32_t imu_kalman_init(const char *path, int32_t hz, float q_angle, float q_bias, float r_measure)
{
    int32_t ret;

    if (!path) path = DEFAULT_IIO_BASE;

    /* copy base path and ensure trailing slash */
    strncpy(iio_base, path, sizeof(iio_base)-1);
    iio_base[sizeof(iio_base)-1] = '\0';
    if (iio_base[strlen(iio_base)-1] != '/') {
        if (strlen(iio_base) + 1 < sizeof(iio_base))
            strncat(iio_base, "/", sizeof(iio_base) - strlen(iio_base) - 1);
    }

    if (hz <= 0) hz = DEFAULT_SAMPLE_HZ;
    sample_hz = hz;

    /* init kalman */
    kalman_init(&k_roll, q_angle, q_bias, r_measure);
    kalman_init(&k_pitch, q_angle, q_bias, r_measure);
    yaw_integral = 0.0f;

    /* zero offs and scales */
    memset(&offs, 0, sizeof(offs));
    memset(&scales, 0, sizeof(scales));
    scales.accel_scale = 1.0f;
    scales.gyro_scale  = 1.0f;
    scales.accel_to_g_factor = 1.0f;
    scales.gyro_to_deg_factor = 1.0f;

    /* detect scales and mount matrices */
    detect_scales();
    load_mount_matrix("in_accel_mount_matrix", accel_mount);
    load_mount_matrix("in_anglvel_mount_matrix", gyro_mount);

    /* quick check base path accessibility */
    ret = access(iio_base, R_OK);
    if (ret != 0) {
        LOG_ERROR("iio base %s not accessible (%s)", iio_base, strerror(errno));
        return -1;
    }

    LOG_INFO("imu_kalman_init path=%s hz=%d q_angle=%.6f q_bias=%.6f r_measure=%.6f",
         iio_base, sample_hz, k_roll.q_angle, k_roll.q_bias, k_roll.r_measure);

    /* initial calibration (blocking) */
    ret = imu_kalman_calibrate();
    if (ret != 0) {
        LOG_WARN("initial calibration failed");
        /* continue, but offsets may be zero */
    }

    return 0;
}
int32_t enable_imu_fn()
{
    int32_t ret;
    char dev_path[MAX_PATH_LEN];
    pthread_t imu_state_handler;

    if (get_ctx()->cfg.imu_en) {
        LOG_WARN("imu already running");
        return 0;
    }

    ret = iio_dev_get_path_by_name(IMU_SENSOR_NAME, dev_path, sizeof(dev_path));
    if (ret) {
        return ret;
    }

    // TODO: load exist configuration
    ret = imu_kalman_init(dev_path, 100, 0.001f, 0.003f, 0.03f);
    if (ret) {
        LOG_ERROR("IMU init task has failed (%d)", ret);
    }


    get_ctx()->cfg.imu_en = true;

    ret = pthread_create(&imu_state_handler, NULL, imu_fn_handler, NULL);
    if (ret) {
        LOG_FATAL("Failed to create IMU monitor thread: %s", strerror(ret));
        get_ctx()->cfg.imu_en = false;
        return -ENOMEM;
    }

    return ret;
}

void disable_imu_fn(void)
{
    if (!get_ctx()->cfg.imu_en) {
        LOG_WARN("imu not enable");
        return;
    }

    get_ctx()->cfg.imu_en = false;
}

bool is_imu_enabled(void)
{
    return get_ctx()->cfg.imu_en;
}

void imu_kalman_reset_yaw(float yaw_deg)
{
    pthread_mutex_lock(&angles_lock);
    if (isnan(yaw_deg))
        shared_angles.yaw = 0.0f;
    else
        shared_angles.yaw = yaw_deg;
    yaw_integral = shared_angles.yaw;
    pthread_mutex_unlock(&angles_lock);
    LOG_INFO("imu_kalman_reset_yaw -> %.3f deg", shared_angles.yaw);
}

struct imu_angles imu_get_angles(void)
{
    struct imu_angles out;
    pthread_mutex_lock(&angles_lock);
    out = shared_angles;
    pthread_mutex_unlock(&angles_lock);
    return out;
}

void imu_kalman_set_tuning(float q_angle, float q_bias, float r_measure)
{
    if (q_angle > 0.0f) k_roll.q_angle = k_pitch.q_angle = q_angle;
    if (q_bias  > 0.0f) k_roll.q_bias  = k_pitch.q_bias  = q_bias;
    if (r_measure> 0.0f) k_roll.r_measure = k_pitch.r_measure = r_measure;

    LOG_INFO("imu_kalman_set_tuning q_angle=%.6f q_bias=%.6f r_measure=%.6f",
         k_roll.q_angle, k_roll.q_bias, k_roll.r_measure);
}
