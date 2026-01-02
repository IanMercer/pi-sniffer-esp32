/**
 * WiFi Provisioning Interface
 * Handles WiFi credential storage and captive portal for configuration
 */

#ifndef WIFI_PROVISION_H
#define WIFI_PROVISION_H

#include <stdbool.h>
#include <stdint.h>

// Maximum lengths for WiFi credentials
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASSWORD_MAX_LEN   64

// Access Point configuration for provisioning mode
#define PROVISION_AP_SSID       "DC Sniffer"
#define PROVISION_AP_PASSWORD   ""  // Open network for easy setup
#define PROVISION_AP_CHANNEL    1
#define PROVISION_AP_MAX_CONN   4

// Provisioning status
typedef enum {
    PROVISION_STATUS_IDLE = 0,
    PROVISION_STATUS_RUNNING,
    PROVISION_STATUS_CONFIGURED,
    PROVISION_STATUS_ERROR
} provision_status_t;

/**
 * WiFi credentials structure
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    bool configured;
} wifi_credentials_t;

/**
 * Initialize the provisioning system
 * @return true on success
 */
bool wifi_provision_init(void);

/**
 * Check if WiFi credentials are stored
 * @return true if credentials exist
 */
bool wifi_provision_has_credentials(void);

/**
 * Load stored WiFi credentials
 * @param creds Pointer to credentials structure to fill
 * @return true if credentials loaded successfully
 */
bool wifi_provision_load_credentials(wifi_credentials_t *creds);

/**
 * Save WiFi credentials to persistent storage
 * @param ssid WiFi network name
 * @param password WiFi password
 * @return true on success
 */
bool wifi_provision_save_credentials(const char *ssid, const char *password);

/**
 * Clear stored WiFi credentials
 * @return true on success
 */
bool wifi_provision_clear_credentials(void);

/**
 * Start the captive portal for WiFi provisioning
 * Creates an access point and starts a web server for configuration
 * @return true on success
 */
bool wifi_provision_start_portal(void);

/**
 * Stop the captive portal
 */
void wifi_provision_stop_portal(void);

/**
 * Get current provisioning status
 * @return Current status
 */
provision_status_t wifi_provision_get_status(void);

/**
 * Check if new configuration was received
 * @return true if new credentials were configured
 */
bool wifi_provision_is_configured(void);

/**
 * Request system reboot
 */
void wifi_provision_reboot(void);

#endif // WIFI_PROVISION_H
