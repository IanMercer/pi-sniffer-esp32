/**
 * HTTP Client Interface
 * Handles REST API communication
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include "device.h"

/**
 * HTTP response structure
 */
typedef struct {
    int status_code;
    char *body;
    size_t body_len;
} http_response_t;

/**
 * Initialize HTTP client
 * @return true on success
 */
bool http_client_init(void);

/**
 * Send device data to the REST API
 * @param list Pointer to device list
 * @param device_id Unique identifier for this sensor
 * @return true on success
 */
bool http_send_devices(const device_list_t *list, const char *device_id);

/**
 * Send a generic JSON POST request
 * @param host Server hostname
 * @param port Server port
 * @param path API path
 * @param json_body JSON body to send
 * @param response Pointer to response structure (can be NULL)
 * @return true on success
 */
bool http_post_json(const char *host, 
                    int port, 
                    const char *path,
                    const char *json_body,
                    http_response_t *response);

/**
 * Free response body memory
 * @param response Pointer to response structure
 */
void http_response_free(http_response_t *response);

/**
 * Deinitialize HTTP client
 */
void http_client_deinit(void);

#endif // HTTP_CLIENT_H
