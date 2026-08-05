/**
 * HTTP Client Implementation for ESP32
 * Handles REST API communication to send device data
 */

#include "http_client.h"
#include "config.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>
#include <time.h>

static const char *TAG = "HTTP_CLIENT";

bool http_client_init(void) {
    ESP_LOGI(TAG, "HTTP client initialized");
    return true;
}

/**
 * Build JSON payload for device data
 */
static char* build_device_json(const device_list_t *list, const char *device_id) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    
    // Add device ID and timestamp
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
    
    // Create devices array
    cJSON *devices_array = cJSON_CreateArray();
    if (!devices_array) {
        cJSON_Delete(root);
        return NULL;
    }
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!list->devices[i].active) {
            continue;
        }
        
        const ble_device_t *dev = &list->devices[i];
        
        cJSON *device_obj = cJSON_CreateObject();
        if (!device_obj) {
            continue;
        }
        
        cJSON_AddStringToObject(device_obj, "mac", dev->mac_str);
        cJSON_AddNumberToObject(device_obj, "rssi", dev->raw_rssi);
        cJSON_AddNumberToObject(device_obj, "distance", dev->distance);
        
        if (dev->name[0] != '\0') {
            cJSON_AddStringToObject(device_obj, "name", dev->name);
        }
        
        cJSON_AddStringToObject(device_obj, "category", category_to_string(dev->category));
        cJSON_AddStringToObject(device_obj, "address_type", 
                               dev->address_type == ADDRESS_TYPE_PUBLIC ? "public" : "random");
        cJSON_AddNumberToObject(device_obj, "seen_count", dev->seen_count);
        cJSON_AddNumberToObject(device_obj, "first_seen", (double)dev->first_seen);
        cJSON_AddNumberToObject(device_obj, "last_seen", (double)dev->last_seen);

        if (dev->has_superseded_by) {
            char superseded_mac[18];
            mac_to_string(dev->superseded_by, superseded_mac);
            cJSON_AddStringToObject(device_obj, "superseded_by", superseded_mac);
        } else {
            cJSON_AddNullToObject(device_obj, "superseded_by");
        }
        cJSON_AddNumberToObject(device_obj, "superseded_probability", dev->superseded_probability);

        cJSON_AddItemToArray(devices_array, device_obj);
    }
    
    cJSON_AddItemToObject(root, "devices", devices_array);
    
    // Add summary
    device_summary_t summary;
    device_get_summary(list, &summary);
    
    cJSON *summary_obj = cJSON_CreateObject();
    if (summary_obj) {
        cJSON_AddNumberToObject(summary_obj, "total_devices", summary.total_devices);
        cJSON_AddNumberToObject(summary_obj, "phones", summary.phones);
        cJSON_AddNumberToObject(summary_obj, "computers", summary.computers);
        cJSON_AddNumberToObject(summary_obj, "wearables", summary.wearables);
        cJSON_AddNumberToObject(summary_obj, "tablets", summary.tablets);
        cJSON_AddNumberToObject(summary_obj, "beacons", summary.beacons);
        cJSON_AddNumberToObject(summary_obj, "watches", summary.watches);
        cJSON_AddNumberToObject(summary_obj, "headphones", summary.headphones);
        cJSON_AddNumberToObject(summary_obj, "speakers", summary.speakers);
        cJSON_AddNumberToObject(summary_obj, "other", summary.other);
        cJSON_AddItemToObject(root, "summary", summary_obj);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

bool http_send_devices(const device_list_t *list, const char *device_id) {
    // Build JSON payload
    char *json_body = build_device_json(list, device_id);
    if (!json_body) {
        ESP_LOGE(TAG, "Failed to build JSON payload");
        return false;
    }
    
    ESP_LOGD(TAG, "Sending: %s", json_body);
    
    // URL is defined in config
    const char *url = API_URL;
    
    // Check if HTTPS is used to optionally skip cert check
    bool is_https = (strncmp(url, "https", 5) == 0);

    ESP_LOGD(TAG, "Sending to: %s", url);
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = API_TIMEOUT_MS,
        // Use the default CRT bundle for SSL verification (works with Cloudflare)
        .crt_bundle_attach = esp_crt_bundle_attach,
        // .skip_cert_common_name_check: false (Default - we want to verify to satisfy strict servers)
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(json_body);
        return false;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    // Set POST data
    esp_http_client_set_post_field(client, json_body, strlen(json_body));
    
    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    
    bool success = false;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status, content_length);
        success = (status >= 200 && status < 300);
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(json_body);
    
    return success;
}

bool http_post_json(const char *host, int port, const char *path,
                    const char *json_body, http_response_t *response) {
    // Build URL from Host/Port/Path arguments
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d%s", host, port, path);
    // Note: This helper function http_post_json currently assumes HTTP because it takes host/port arguments usually used for mDNS resolution or similar.
    // If we want it to support HTTPS, we'd need to pass the full URL or an 'is_https' flag. 
    // Given the arguments, we will construct a standard URL.
    // However, if the user passes 443 as port, we could infer.
    if (port == 443) {
         snprintf(url, sizeof(url), "https://%s:%d%s", host, port, path);
    }
    
    // Check if HTTPS is used to optionally skip cert check
    bool is_https = (strncmp(url, "https", 5) == 0);

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = API_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    // Set POST data
    esp_http_client_set_post_field(client, json_body, strlen(json_body));
    
    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    
    bool success = false;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status);
        success = (status >= 200 && status < 300);
        
        if (response) {
            response->status_code = status;
            response->body = NULL;
            response->body_len = 0;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        if (response) {
            response->status_code = -1;
            response->body = NULL;
            response->body_len = 0;
        }
    }
    
    esp_http_client_cleanup(client);
    return success;
}

void http_response_free(http_response_t *response) {
    if (response && response->body) {
        free(response->body);
        response->body = NULL;
        response->body_len = 0;
    }
}

void http_client_deinit(void) {
    ESP_LOGI(TAG, "HTTP client deinitialized");
}
