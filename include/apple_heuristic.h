/**
 * Apple Continuity Protocol Heuristics
 */

#ifndef APPLE_HEURISTIC_H
#define APPLE_HEURISTIC_H

#include <stdint.h>
#include "device.h"

/**
 * Decode an Apple manufacturer-specific advertisement payload (Continuity
 * protocol) and use whatever the message type reveals to improve the
 * device's name/category.
 *
 * @param device Device to update
 * @param payload Manufacturer-specific data with the 2-byte Apple company ID
 *        (0x004C) already stripped off, i.e. payload[0] is the Continuity
 *        message type
 * @param payload_len Length of payload
 */
void apple_heuristic_process(ble_device_t *device, const uint8_t *payload, uint8_t payload_len);

#endif // APPLE_HEURISTIC_H
