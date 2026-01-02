/**
 * WiFi Provisioning Implementation
 * Captive portal for WiFi configuration with NVS storage
 */

#include "wifi_provision.h"
#include "captive_dns.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "lwip/inet.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "WIFI_PROVISION";

// NVS namespace and keys
#define NVS_NAMESPACE       "wifi_config"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASSWORD    "password"
#define NVS_KEY_CONFIGURED  "configured"

// Provisioning state
static provision_status_t provision_status = PROVISION_STATUS_IDLE;
static httpd_handle_t http_server = NULL;
static esp_netif_t *ap_netif = NULL;
static bool new_config_received = false;

// HTML page for configuration
static const char *HTML_PAGE = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"    <meta name='viewport' content='width=device-width, initial-scale=1'>"
"    <title>BLE Sniffer Setup</title>"
"    <style>"
"        * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }"
"        body { margin: 0; padding: 20px; background: linear-gradient(135deg, #1a1a2e 0%%, #16213e 100%%); min-height: 100vh; }"
"        .container { max-width: 400px; margin: 0 auto; }"
"        .card { background: rgba(255,255,255,0.95); border-radius: 16px; padding: 30px; box-shadow: 0 10px 40px rgba(0,0,0,0.3); }"
"        h1 { color: #1a1a2e; margin: 0 0 10px; font-size: 24px; text-align: center; }"
"        .subtitle { color: #666; text-align: center; margin-bottom: 25px; font-size: 14px; }"
"        .form-group { margin-bottom: 20px; }"
"        label { display: block; color: #333; margin-bottom: 8px; font-weight: 500; }"
"        input[type='text'], input[type='password'] {"
"            width: 100%%; padding: 14px; border: 2px solid #e0e0e0; border-radius: 10px;"
"            font-size: 16px; transition: border-color 0.3s;"
"        }"
"        input:focus { outline: none; border-color: #4a90d9; }"
"        button {"
"            width: 100%%; padding: 14px; background: linear-gradient(135deg, #4a90d9 0%%, #357abd 100%%);"
"            color: white; border: none; border-radius: 10px; font-size: 16px; font-weight: 600;"
"            cursor: pointer; transition: transform 0.2s, box-shadow 0.2s;"
"        }"
"        button:hover { transform: translateY(-2px); box-shadow: 0 5px 20px rgba(74,144,217,0.4); }"
"        button:active { transform: translateY(0); }"
"        .icon { text-align: center; font-size: 48px; margin-bottom: 15px; }"
"        .success { background: #d4edda; color: #155724; padding: 15px; border-radius: 10px; text-align: center; display: none; }"
"        .note { color: #888; font-size: 12px; text-align: center; margin-top: 20px; }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <div class='card'>"
"            <div class='icon'>📡</div>"
"            <h1>BLE Sniffer Setup</h1>"
"            <p class='subtitle'>Configure WiFi connection</p>"
"            <div class='success' id='success'>Configuration saved! Device will restart...</div>"
"            <form id='wifiForm' action='/save' method='POST'>"
"                <div class='form-group'>"
"                    <label for='ssid'>WiFi Network Name</label>"
"                    <input type='text' id='ssid' name='ssid' placeholder='Enter SSID' required maxlength='31'>"
"                </div>"
"                <div class='form-group'>"
"                    <label for='password'>WiFi Password</label>"
"                    <input type='password' id='password' name='password' placeholder='Enter password' maxlength='63'>"
"                </div>"
"                <button type='submit'>Save & Connect</button>"
"            </form>"
"            <p class='note'>After saving, the device will restart and connect to your WiFi network.</p>"
"        </div>"
"    </div>"
"    <script>"
"        document.getElementById('wifiForm').onsubmit = function(e) {"
"            e.preventDefault();"
"            var formData = new FormData(this);"
"            fetch('/save', { method: 'POST', body: formData })"
"            .then(function(r) { return r.text(); })"
"            .then(function(t) {"
"                document.getElementById('success').style.display = 'block';"
"                document.getElementById('wifiForm').style.display = 'none';"
"            });"
"        };"
"    </script>"
"</body>"
"</html>";

static const char *SUCCESS_PAGE = 
"<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>body{font-family:sans-serif;text-align:center;padding:50px;background:#1a1a2e;color:#fff;}"
".ok{font-size:64px;}</style></head><body>"
"<div class='ok'>✅</div><h1>Configuration Saved!</h1>"
"<p>Device will restart in 3 seconds...</p></body></html>";

// Initialize NVS
static bool init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret == ESP_OK;
}

bool wifi_provision_init(void) {
    if (!init_nvs()) {
        ESP_LOGE(TAG, "NVS init failed");
        return false;
    }
    ESP_LOGI(TAG, "Provisioning system initialized");
    return true;
}

bool wifi_provision_has_credentials(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }
    
    uint8_t configured = 0;
    err = nvs_get_u8(nvs, NVS_KEY_CONFIGURED, &configured);
    nvs_close(nvs);
    
    return (err == ESP_OK && configured == 1);
}

bool wifi_provision_load_credentials(wifi_credentials_t *creds) {
    if (creds == NULL) {
        return false;
    }
    
    memset(creds, 0, sizeof(wifi_credentials_t));
    
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No stored credentials found");
        return false;
    }
    
    size_t ssid_len = WIFI_SSID_MAX_LEN;
    size_t pass_len = WIFI_PASSWORD_MAX_LEN;
    
    err = nvs_get_str(nvs, NVS_KEY_SSID, creds->ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_get_str(nvs, NVS_KEY_PASSWORD, creds->password, &pass_len);
    if (err != ESP_OK) {
        // Password might be empty for open networks
        creds->password[0] = '\0';
    }
    
    uint8_t configured = 0;
    nvs_get_u8(nvs, NVS_KEY_CONFIGURED, &configured);
    creds->configured = (configured == 1);
    
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Loaded credentials for SSID: %s", creds->ssid);
    return true;
}

bool wifi_provision_save_credentials(const char *ssid, const char *password) {
    if (ssid == NULL || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "Invalid SSID");
        return false;
    }
    
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_str(nvs, NVS_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID");
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_set_str(nvs, NVS_KEY_PASSWORD, password ? password : "");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password");
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_set_u8(nvs, NVS_KEY_CONFIGURED, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configured flag");
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_commit(nvs);
    nvs_close(nvs);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed");
        return false;
    }
    
    ESP_LOGI(TAG, "Credentials saved for SSID: %s", ssid);
    new_config_received = true;
    provision_status = PROVISION_STATUS_CONFIGURED;
    return true;
}

bool wifi_provision_clear_credentials(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return false;
    }
    
    nvs_erase_all(nvs);
    nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Credentials cleared");
    return true;
}

// URL decode helper
static void url_decode(char *dst, const char *src, size_t dst_size) {
    char a, b;
    size_t i = 0;
    while (*src && i < dst_size - 1) {
        if (*src == '%' && ((a = src[1]) && (b = src[2])) && 
            isxdigit(a) && isxdigit(b)) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= 'A' - 10;
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= 'A' - 10;
            else b -= '0';
            dst[i++] = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

// Parse multipart form data (WebKitFormBoundary format)
static bool parse_multipart_form(const char *content, wifi_credentials_t *creds) {
    // Look for name="ssid" followed by value
    const char *ssid_name = strstr(content, "name=\"ssid\"");
    if (ssid_name) {
        // Find the blank line after headers (double newline)
        const char *value_start = strstr(ssid_name, "\r\n\r\n");
        if (!value_start) value_start = strstr(ssid_name, "\n\n");
        if (value_start) {
            value_start += (value_start[0] == '\r') ? 4 : 2;  // Skip the double newline
            // Find end (next boundary starts with -)
            const char *value_end = strstr(value_start, "\r\n-");
            if (!value_end) value_end = strstr(value_start, "\n-");
            if (value_end) {
                size_t len = value_end - value_start;
                if (len >= WIFI_SSID_MAX_LEN) len = WIFI_SSID_MAX_LEN - 1;
                strncpy(creds->ssid, value_start, len);
                creds->ssid[len] = '\0';
                // Trim trailing whitespace
                while (len > 0 && (creds->ssid[len-1] == '\r' || creds->ssid[len-1] == '\n' || creds->ssid[len-1] == ' ')) {
                    creds->ssid[--len] = '\0';
                }
            }
        }
    }
    
    // Look for name="password" followed by value
    const char *pass_name = strstr(content, "name=\"password\"");
    if (pass_name) {
        const char *value_start = strstr(pass_name, "\r\n\r\n");
        if (!value_start) value_start = strstr(pass_name, "\n\n");
        if (value_start) {
            value_start += (value_start[0] == '\r') ? 4 : 2;
            const char *value_end = strstr(value_start, "\r\n-");
            if (!value_end) value_end = strstr(value_start, "\n-");
            if (value_end) {
                size_t len = value_end - value_start;
                if (len >= WIFI_PASSWORD_MAX_LEN) len = WIFI_PASSWORD_MAX_LEN - 1;
                strncpy(creds->password, value_start, len);
                creds->password[len] = '\0';
                // Trim trailing whitespace
                while (len > 0 && (creds->password[len-1] == '\r' || creds->password[len-1] == '\n' || creds->password[len-1] == ' ')) {
                    creds->password[--len] = '\0';
                }
            }
        }
    } else {
        creds->password[0] = '\0';
    }
    
    return strlen(creds->ssid) > 0;
}

// Parse URL-encoded form data (ssid=value&password=value)
static bool parse_urlencoded_form(const char *content, wifi_credentials_t *creds) {
    char *ssid_start = strstr(content, "ssid=");
    if (ssid_start == NULL) {
        return false;
    }
    
    ssid_start += 5; // Skip "ssid="
    char *ssid_end = strchr(ssid_start, '&');
    
    char ssid_encoded[WIFI_SSID_MAX_LEN * 3];
    size_t ssid_len = ssid_end ? (size_t)(ssid_end - ssid_start) : strlen(ssid_start);
    if (ssid_len >= sizeof(ssid_encoded)) ssid_len = sizeof(ssid_encoded) - 1;
    strncpy(ssid_encoded, ssid_start, ssid_len);
    ssid_encoded[ssid_len] = '\0';
    url_decode(creds->ssid, ssid_encoded, WIFI_SSID_MAX_LEN);
    
    char *pass_start = strstr(content, "password=");
    if (pass_start) {
        pass_start += 9; // Skip "password="
        char *pass_end = strchr(pass_start, '&');
        
        char pass_encoded[WIFI_PASSWORD_MAX_LEN * 3];
        size_t pass_len = pass_end ? (size_t)(pass_end - pass_start) : strlen(pass_start);
        if (pass_len >= sizeof(pass_encoded)) pass_len = sizeof(pass_encoded) - 1;
        strncpy(pass_encoded, pass_start, pass_len);
        pass_encoded[pass_len] = '\0';
        url_decode(creds->password, pass_encoded, WIFI_PASSWORD_MAX_LEN);
    } else {
        creds->password[0] = '\0';
    }
    
    return strlen(creds->ssid) > 0;
}

// Parse form data - handles both multipart and URL-encoded formats
static bool parse_form_data(const char *content, wifi_credentials_t *creds) {
    memset(creds, 0, sizeof(wifi_credentials_t));
    
    // Check if it's multipart form data (contains boundary)
    if (strstr(content, "------") != NULL || strstr(content, "boundary") != NULL || 
        strstr(content, "Content-Disposition") != NULL) {
        ESP_LOGI(TAG, "Parsing as multipart form data");
        return parse_multipart_form(content, creds);
    }
    
    // Otherwise try URL-encoded
    ESP_LOGI(TAG, "Parsing as URL-encoded form data");
    return parse_urlencoded_form(content, creds);
}

// HTTP handlers
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req) {
    char content[1024];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    ESP_LOGI(TAG, "Received form data: %s", content);
    
    wifi_credentials_t creds;
    if (parse_form_data(content, &creds)) {
        if (wifi_provision_save_credentials(creds.ssid, creds.password)) {
            httpd_resp_set_type(req, "text/html");
            httpd_resp_send(req, SUCCESS_PAGE, strlen(SUCCESS_PAGE));
            
            // Schedule reboot
            ESP_LOGI(TAG, "Configuration saved, rebooting in 3 seconds...");
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            wifi_provision_reboot();
            return ESP_OK;
        }
    }
    
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

// Captive portal redirect handler
static esp_err_t captive_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;
    
    if (httpd_start(&http_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return NULL;
    }
    
    // Main page
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };
    httpd_register_uri_handler(http_server, &root);
    
    // Save endpoint
    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler
    };
    httpd_register_uri_handler(http_server, &save);
    
    // ====== Captive portal detection endpoints ======
    // These are checked by various operating systems to detect captive portals
    
    // Android captive portal detection
    httpd_uri_t generate_204 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &generate_204);
    
    httpd_uri_t gen_204 = {
        .uri = "/gen_204",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &gen_204);
    
    // Apple/iOS captive portal detection
    httpd_uri_t hotspot = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &hotspot);
    
    httpd_uri_t library_test = {
        .uri = "/library/test/success.html",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &library_test);
    
    // Windows NCSI (Network Connectivity Status Indicator)
    httpd_uri_t ncsi = {
        .uri = "/ncsi.txt",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &ncsi);
    
    httpd_uri_t connecttest = {
        .uri = "/connecttest.txt",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &connecttest);
    
    httpd_uri_t redirect_ms = {
        .uri = "/redirect",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &redirect_ms);
    
    // Firefox captive portal detection
    httpd_uri_t success = {
        .uri = "/success.txt",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &success);
    
    // Kindle/Amazon
    httpd_uri_t kindle = {
        .uri = "/kindle-wifi/wifistub.html",
        .method = HTTP_GET,
        .handler = captive_handler
    };
    httpd_register_uri_handler(http_server, &kindle);
    
    // Generic favicon handler (prevents 404)
    httpd_uri_t favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = root_handler  // Just return main page
    };
    httpd_register_uri_handler(http_server, &favicon);
    
    ESP_LOGI(TAG, "Web server started with captive portal support");
    return http_server;
}

bool wifi_provision_start_portal(void) {
    ESP_LOGI(TAG, "Starting provisioning portal...");
    
    // Initialize TCP/IP and event loop if not already done
    esp_netif_init();
    esp_event_loop_create_default();
    
    // Create AP netif
    ap_netif = esp_netif_create_default_wifi_ap();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = PROVISION_AP_SSID,
            .ssid_len = strlen(PROVISION_AP_SSID),
            .channel = PROVISION_AP_CHANNEL,
            .password = PROVISION_AP_PASSWORD,
            .max_connection = PROVISION_AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "AP started - SSID: %s", PROVISION_AP_SSID);
    
    // Start web server
    if (start_webserver() == NULL) {
        ESP_LOGE(TAG, "Failed to start web server");
        return false;
    }
    
    // Start DNS server for captive portal (redirects all DNS to our IP)
    if (!captive_dns_start()) {
        ESP_LOGW(TAG, "DNS server failed to start, captive portal auto-popup may not work");
        // Continue anyway - users can still manually navigate to 192.168.4.1
    } else {
        ESP_LOGI(TAG, "Captive portal DNS server started");
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  Connect to WiFi: %s", PROVISION_AP_SSID);
    ESP_LOGI(TAG, "  Configuration page should open automatically!");
    ESP_LOGI(TAG, "  If not, open: http://192.168.4.1");
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "");
    
    provision_status = PROVISION_STATUS_RUNNING;
    return true;
}

void wifi_provision_stop_portal(void) {
    // Stop DNS server first
    captive_dns_stop();
    
    if (http_server) {
        httpd_stop(http_server);
        http_server = NULL;
    }
    
    esp_wifi_stop();
    esp_wifi_deinit();
    
    if (ap_netif) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    
    provision_status = PROVISION_STATUS_IDLE;
    ESP_LOGI(TAG, "Provisioning portal stopped");
}

provision_status_t wifi_provision_get_status(void) {
    return provision_status;
}

bool wifi_provision_is_configured(void) {
    return new_config_received;
}

void wifi_provision_reboot(void) {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}
