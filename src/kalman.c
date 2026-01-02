/**
 * Kalman Filter Implementation
 * Used for smoothing RSSI readings to get more accurate distance estimates
 */

#include "kalman.h"
#include <math.h>

// Default parameters
#define DEFAULT_ERR_MEASURE     10.0f
#define DEFAULT_ERR_ESTIMATE    10.0f
#define DEFAULT_Q               0.25f   // Process noise covariance

void kalman_init(kalman_filter_t *filter) {
    kalman_init_custom(filter, DEFAULT_ERR_MEASURE, DEFAULT_Q);
}

void kalman_init_custom(kalman_filter_t *filter, float err_measure, float q) {
    filter->err_measure = err_measure;
    filter->err_estimate = DEFAULT_ERR_ESTIMATE;
    filter->q = q;
    filter->current_estimate = 0.0f;
    filter->last_estimate = 0.0f;
    filter->kalman_gain = 0.0f;
    filter->initialized = false;
}

float kalman_update(kalman_filter_t *filter, float measurement) {
    // First measurement - use it directly
    if (!filter->initialized) {
        filter->last_estimate = measurement;
        filter->current_estimate = measurement;
        filter->initialized = true;
        return measurement;
    }
    
    // Calculate Kalman gain
    filter->kalman_gain = filter->err_estimate / (filter->err_estimate + filter->err_measure);
    
    // Calculate current estimate
    filter->current_estimate = filter->last_estimate + 
                               filter->kalman_gain * (measurement - filter->last_estimate);
    
    // Update estimation error
    filter->err_estimate = (1.0f - filter->kalman_gain) * filter->err_estimate + 
                           fabsf(filter->last_estimate - filter->current_estimate) * filter->q;
    
    // Store for next iteration
    filter->last_estimate = filter->current_estimate;
    
    return filter->current_estimate;
}

void kalman_reset(kalman_filter_t *filter) {
    float err_measure = filter->err_measure;
    float q = filter->q;
    kalman_init_custom(filter, err_measure, q);
}

float kalman_get_estimate(const kalman_filter_t *filter) {
    if (!filter->initialized) {
        return 0.0f;
    }
    return filter->current_estimate;
}
