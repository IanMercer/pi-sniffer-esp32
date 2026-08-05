/**
 * MAC-Rotation Packing
 *
 * Detects when a BLE device's random MAC address has likely rotated (a
 * privacy feature of iOS/Android) so the same physical device isn't counted
 * twice. Ported from the original pi-sniffer project's overlaps.c /
 * pack_closest_columns(), simplified for a single access point and the flat
 * device_list_t model used by this firmware (see the ESP32 port's plan for
 * the full mapping from the original multi-access-point algorithm).
 */

#ifndef MAC_PACK_H
#define MAC_PACK_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include "device.h"

/**
 * Do two observation windows overlap in time? If they do, the two MACs
 * cannot be the same physical device. Touching endpoints (one window's
 * first_seen equal to the other's last_seen) do not count as overlap.
 */
bool mac_pack_overlaps(time_t a_first, time_t a_last, time_t b_first, time_t b_last);

/**
 * Was the older or newer device's data too sparse/oddly-timed to trust as a
 * real MAC rotation? True if either side has a single observation and the
 * gap between them is under MAC_PACK_BLIP_MIN_GAP_SEC (same burst, noise) or
 * over MAC_PACK_BLIP_MAX_GAP_SEC (too far apart to be related).
 */
bool mac_pack_is_blip(time_t older_first, time_t older_last, uint32_t older_count,
                       time_t newer_first, time_t newer_last, uint32_t newer_count);

/**
 * Could `newer` plausibly be `older` after a MAC rotation, based on address
 * type, category, and name? Does not check timing.
 */
bool mac_pack_compatible(const ble_device_t *older, const ble_device_t *newer);

/**
 * Confidence [0,1] that `newer` is `older` after a MAC rotation, given the
 * pair already passed the compatibility/overlap/blip checks. Based purely on
 * the time gap between older's last sighting and newer's first sighting.
 */
float mac_pack_probability(const ble_device_t *older, const ble_device_t *newer);

/**
 * Run a full pass over the device list, resetting and recomputing the
 * has_superseded_by / superseded_by / superseded_probability fields for
 * every active device.
 */
void mac_pack_run(device_list_t *list);

#endif // MAC_PACK_H
