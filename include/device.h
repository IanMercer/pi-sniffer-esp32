/**
 * BLE Device Tracking Structures and Functions
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
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
    CATEGORY_FIXED,   // stationary receiver, e.g. an AirPlay target/Apple TV
    CATEGORY_APPLIANCE, // e.g. a coffee machine, fridge, air filter
    CATEGORY_OTHER
} device_category_t;

// Address type constants
typedef enum {
    ADDRESS_TYPE_UNKNOWN = 0,
    ADDRESS_TYPE_PUBLIC,
    ADDRESS_TYPE_RANDOM
} address_type_t;

// How much to trust a device's name, lowest to highest. A name is only ever
// overwritten by a value with strictly higher confidence than what's already
// set (see device_set_name()), so the actual advertised local name always
// wins over a heuristic guess.
typedef enum {
    NAME_CONF_NONE = 0,          // no name set yet
    NAME_CONF_GENERIC = 100,     // e.g. "Beacon"
    NAME_CONF_MANUFACTURER = 200,// e.g. "Apple", "AirPrint"
    NAME_CONF_DEVICE = 300,      // e.g. "AirPods", "HomeKit"
    NAME_CONF_KNOWN = 400        // the device's actual advertised local name
} name_confidence_t;

// Maximum name length
#define MAX_NAME_LENGTH 32

// Sentinel for ble_device_t.tx_power meaning "no TX Power AD field seen yet"
#define TX_POWER_UNKNOWN INT8_MIN

/**
 * Structure representing a tracked BLE device
 */
typedef struct {
    uint8_t mac[6];              // MAC address bytes
    char mac_str[18];            // MAC address string "XX:XX:XX:XX:XX:XX"
    char name[MAX_NAME_LENGTH];  // Device name (if available)
    name_confidence_t name_confidence; // How the current name was determined

    address_type_t address_type; // Public or random address
    device_category_t category;  // Device category
    
    int8_t raw_rssi;             // Last raw RSSI value
    kalman_filter_t rssi_filter; // Kalman filter for RSSI
    float distance;              // Estimated distance in meters
    
    int8_t tx_power;             // Reported TX power, or TX_POWER_UNKNOWN
    uint16_t manufacturer_id;    // Manufacturer ID (only valid if has_manufacturer_data)
    bool has_manufacturer_data;  // Was a manufacturer-specific AD structure ever seen?
                                  // (0x0000 is Ericsson's real company ID, so manufacturer_id
                                  // alone can't distinguish "never seen" from "seen, id 0")
    
    time_t first_seen;           // First time device was seen
    time_t last_seen;            // Most recent time device was seen
    uint32_t seen_count;         // Number of times seen
    
    bool active;                 // Is this slot in use?

    bool has_superseded_by;        // true if this (older) device is believed to have
                                    // rotated its MAC to superseded_by
    uint8_t superseded_by[6];      // MAC of the newer device it rotated to (valid iff has_superseded_by)
    float superseded_probability;  // confidence 0.0-1.0 in that link
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
 * @param manufacturer_payload Manufacturer-specific data with the 2-byte
 *        manufacturer ID stripped off (can be NULL)
 * @param manufacturer_payload_len Length of manufacturer_payload
 * @return Pointer to the device, or NULL if list is full
 */
ble_device_t* device_update(device_list_t *list,
                            const uint8_t *mac,
                            int8_t rssi,
                            address_type_t addr_type,
                            const char *name,
                            uint16_t manufacturer_id,
                            int8_t tx_power,
                            const uint8_t *manufacturer_payload,
                            uint8_t manufacturer_payload_len);

/**
 * Set a device's name if the new value has strictly higher confidence than
 * whatever name is currently set (or no name is set yet). No-op for a NULL
 * or empty value.
 * @param device Pointer to the device
 * @param value New name to consider
 * @param confidence How trustworthy this name is
 */
void device_set_name(ble_device_t *device, const char *value, name_confidence_t confidence);

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

/**
 * Get manufacturer name for a Bluetooth SIG company identifier
 * @param has_manufacturer_id Was a manufacturer-specific AD structure seen at all?
 * @param manufacturer_id Manufacturer ID (ignored unless has_manufacturer_id is true)
 * @param buf Scratch buffer used if the ID is not recognized (holds "0xXXXX")
 * @param buf_len Size of buf
 * @return Manufacturer name, or the hex ID formatted into buf if not recognized,
 *         or "unknown" if has_manufacturer_id is false
 */
const char* manufacturer_to_string(bool has_manufacturer_id, uint16_t manufacturer_id, char *buf, size_t buf_len);

#endif // DEVICE_H
