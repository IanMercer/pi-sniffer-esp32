/**
 * Kalman Filter for RSSI Smoothing
 * Reduces noise in RSSI readings for better distance estimation
 */

#ifndef KALMAN_H
#define KALMAN_H

#include <stdbool.h>

/**
 * Kalman filter state structure
 */
typedef struct {
    float err_measure;      // Measurement error estimate
    float err_estimate;     // Estimation error
    float q;                // Process noise covariance
    float current_estimate; // Current estimate
    float last_estimate;    // Previous estimate
    float kalman_gain;      // Current Kalman gain
    bool initialized;       // Has received first measurement
} kalman_filter_t;

/**
 * Initialize a Kalman filter with default parameters
 * @param filter Pointer to the filter structure
 */
void kalman_init(kalman_filter_t *filter);

/**
 * Initialize a Kalman filter with custom parameters
 * @param filter Pointer to the filter structure
 * @param err_measure Measurement error estimate
 * @param q Process noise covariance
 */
void kalman_init_custom(kalman_filter_t *filter, float err_measure, float q);

/**
 * Update the Kalman filter with a new measurement
 * @param filter Pointer to the filter structure
 * @param measurement New measurement value
 * @return Filtered estimate
 */
float kalman_update(kalman_filter_t *filter, float measurement);

/**
 * Reset the Kalman filter to initial state
 * @param filter Pointer to the filter structure
 */
void kalman_reset(kalman_filter_t *filter);

/**
 * Get the current filtered estimate
 * @param filter Pointer to the filter structure
 * @return Current estimate (or 0 if not initialized)
 */
float kalman_get_estimate(const kalman_filter_t *filter);

#endif // KALMAN_H
