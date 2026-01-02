/**
 * ESP32 BLE Sniffer Configuration
 * Modify these settings to match your environment
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WiFi Configuration
// ============================================================================
// WiFi credentials are now configured via captive portal
// On first boot or double-press BOOT button, device creates "DC Sniffer" AP
// Connect to it and open http://192.168.4.1 to configure WiFi

#define WIFI_MAX_RETRY      10

// ============================================================================
#define API_URL             "https://3334.xomnghien.com/api/devices"
#define API_TIMEOUT_MS      10000
// Skip SSL certificate verification (e.g. for self-signed certs)
#define API_SKIP_CERT_CHECK 1

// ============================================================================
// BLE Scanning Configuration
// ============================================================================
// Duration of each scan cycle in seconds
#define SCAN_DURATION_SEC       10

// How often to send data to the API (seconds)
#define REPORT_INTERVAL_SEC     30

// Maximum number of devices to track
#define MAX_DEVICES             128

// Maximum age before removing a device from tracking (seconds)
#define MAX_DEVICE_AGE_SEC      300

// ============================================================================
// RSSI to Distance Configuration
// ============================================================================
// RSSI value at 1 meter distance (calibrate for your environment)
// Typical values: -50 to -70 dBm
#define RSSI_ONE_METER          -59

// Path loss exponent (2.0-4.0)
// Lower = indoor/cluttered, Higher = outdoor/open
// Typical: 2.0-2.5 indoor, 3.0-4.0 outdoor
#define PATH_LOSS_EXPONENT      2.5f

// Maximum reasonable distance in meters (cap unrealistic values)
#define MAX_DISTANCE_METERS     50.0f

// ============================================================================
// Device Identification
// ============================================================================
// Enable name resolution via BLE scan response
#define ENABLE_NAME_RESOLUTION  1

// Enable device categorization based on manufacturer data
#define ENABLE_CATEGORIZATION   1

// ============================================================================
// Debugging
// ============================================================================
// Enable verbose logging
#define DEBUG_LOGGING           1

// Log individual device discoveries
#define DEBUG_DEVICE_DISCOVERY  0

#endif // CONFIG_H
