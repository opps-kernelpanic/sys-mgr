/**
 * @file kalman.c
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

#include <string.h>

#include "kalman.h"

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
/* 1D Kalman implementation (angle + bias) */

void kalman_init(struct kalman *k, float q_angle, float q_bias, float r_measure)
{
    if (!k) return;

    k->q_angle = q_angle;
    k->q_bias = q_bias;
    k->r_measure = r_measure;

    k->angle = 0.0f;
    k->bias  = 0.0f;
    k->rate  = 0.0f;

    k->P00 = k->P01 = k->P10 = k->P11 = 0.0f;
}

void kalman_reset(struct kalman *k, float angle)
{
    if (!k) return;
    k->angle = angle;
    k->bias = 0.0f;
    k->rate = 0.0f;
    k->P00 = k->P01 = k->P10 = k->P11 = 0.0f;
}

float kalman_update(struct kalman *k, float new_angle, float new_rate, float dt)
{
    float rate;
    float S, K0, K1, y;
    float t00, t01;

    if (!k || dt <= 0.0f) return new_angle;

    /* Predict */
    rate = new_rate - k->bias;
    k->angle += dt * rate;

    /* Update error covariance */
    k->P00 += dt * (dt * k->P11 - k->P01 - k->P10 + k->q_angle);
    k->P01 -= dt * k->P11;
    k->P10 -= dt * k->P11;
    k->P11 += k->q_bias * dt;

    /* Compute Kalman gain */
    S = k->P00 + k->r_measure;
    if (S == 0.0f) S = 1e-6f;
    K0 = k->P00 / S;
    K1 = k->P10 / S;

    /* Update with measurement */
    y = new_angle - k->angle;
    k->angle += K0 * y;
    k->bias  += K1 * y;

    /* Update covariance */
    t00 = k->P00;
    t01 = k->P01;

    k->P00 = t00 - K0 * t00;
    k->P01 = t01 - K0 * t01;
    k->P10 = k->P10 - K1 * t00;
    k->P11 = k->P11 - K1 * t01;

    return k->angle;
}
