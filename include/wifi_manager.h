/**
 * WiFi Manager Interface
 * Handles WiFi connectivity for the ESP32
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

/**
 * WiFi connection status
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_ERROR
} wifi_status_t;

/**
 * Initialize WiFi in station mode
 * @return true on success, false on failure
 */
bool wifi_manager_init(void);

/**
 * Connect to the configured WiFi network
 * @param ssid WiFi network name
 * @param password WiFi password
 * @return true if connection initiated successfully
 */
bool wifi_connect(const char *ssid, const char *password);

/**
 * Disconnect from WiFi
 * @return true on success
 */
bool wifi_disconnect(void);

/**
 * Get current WiFi connection status
 * @return Current status
 */
wifi_status_t wifi_get_status(void);

/**
 * Check if WiFi is connected
 * @return true if connected
 */
bool wifi_is_connected(void);

/**
 * Wait for WiFi connection with timeout
 * @param timeout_ms Timeout in milliseconds
 * @return true if connected within timeout
 */
bool wifi_wait_connected(int timeout_ms);

/**
 * Get the device's IP address as string
 * @param ip_str Buffer to store IP string (at least 16 bytes)
 * @return true if IP available
 */
bool wifi_get_ip(char *ip_str);

/**
 * Get the device's MAC address as string
 * @param mac_str Buffer to store MAC string (at least 18 bytes)
 * @return true on success
 */
bool wifi_get_mac(char *mac_str);

/**
 * Deinitialize WiFi
 */
void wifi_manager_deinit(void);

#endif // WIFI_MANAGER_H
