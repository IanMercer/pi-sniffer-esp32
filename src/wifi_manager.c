/**
 * WiFi Manager Implementation for ESP32
 * Handles WiFi connectivity in station mode
 */

#include "wifi_manager.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <string.h>

static const char *TAG = "WIFI_MANAGER";

// Event group for WiFi events
static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;

// WiFi state
static wifi_status_t wifi_status = WIFI_STATUS_DISCONNECTED;
static int retry_count = 0;
static esp_netif_t *sta_netif = NULL;

/**
 * WiFi event handler
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, connecting...");
                wifi_status = WIFI_STATUS_CONNECTING;
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_status = WIFI_STATUS_DISCONNECTED;
                if (retry_count < WIFI_MAX_RETRY) {
                    ESP_LOGI(TAG, "Retry connecting (%d/%d)", retry_count + 1, WIFI_MAX_RETRY);
                    esp_wifi_connect();
                    retry_count++;
                } else {
                    ESP_LOGE(TAG, "Failed to connect after %d attempts", WIFI_MAX_RETRY);
                    wifi_status = WIFI_STATUS_ERROR;
                    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            wifi_status = WIFI_STATUS_CONNECTED;
            retry_count = 0;
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

bool wifi_manager_init(void) {
    // Create event group
    wifi_event_group = xEventGroupCreate();
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi station
    sta_netif = esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    ESP_LOGI(TAG, "WiFi initialized");
    return true;
}

bool wifi_connect(const char *ssid, const char *password) {
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // Copy SSID and password
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
    wifi_status = WIFI_STATUS_CONNECTING;
    retry_count = 0;
    
    return true;
}

bool wifi_disconnect(void) {
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Disconnect failed: %s", esp_err_to_name(ret));
        return false;
    }
    wifi_status = WIFI_STATUS_DISCONNECTED;
    return true;
}

wifi_status_t wifi_get_status(void) {
    return wifi_status;
}

bool wifi_is_connected(void) {
    return wifi_status == WIFI_STATUS_CONNECTED;
}

bool wifi_wait_connected(int timeout_ms) {
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           timeout_ms / portTICK_PERIOD_MS);
    
    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        return false;
    }
    
    // Timeout
    return false;
}

bool wifi_get_ip(char *ip_str) {
    if (!wifi_is_connected()) {
        return false;
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK) {
        return false;
    }
    
    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    return true;
}

bool wifi_get_mac(char *mac_str) {
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) {
        return false;
    }
    
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

void wifi_manager_deinit(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy(sta_netif);
    sta_netif = NULL;
    vEventGroupDelete(wifi_event_group);
    wifi_event_group = NULL;
    wifi_status = WIFI_STATUS_DISCONNECTED;
    ESP_LOGI(TAG, "WiFi deinitialized");
}
