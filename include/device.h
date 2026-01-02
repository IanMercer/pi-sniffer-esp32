/**
 * BLE Device Tracking Structures and Functions
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "kalman.h"
#include "config.h"

// Device category constants
typedef enum {
    CATEGORY_UNKNOWN = 0,
    CATEGORY_PHONE,
    CATEGORY_WEARABLE,
    CATEGORY_TABLET,
    CATEGORY_HEADPHONES,
    CATEGORY_COMPUTER,
    CATEGORY_TV,
    CATEGORY_BEACON,
    CATEGORY_CAR,
    CATEGORY_WATCH,
    CATEGORY_FITNESS,
    CATEGORY_SPEAKER,
    CATEGORY_OTHER
} device_category_t;

// Address type constants
typedef enum {
    ADDRESS_TYPE_UNKNOWN = 0,
    ADDRESS_TYPE_PUBLIC,
    ADDRESS_TYPE_RANDOM
} address_type_t;

// Maximum name length
#define MAX_NAME_LENGTH 32

/**
 * Structure representing a tracked BLE device
 */
typedef struct {
    uint8_t mac[6];              // MAC address bytes
    char mac_str[18];            // MAC address string "XX:XX:XX:XX:XX:XX"
    char name[MAX_NAME_LENGTH];  // Device name (if available)
    
    address_type_t address_type; // Public or random address
    device_category_t category;  // Device category
    
    int8_t raw_rssi;             // Last raw RSSI value
    kalman_filter_t rssi_filter; // Kalman filter for RSSI
    float distance;              // Estimated distance in meters
    
    int8_t tx_power;             // Reported TX power (if available)
    uint16_t manufacturer_id;    // Manufacturer ID (if available)
    
    time_t first_seen;           // First time device was seen
    time_t last_seen;            // Most recent time device was seen
    uint32_t seen_count;         // Number of times seen
    
    bool active;                 // Is this slot in use?
} ble_device_t;

/**
 * Device tracking list manager
 */
typedef struct {
    ble_device_t devices[MAX_DEVICES];
    int count;                   // Number of active devices
    uint32_t total_discovered;   // Total unique devices discovered
} device_list_t;

/**
 * Summary statistics
 */
typedef struct {
    int total_devices;
    int phones;
    int computers;
    int wearables;
    int tablets;
    int beacons;
    int watches;
    int headphones;
    int speakers;
    int other;
} device_summary_t;

/**
 * Initialize the device list
 * @param list Pointer to the device list
 */
void device_list_init(device_list_t *list);

/**
 * Find a device by MAC address
 * @param list Pointer to the device list
 * @param mac MAC address bytes (6 bytes)
 * @return Pointer to device or NULL if not found
 */
ble_device_t* device_find(device_list_t *list, const uint8_t *mac);

/**
 * Add or update a device in the list
 * @param list Pointer to the device list
 * @param mac MAC address bytes (6 bytes)
 * @param rssi RSSI value
 * @param addr_type Address type (public/random)
 * @param name Device name (can be NULL)
 * @param manufacturer_id Manufacturer ID (0 if unknown)
 * @param tx_power TX power (0 if unknown)
 * @return Pointer to the device, or NULL if list is full
 */
ble_device_t* device_update(device_list_t *list, 
                            const uint8_t *mac,
                            int8_t rssi,
                            address_type_t addr_type,
                            const char *name,
                            uint16_t manufacturer_id,
                            int8_t tx_power);

/**
 * Remove stale devices (not seen recently)
 * @param list Pointer to the device list
 * @param max_age_sec Remove devices not seen in this many seconds
 * @return Number of devices removed
 */
int device_cleanup(device_list_t *list, int max_age_sec);

/**
 * Calculate distance from RSSI
 * @param rssi RSSI value in dBm
 * @param tx_power TX power at 1 meter (use RSSI_ONE_METER if 0)
 * @return Estimated distance in meters
 */
float calculate_distance(int8_t rssi, int8_t tx_power);

/**
 * Categorize a device based on available data
 * @param device Pointer to the device
 */
void device_categorize(ble_device_t *device);

/**
 * Get summary statistics for the device list
 * @param list Pointer to the device list
 * @param summary Pointer to summary structure to fill
 */
void device_get_summary(const device_list_t *list, device_summary_t *summary);

/**
 * Convert MAC bytes to string
 * @param mac MAC address bytes (6 bytes)
 * @param str Output string buffer (at least 18 bytes)
 */
void mac_to_string(const uint8_t *mac, char *str);

/**
 * Get category name as string
 * @param category Category enum value
 * @return Category name string
 */
const char* category_to_string(device_category_t category);

#endif // DEVICE_H
