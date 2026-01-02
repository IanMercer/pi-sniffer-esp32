/**
 * BLE Device Tracking Implementation
 * Manages the list of discovered BLE devices
 */

#include "device.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Apple manufacturer ID
#define APPLE_MANUFACTURER_ID       0x004C

// Microsoft manufacturer ID  
#define MICROSOFT_MANUFACTURER_ID   0x0006

// Samsung manufacturer ID
#define SAMSUNG_MANUFACTURER_ID     0x0075

// Category name lookup table
static const char* category_names[] = {
    "unknown",
    "phone",
    "wearable",
    "tablet",
    "headphones",
    "computer",
    "tv",
    "beacon",
    "car",
    "watch",
    "fitness",
    "speaker",
    "other"
};

void device_list_init(device_list_t *list) {
    memset(list, 0, sizeof(device_list_t));
    list->count = 0;
    list->total_discovered = 0;
}

void mac_to_string(const uint8_t *mac, char *str) {
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

ble_device_t* device_find(device_list_t *list, const uint8_t *mac) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (list->devices[i].active && 
            memcmp(list->devices[i].mac, mac, 6) == 0) {
            return &list->devices[i];
        }
    }
    return NULL;
}

static ble_device_t* find_empty_slot(device_list_t *list) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!list->devices[i].active) {
            return &list->devices[i];
        }
    }
    return NULL;
}

float calculate_distance(int8_t rssi, int8_t tx_power) {
    // Use calibrated value if tx_power isn't provided
    if (tx_power == 0) {
        tx_power = RSSI_ONE_METER;
    }
    
    if (rssi == 0) {
        return -1.0f; // Invalid RSSI
    }
    
    // Log-distance path loss model
    // distance = 10 ^ ((tx_power - rssi) / (10 * n))
    // where n is the path loss exponent
    float exponent = ((float)tx_power - (float)rssi) / (10.0f * PATH_LOSS_EXPONENT);
    float distance = powf(10.0f, exponent);
    
    // Cap unreasonable values
    if (distance > MAX_DISTANCE_METERS) {
        distance = MAX_DISTANCE_METERS;
    }
    if (distance < 0.0f) {
        distance = 0.0f;
    }
    
    return distance;
}

void device_categorize(ble_device_t *device) {
    // Already categorized
    if (device->category != CATEGORY_UNKNOWN) {
        return;
    }
    
    // Categorize based on manufacturer ID
    switch (device->manufacturer_id) {
        case APPLE_MANUFACTURER_ID:
            // Apple devices - likely phone, tablet, or watch
            // Could refine based on manufacturer data bytes
            device->category = CATEGORY_PHONE;
            break;
            
        case MICROSOFT_MANUFACTURER_ID:
            // Microsoft - likely computer
            device->category = CATEGORY_COMPUTER;
            break;
            
        case SAMSUNG_MANUFACTURER_ID:
            // Samsung - likely phone or tablet
            device->category = CATEGORY_PHONE;
            break;
            
        default:
            break;
    }
    
    // Categorize based on name patterns
    if (device->name[0] != '\0' && device->category == CATEGORY_UNKNOWN) {
        // Convert name to lowercase for comparison
        char lower_name[MAX_NAME_LENGTH];
        for (int i = 0; i < MAX_NAME_LENGTH && device->name[i]; i++) {
            lower_name[i] = (device->name[i] >= 'A' && device->name[i] <= 'Z') 
                          ? device->name[i] + 32 
                          : device->name[i];
        }
        lower_name[MAX_NAME_LENGTH - 1] = '\0';
        
        if (strstr(lower_name, "iphone") || strstr(lower_name, "android") ||
            strstr(lower_name, "pixel") || strstr(lower_name, "galaxy") ||
            strstr(lower_name, "oneplus") || strstr(lower_name, "xiaomi")) {
            device->category = CATEGORY_PHONE;
        }
        else if (strstr(lower_name, "ipad") || strstr(lower_name, "tablet")) {
            device->category = CATEGORY_TABLET;
        }
        else if (strstr(lower_name, "macbook") || strstr(lower_name, "laptop") ||
                 strstr(lower_name, "windows") || strstr(lower_name, "pc")) {
            device->category = CATEGORY_COMPUTER;
        }
        else if (strstr(lower_name, "watch") || strstr(lower_name, "band") ||
                 strstr(lower_name, "fitbit") || strstr(lower_name, "garmin")) {
            device->category = CATEGORY_WATCH;
        }
        else if (strstr(lower_name, "airpod") || strstr(lower_name, "buds") ||
                 strstr(lower_name, "headphone") || strstr(lower_name, "earphone")) {
            device->category = CATEGORY_HEADPHONES;
        }
        else if (strstr(lower_name, "speaker") || strstr(lower_name, "sonos") ||
                 strstr(lower_name, "bose") || strstr(lower_name, "jbl")) {
            device->category = CATEGORY_SPEAKER;
        }
        else if (strstr(lower_name, "beacon") || strstr(lower_name, "tile") ||
                 strstr(lower_name, "airtag")) {
            device->category = CATEGORY_BEACON;
        }
        else if (strstr(lower_name, "tv") || strstr(lower_name, "roku") ||
                 strstr(lower_name, "fire stick") || strstr(lower_name, "chromecast")) {
            device->category = CATEGORY_TV;
        }
    }
    
    // Categorize based on address type
    // Random addresses are more likely to be phones/tablets (MAC randomization)
    if (device->category == CATEGORY_UNKNOWN && 
        device->address_type == ADDRESS_TYPE_RANDOM) {
        device->category = CATEGORY_PHONE; // Assume phone for random addresses
    }
}

ble_device_t* device_update(device_list_t *list, 
                            const uint8_t *mac,
                            int8_t rssi,
                            address_type_t addr_type,
                            const char *name,
                            uint16_t manufacturer_id,
                            int8_t tx_power) {
    time_t now = time(NULL);
    ble_device_t *device = device_find(list, mac);
    
    if (device == NULL) {
        // New device
        device = find_empty_slot(list);
        if (device == NULL) {
            // List is full, could implement LRU here
            return NULL;
        }
        
        // Initialize new device
        memset(device, 0, sizeof(ble_device_t));
        memcpy(device->mac, mac, 6);
        mac_to_string(mac, device->mac_str);
        device->active = true;
        device->first_seen = now;
        device->category = CATEGORY_UNKNOWN;
        kalman_init(&device->rssi_filter);
        
        list->count++;
        list->total_discovered++;
    }
    
    // Update device data
    device->last_seen = now;
    device->seen_count++;
    device->raw_rssi = rssi;
    device->address_type = addr_type;
    
    // Update name if provided and not empty
    if (name != NULL && name[0] != '\0' && device->name[0] == '\0') {
        strncpy(device->name, name, MAX_NAME_LENGTH - 1);
        device->name[MAX_NAME_LENGTH - 1] = '\0';
    }
    
    // Update manufacturer ID
    if (manufacturer_id != 0) {
        device->manufacturer_id = manufacturer_id;
    }
    
    // Update TX power
    if (tx_power != 0) {
        device->tx_power = tx_power;
    }
    
    // Apply Kalman filter to RSSI
    float filtered_rssi = kalman_update(&device->rssi_filter, (float)rssi);
    
    // Calculate distance from filtered RSSI
    device->distance = calculate_distance((int8_t)filtered_rssi, device->tx_power);
    
    // Try to categorize the device
#if ENABLE_CATEGORIZATION
    device_categorize(device);
#endif
    
    return device;
}

int device_cleanup(device_list_t *list, int max_age_sec) {
    time_t now = time(NULL);
    int removed = 0;
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (list->devices[i].active) {
            double age = difftime(now, list->devices[i].last_seen);
            if (age > max_age_sec) {
                list->devices[i].active = false;
                list->count--;
                removed++;
            }
        }
    }
    
    return removed;
}

void device_get_summary(const device_list_t *list, device_summary_t *summary) {
    memset(summary, 0, sizeof(device_summary_t));
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!list->devices[i].active) {
            continue;
        }
        
        summary->total_devices++;
        
        switch (list->devices[i].category) {
            case CATEGORY_PHONE:
                summary->phones++;
                break;
            case CATEGORY_COMPUTER:
                summary->computers++;
                break;
            case CATEGORY_WEARABLE:
                summary->wearables++;
                break;
            case CATEGORY_TABLET:
                summary->tablets++;
                break;
            case CATEGORY_BEACON:
                summary->beacons++;
                break;
            case CATEGORY_WATCH:
                summary->watches++;
                break;
            case CATEGORY_HEADPHONES:
                summary->headphones++;
                break;
            case CATEGORY_SPEAKER:
                summary->speakers++;
                break;
            default:
                summary->other++;
                break;
        }
    }
}

const char* category_to_string(device_category_t category) {
    if (category >= 0 && category < sizeof(category_names) / sizeof(category_names[0])) {
        return category_names[category];
    }
    return "unknown";
}
