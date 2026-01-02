/**
 * Captive DNS Server
 * Redirects all DNS queries to the ESP32's IP for captive portal functionality
 */

#ifndef CAPTIVE_DNS_H
#define CAPTIVE_DNS_H

#include <stdbool.h>

/**
 * Start the captive DNS server
 * All DNS queries will be redirected to the specified IP
 * @return true on success
 */
bool captive_dns_start(void);

/**
 * Stop the captive DNS server
 */
void captive_dns_stop(void);

#endif // CAPTIVE_DNS_H
