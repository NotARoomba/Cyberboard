/**
 * @file kalman.h
 * @brief Simple 2-state Kalman filter for angle estimation (sensor fusion).
 *
 * Fuses gyroscope rate (deg/s) with an accelerometer-derived angle (deg)
 * to produce a drift-free, low-noise angle estimate.
 *
 * State vector: [angle, gyro_bias]
 *   - angle:     estimated tilt angle in degrees
 *   - gyro_bias: estimated gyroscope bias (drift) in deg/s
 */

#ifndef KALMAN_H
#define KALMAN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* --- State --- */
    float angle;    /* Estimated angle (degrees) */
    float bias;     /* Estimated gyro bias (deg/s) */

    /* --- Error covariance matrix (2x2) --- */
    float P[2][2];

    /* --- Tuning parameters (set once, then leave alone) --- */
    float Q_angle;   /* Process noise variance for angle.
                        Larger = less trust in gyro integration,
                        filter reacts faster but noisier. */
    float Q_bias;    /* Process noise variance for bias.
                        Larger = assume bias changes quickly. */
    float R_measure; /* Measurement noise variance (accelerometer).
                        Larger = less trust in accel, smoother but slower. */
} Kalman_t;

/**
 * @brief  Initialise a Kalman filter instance with default tuning.
 * @param  kf            Pointer to Kalman filter struct
 * @param  initial_angle Starting angle from accelerometer (deg).
 *                       Pass the first atan2 reading so the filter
 *                       doesn't have to converge from 0.
 */
void Kalman_Init(Kalman_t *kf, float initial_angle);

/**
 * @brief  Run one predict+update cycle of the Kalman filter.
 *
 * Call this once per sample period. The filter:
 *   1. PREDICTS the new angle using the gyro rate (corrected for estimated bias)
 *   2. UPDATES the estimate using the accelerometer-derived angle
 *
 * @param  kf          Pointer to Kalman filter struct
 * @param  gyro_rate   Raw gyroscope reading for this axis (deg/s)
 * @param  accel_angle Angle calculated from accelerometer via atan2 (deg)
 * @param  dt          Time step since last call (seconds)
 * @return Filtered angle estimate (degrees)
 */
float Kalman_Update(Kalman_t *kf, float gyro_rate, float accel_angle, float dt);

#ifdef __cplusplus
}
#endif

#endif /* KALMAN_H */
