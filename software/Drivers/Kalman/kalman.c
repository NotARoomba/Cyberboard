/**
 * @file kalman.c
 * @brief Simple 2-state Kalman filter for angle estimation (sensor fusion).
 *
 * How it works, step by step:
 *
 * The Kalman filter maintains two things:
 *   1. A "state" — our best guess of [angle, gyro_bias]
 *   2. A "covariance matrix" P — how uncertain we are about that guess
 *
 * Each cycle has two phases:
 *
 * PREDICT: We use the gyroscope to predict where the angle should be now.
 *   - new_angle = old_angle + (gyro_reading - bias_estimate) * dt
 *   - Our uncertainty grows a little (because the gyro isn't perfect)
 *
 * UPDATE: We compare our prediction to what the accelerometer says.
 *   - If they disagree, we blend toward the accelerometer
 *   - How much we blend is the "Kalman gain" (0 to 1):
 *       gain ≈ our_uncertainty / (our_uncertainty + sensor_noise)
 *   - If we're very uncertain → gain is high → trust accelerometer more
 *   - If accelerometer is noisy → gain is low → trust our prediction more
 *   - After blending, our uncertainty shrinks (we learned something)
 *
 * Over time, this automatically:
 *   - Removes gyroscope drift (by estimating and subtracting the bias)
 *   - Smooths out accelerometer noise
 *   - Gives you a clean, accurate angle estimate
 */

#include "kalman.h"

void Kalman_Init(Kalman_t *kf, float initial_angle)
{
    kf->angle = initial_angle;
    kf->bias  = 0.0f;

    /* Start with moderate uncertainty — the filter will converge quickly.
     * P[0][0] = uncertainty in angle, P[1][1] = uncertainty in bias.
     * Off-diagonals represent correlation between angle and bias errors. */
    kf->P[0][0] = 1.0f;
    kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f;
    kf->P[1][1] = 1.0f;

    /* Default tuning — good starting point for a 50 Hz IMU loop.
     * Tweak these if the output is too noisy or too sluggish:
     *   - Increase Q_angle  → faster response, more noise
     *   - Increase R_measure → smoother output, slower response
     *   - Increase Q_bias   → bias estimate adapts faster */
    kf->Q_angle   = 0.001f;
    kf->Q_bias    = 0.003f;
    kf->R_measure  = 0.03f;
}

float Kalman_Update(Kalman_t *kf, float gyro_rate, float accel_angle, float dt)
{
    /*
     * ========== STEP 1: PREDICT ==========
     *
     * Use the gyroscope to project the angle forward in time.
     * We subtract our current bias estimate to correct for drift:
     *   angle_new = angle_old + (gyro - bias) * dt
     *
     * The bias itself doesn't change in our model (it's assumed constant
     * or slowly varying), so bias_new = bias_old.
     */
    kf->angle += dt * (gyro_rate - kf->bias);
    /* bias stays the same: kf->bias = kf->bias */

    /*
     * Grow the uncertainty (covariance matrix P).
     *
     * This is the math for: P = F * P * F^T + Q
     * where F is the state transition matrix:
     *   F = | 1  -dt |    (angle depends on bias through the gyro)
     *       | 0   1  |    (bias doesn't change)
     * and Q is the process noise:
     *   Q = | Q_angle  0      |
     *       | 0        Q_bias |
     *
     * In English: our uncertainty about the angle grows because:
     *   1. The gyro integration adds noise (Q_angle)
     *   2. Any error in our bias estimate also affects the angle (-dt * P[1][1])
     */
    kf->P[0][0] += dt * (dt * kf->P[1][1] - kf->P[0][1] - kf->P[1][0] + kf->Q_angle);
    kf->P[0][1] -= dt * kf->P[1][1];
    kf->P[1][0] -= dt * kf->P[1][1];
    kf->P[1][1] += kf->Q_bias * dt;

    /*
     * ========== STEP 2: UPDATE ==========
     *
     * Now we have a measurement from the accelerometer (accel_angle).
     * Compare it to our prediction and correct.
     */

    /* Innovation (measurement residual):
     * How much the accelerometer disagrees with our prediction.
     * If this is zero, accelerometer confirms our prediction perfectly. */
    float y = accel_angle - kf->angle;

    /* Innovation covariance:
     * Total uncertainty = our prediction uncertainty + sensor noise.
     * This is the denominator for the Kalman gain calculation. */
    float S = kf->P[0][0] + kf->R_measure;

    /* Kalman gain (2x1 vector):
     * K[0] = how much to correct the angle  (0 to 1)
     * K[1] = how much to correct the bias   (0 to 1)
     *
     * When S is large (we're uncertain OR sensor is noisy),
     * gain is smaller → we correct less.
     * When P[0][0] is large (we're very uncertain about our prediction),
     * gain is larger → we trust the accelerometer more. */
    float K0 = kf->P[0][0] / S;
    float K1 = kf->P[1][0] / S;

    /* State update:
     * Nudge our estimates toward the measurement, proportional to the gain.
     *
     * For angle: if K0=0.7 and accel says we're 2° off, correct by 1.4°
     * For bias: if K1=0.1 and we're 2° off, adjust bias by 0.2 deg/s
     *           (the filter learns "my gyro has been drifting") */
    kf->angle += K0 * y;
    kf->bias  += K1 * y;

    /* Covariance update:
     * Our uncertainty SHRINKS because we incorporated new information.
     * This is the Joseph form simplified for a scalar measurement:
     *   P = (I - K*H) * P
     * where H = [1, 0] (we only directly measure the angle, not bias). */
    float P00_temp = kf->P[0][0];
    float P01_temp = kf->P[0][1];
    kf->P[0][0] -= K0 * P00_temp;
    kf->P[0][1] -= K0 * P01_temp;
    kf->P[1][0] -= K1 * P00_temp;
    kf->P[1][1] -= K1 * P01_temp;

    return kf->angle;
}
