/**
 * BLE Scanner Implementation for ESP32
 * Uses ESP-IDF BLE stack for scanning
 */

#include "ble_scanner.h"
#include "config.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include <string.h>

static const char *TAG = "BLE_SCANNER";

// Scanner state
static scanner_status_t scanner_status = SCANNER_STOPPED;
static device_callback_t device_cb = NULL;

// BLE scan parameters
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,     // 50ms
    .scan_window            = 0x30,     // 30ms
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

/**
 * Extract manufacturer ID from advertisement data
 */
static uint16_t get_manufacturer_id(uint8_t *adv_data, uint8_t adv_data_len) {
    uint8_t *ptr = adv_data;
    
    while (ptr < adv_data + adv_data_len) {
        uint8_t len = ptr[0];
        if (len == 0) break;
        
        uint8_t type = ptr[1];
        
        // Manufacturer Specific Data
        if (type == 0xFF && len >= 3) {
            return ptr[2] | (ptr[3] << 8);
        }
        
        ptr += len + 1;
    }
    
    return 0;
}

/**
 * Extract TX power from advertisement data
 */
static int8_t get_tx_power(uint8_t *adv_data, uint8_t adv_data_len) {
    uint8_t *ptr = adv_data;
    
    while (ptr < adv_data + adv_data_len) {
        uint8_t len = ptr[0];
        if (len == 0) break;
        
        uint8_t type = ptr[1];
        
        // TX Power Level
        if (type == 0x0A && len >= 2) {
            return (int8_t)ptr[2];
        }
        
        ptr += len + 1;
    }
    
    return 0;
}

/**
 * Extract device name from advertisement data or scan response
 */
static void get_device_name(uint8_t *adv_data, uint8_t adv_data_len, char *name, size_t name_size) {
    uint8_t *ptr = adv_data;
    name[0] = '\0';
    
    while (ptr < adv_data + adv_data_len) {
        uint8_t len = ptr[0];
        if (len == 0) break;
        
        uint8_t type = ptr[1];
        
        // Complete Local Name or Shortened Local Name
        if ((type == 0x09 || type == 0x08) && len > 1) {
            size_t copy_len = len - 1;
            if (copy_len >= name_size) {
                copy_len = name_size - 1;
            }
            memcpy(name, &ptr[2], copy_len);
            name[copy_len] = '\0';
            return;
        }
        
        ptr += len + 1;
    }
}

/**
 * GAP event handler
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            ESP_LOGI(TAG, "Scan parameters set");
            break;
        }
        
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Scan start failed: %d", param->scan_start_cmpl.status);
                scanner_status = SCANNER_ERROR;
            } else {
                ESP_LOGI(TAG, "Scan started");
                scanner_status = SCANNER_RUNNING;
            }
            break;
        }
        
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Scan stop failed: %d", param->scan_stop_cmpl.status);
            } else {
                ESP_LOGI(TAG, "Scan stopped");
                scanner_status = SCANNER_STOPPED;
            }
            break;
        }
        
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t *scan_result = param;
            
            if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                // Device found
                if (device_cb != NULL) {
                    // Determine address type
                    address_type_t addr_type;
                    switch (scan_result->scan_rst.ble_addr_type) {
                        case BLE_ADDR_TYPE_PUBLIC:
                            addr_type = ADDRESS_TYPE_PUBLIC;
                            break;
                        case BLE_ADDR_TYPE_RANDOM:
                        case BLE_ADDR_TYPE_RPA_PUBLIC:
                        case BLE_ADDR_TYPE_RPA_RANDOM:
                        default:
                            addr_type = ADDRESS_TYPE_RANDOM;
                            break;
                    }
                    
                    // Extract device info from advertisement data
                    char name[MAX_NAME_LENGTH] = {0};
                    get_device_name(scan_result->scan_rst.ble_adv, 
                                   scan_result->scan_rst.adv_data_len,
                                   name, sizeof(name));
                    
                    // Also check scan response for name
                    if (name[0] == '\0' && scan_result->scan_rst.scan_rsp_len > 0) {
                        get_device_name(scan_result->scan_rst.ble_adv + scan_result->scan_rst.adv_data_len,
                                       scan_result->scan_rst.scan_rsp_len,
                                       name, sizeof(name));
                    }
                    
                    uint16_t manufacturer_id = get_manufacturer_id(
                        scan_result->scan_rst.ble_adv,
                        scan_result->scan_rst.adv_data_len);
                    
                    int8_t tx_power = get_tx_power(
                        scan_result->scan_rst.ble_adv,
                        scan_result->scan_rst.adv_data_len);
                    
                    // Call the callback
                    device_cb(scan_result->scan_rst.bda,
                             scan_result->scan_rst.rssi,
                             addr_type,
                             name[0] ? name : NULL,
                             manufacturer_id,
                             tx_power);
                    
#if DEBUG_DEVICE_DISCOVERY
                    char mac_str[18];
                    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                           scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                           scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3],
                           scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);
                    ESP_LOGI(TAG, "Device: %s RSSI:%d Name:%s", 
                            mac_str, scan_result->scan_rst.rssi, name);
#endif
                }
            }
            else if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
                ESP_LOGI(TAG, "Scan complete");
                scanner_status = SCANNER_STOPPED;
            }
            break;
        }
        
        default:
            break;
    }
}

bool ble_scanner_init(void) {
    esp_err_t ret;
    
    // Initialize NVS (required for BLE)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Release memory for classic BT (we only need BLE)
    // Note: ESP32-C3/C2/C6 don't have classic BT, so this is a no-op for them
#if !CONFIG_IDF_TARGET_ESP32C3 && !CONFIG_IDF_TARGET_ESP32C2 && !CONFIG_IDF_TARGET_ESP32C6 && !CONFIG_IDF_TARGET_ESP32H2
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
#endif
    
    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Enable BT controller in BLE mode
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Enable Bluedroid
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Register GAP callback
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Set scan parameters
    ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret) {
        ESP_LOGE(TAG, "Set scan params failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "BLE scanner initialized");
    scanner_status = SCANNER_STOPPED;
    return true;
}

bool ble_scanner_start(int duration_sec, device_callback_t callback) {
    if (scanner_status == SCANNER_RUNNING) {
        ESP_LOGW(TAG, "Scanner already running");
        return true;
    }
    
    device_cb = callback;
    
    // Duration 0 means continuous scanning
    uint32_t duration = duration_sec > 0 ? duration_sec : 0;
    
    esp_err_t ret = esp_ble_gap_start_scanning(duration);
    if (ret) {
        ESP_LOGE(TAG, "Start scanning failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool ble_scanner_stop(void) {
    if (scanner_status != SCANNER_RUNNING) {
        return true;
    }
    
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret) {
        ESP_LOGE(TAG, "Stop scanning failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

scanner_status_t ble_scanner_get_status(void) {
    return scanner_status;
}

void ble_scanner_deinit(void) {
    ble_scanner_stop();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    scanner_status = SCANNER_STOPPED;
    device_cb = NULL;
    ESP_LOGI(TAG, "BLE scanner deinitialized");
}
