/**
 * Captive DNS Server Implementation
 * Redirects ALL DNS queries to the ESP32's AP IP address (192.168.4.1)
 * This enables true captive portal functionality where devices automatically
 * open the configuration page when connecting to the WiFi network.
 */

#include "captive_dns.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static const char *TAG = "CAPTIVE_DNS";

// DNS server settings
#define DNS_PORT 53
#define DNS_MAX_PACKET_SIZE 512

// ESP32 AP IP address (default for softAP)
#define CAPTIVE_PORTAL_IP "192.168.4.1"

// DNS task handle
static TaskHandle_t dns_task_handle = NULL;
static int dns_socket = -1;
static bool dns_running = false;

// DNS header structure
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;  // Questions
    uint16_t an_count;  // Answers
    uint16_t ns_count;  // Authority RRs
    uint16_t ar_count;  // Additional RRs
} dns_header_t;

/**
 * Build a DNS response that redirects to our IP
 * Returns the size of the response packet
 */
static int build_dns_response(uint8_t *request, int request_len, uint8_t *response) {
    if (request_len < sizeof(dns_header_t)) {
        return 0;
    }
    
    // Copy the request header
    dns_header_t *req_header = (dns_header_t *)request;
    dns_header_t *resp_header = (dns_header_t *)response;
    
    // Copy request to response
    memcpy(response, request, request_len);
    
    // Modify header for response
    resp_header->flags = htons(0x8180);  // Standard response, no error
    resp_header->an_count = htons(1);    // One answer
    resp_header->ns_count = 0;
    resp_header->ar_count = 0;
    
    // Find the end of the question section
    uint8_t *ptr = response + sizeof(dns_header_t);
    uint8_t *end = response + request_len;
    
    // Skip the domain name
    while (ptr < end && *ptr != 0) {
        ptr += *ptr + 1;
    }
    if (ptr >= end) return 0;
    ptr++;  // Skip null terminator
    
    // Skip QTYPE and QCLASS (4 bytes)
    ptr += 4;
    if (ptr > end) return 0;
    
    int question_end = ptr - response;
    
    // Add answer section
    // Name pointer (points to question name)
    *ptr++ = 0xC0;  // Pointer flag
    *ptr++ = 0x0C;  // Offset to name in question (12 bytes = header size)
    
    // Type A (host address)
    *ptr++ = 0x00;
    *ptr++ = 0x01;
    
    // Class IN (internet)
    *ptr++ = 0x00;
    *ptr++ = 0x01;
    
    // TTL (60 seconds - short so it doesn't get cached long)
    *ptr++ = 0x00;
    *ptr++ = 0x00;
    *ptr++ = 0x00;
    *ptr++ = 0x3C;
    
    // Data length (4 bytes for IPv4)
    *ptr++ = 0x00;
    *ptr++ = 0x04;
    
    // IP address (192.168.4.1)
    *ptr++ = 192;
    *ptr++ = 168;
    *ptr++ = 4;
    *ptr++ = 1;
    
    return ptr - response;
}

/**
 * DNS server task
 */
static void dns_server_task(void *pvParameters) {
    uint8_t rx_buffer[DNS_MAX_PACKET_SIZE];
    uint8_t tx_buffer[DNS_MAX_PACKET_SIZE];
    
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    // Create UDP socket
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Set socket options for reuse
    int opt = 1;
    setsockopt(dns_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to DNS port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);
    
    if (bind(dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(dns_socket);
        dns_socket = -1;
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS server started on port %d", DNS_PORT);
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(dns_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (dns_running) {
        int len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer), 0,
                          (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (len > 0) {
            // Build response redirecting to our IP
            int resp_len = build_dns_response(rx_buffer, len, tx_buffer);
            
            if (resp_len > 0) {
                sendto(dns_socket, tx_buffer, resp_len, 0,
                      (struct sockaddr *)&client_addr, client_addr_len);
                
                ESP_LOGD(TAG, "DNS query redirected to %s", CAPTIVE_PORTAL_IP);
            }
        }
    }
    
    // Cleanup
    if (dns_socket >= 0) {
        close(dns_socket);
        dns_socket = -1;
    }
    
    ESP_LOGI(TAG, "DNS server stopped");
    vTaskDelete(NULL);
}

bool captive_dns_start(void) {
    if (dns_running) {
        ESP_LOGW(TAG, "DNS server already running");
        return true;
    }
    
    dns_running = true;
    
    BaseType_t ret = xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS server task");
        dns_running = false;
        return false;
    }
    
    return true;
}

void captive_dns_stop(void) {
    if (!dns_running) {
        return;
    }
    
    dns_running = false;
    
    // Wait for task to finish
    if (dns_task_handle != NULL) {
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        dns_task_handle = NULL;
    }
}
