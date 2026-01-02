/**
 * BLE Scanner Interface
 * Handles ESP32 BLE scanning operations
 */

#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <stdbool.h>
#include "device.h"

/**
 * Scanner status
 */
typedef enum {
    SCANNER_STOPPED = 0,
    SCANNER_RUNNING,
    SCANNER_ERROR
} scanner_status_t;

/**
 * Callback function for device discovery
 */
typedef void (*device_callback_t)(const uint8_t *mac, 
                                   int8_t rssi,
                                   address_type_t addr_type,
                                   const char *name,
                                   uint16_t manufacturer_id,
                                   int8_t tx_power);

/**
 * Initialize the BLE scanner
 * @return true on success, false on failure
 */
bool ble_scanner_init(void);

/**
 * Start BLE scanning
 * @param duration_sec Scan duration in seconds (0 for continuous)
 * @param callback Callback function for device discoveries
 * @return true on success, false on failure
 */
bool ble_scanner_start(int duration_sec, device_callback_t callback);

/**
 * Stop BLE scanning
 * @return true on success, false on failure
 */
bool ble_scanner_stop(void);

/**
 * Get current scanner status
 * @return Current status
 */
scanner_status_t ble_scanner_get_status(void);

/**
 * Deinitialize the BLE scanner
 */
void ble_scanner_deinit(void);

#endif // BLE_SCANNER_H
