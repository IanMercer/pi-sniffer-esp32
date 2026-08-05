/**
 * Host-side unit tests for mac_pack.c.
 *
 * Not part of the ESP-IDF/PlatformIO firmware build. Compile and run with a
 * plain host compiler, e.g.:
 *
 *   gcc -I../include -o /tmp/test_mac_pack test_mac_pack.c \
 *       ../src/mac_pack.c ../src/device.c ../src/kalman.c \
 *       ../src/apple_heuristic.c -lm
 *   /tmp/test_mac_pack
 */

#include "mac_pack.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);    \
            failures++;                                               \
        } else {                                                      \
            printf("ok:   %s\n", msg);                                \
        }                                                              \
    } while (0)

static ble_device_t make_device(uint8_t mac_last_byte, time_t first_seen, time_t last_seen,
                                 uint32_t seen_count, address_type_t addr_type,
                                 device_category_t category, const char *name) {
    ble_device_t d;
    memset(&d, 0, sizeof(d));
    d.mac[5] = mac_last_byte;
    d.first_seen = first_seen;
    d.last_seen = last_seen;
    d.seen_count = seen_count;
    d.address_type = addr_type;
    d.category = category;
    d.active = true;
    if (name) {
        strncpy(d.name, name, MAX_NAME_LENGTH - 1);
    }
    return d;
}

static void test_overlap(void) {
    // Windows that share time cannot be the same rotated device.
    CHECK(mac_pack_overlaps(0, 10, 5, 15) == true, "overlapping windows overlap");
    CHECK(mac_pack_overlaps(0, 10, 10, 20) == false, "touching windows do not overlap");
    CHECK(mac_pack_overlaps(0, 10, 20, 30) == false, "disjoint windows do not overlap");
    CHECK(mac_pack_overlaps(20, 30, 0, 10) == false, "disjoint windows do not overlap (reversed)");
}

static void test_blip(void) {
    // Single-observation device with a very short gap: same burst, noise.
    CHECK(mac_pack_is_blip(0, 100, 1, 101, 101, 5) == true, "single obs + 1s gap is a blip");
    // Single-observation device with a very long gap: too far apart to trust.
    CHECK(mac_pack_is_blip(0, 100, 1, 300, 300, 5) == true, "single obs + 200s gap is a blip");
    // Single-observation device with a plausible gap: not a blip.
    CHECK(mac_pack_is_blip(0, 100, 1, 105, 105, 5) == false, "single obs + 5s gap is not a blip");
    // Both sides have multiple observations: never a blip, regardless of gap.
    CHECK(mac_pack_is_blip(0, 100, 10, 101, 101, 10) == false, "multi-obs short gap is not a blip");
    CHECK(mac_pack_is_blip(0, 100, 10, 300, 300, 10) == false, "multi-obs long gap is not a blip");
}

static void test_compatible(void) {
    ble_device_t older = make_device(1, 0, 100, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "Ian's iPhone");
    ble_device_t newer = make_device(2, 105, 200, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "Ian's iPhone");
    older.name_confidence = NAME_CONF_KNOWN;
    newer.name_confidence = NAME_CONF_KNOWN;
    CHECK(mac_pack_compatible(&older, &newer) == true, "matching address/category/name is compatible");

    ble_device_t public_older = older;
    public_older.address_type = ADDRESS_TYPE_PUBLIC;
    CHECK(mac_pack_compatible(&public_older, &newer) == false, "public address is never compatible");

    ble_device_t public_newer = newer;
    public_newer.address_type = ADDRESS_TYPE_PUBLIC;
    CHECK(mac_pack_compatible(&older, &public_newer) == false, "public address is never compatible (newer side)");

    ble_device_t different_addr_newer = newer;
    different_addr_newer.address_type = ADDRESS_TYPE_UNKNOWN;
    CHECK(mac_pack_compatible(&older, &different_addr_newer) == true,
          "unknown address type on either side does not block a match");

    ble_device_t unknown_category_older = older;
    unknown_category_older.category = CATEGORY_UNKNOWN;
    CHECK(mac_pack_compatible(&unknown_category_older, &newer) == false,
          "older device with unknown category is never compatible (matches original asymmetric behavior)");

    ble_device_t different_category_newer = newer;
    different_category_newer.category = CATEGORY_TABLET;
    CHECK(mac_pack_compatible(&older, &different_category_newer) == false,
          "differing known categories are incompatible");

    ble_device_t conflicting_name_newer = newer;
    strncpy(conflicting_name_newer.name, "Someone Else's Pixel", MAX_NAME_LENGTH - 1);
    CHECK(mac_pack_compatible(&older, &conflicting_name_newer) == false,
          "conflicting non-empty names are incompatible");

    ble_device_t empty_name_newer = newer;
    empty_name_newer.name[0] = '\0';
    CHECK(mac_pack_compatible(&older, &empty_name_newer) == true,
          "an empty name on either side does not block a match");

    // Heuristic-derived names (anything below NAME_CONF_KNOWN) are inherently
    // volatile - e.g. Apple Continuity guesses differ by message type and
    // even lock/screen state for the same physical phone - so a mismatch
    // there must NOT block a match the way a real advertised name does.
    ble_device_t heuristic_older = make_device(3, 0, 100, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "iPhone di=01f");
    heuristic_older.name_confidence = NAME_CONF_MANUFACTURER;
    ble_device_t heuristic_newer = make_device(4, 105, 200, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "Apple di=11f");
    heuristic_newer.name_confidence = NAME_CONF_MANUFACTURER;
    CHECK(mac_pack_compatible(&heuristic_older, &heuristic_newer) == true,
          "conflicting heuristic-confidence names do not block a match");

    // The name check only fires when BOTH sides are real known names - a
    // known name on one side and a mere heuristic guess on the other is
    // not a conflict either (the guess isn't trustworthy enough to compare).
    ble_device_t known_vs_guess_older = heuristic_older;
    known_vs_guess_older.name_confidence = NAME_CONF_KNOWN;
    strncpy(known_vs_guess_older.name, "Ian's iPhone", MAX_NAME_LENGTH - 1);
    CHECK(mac_pack_compatible(&known_vs_guess_older, &heuristic_newer) == true,
          "a known name vs. a lower-confidence guess is not treated as a conflict");
}

static void test_probability(void) {
    ble_device_t older = make_device(1, 0, 100, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "phone");
    ble_device_t near = make_device(2, 105, 200, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "phone");
    ble_device_t far = make_device(3, 100 + (time_t)(MAC_PACK_TIME_CONSTANT_SEC * 10), 500, 5,
                                    ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "phone");

    float near_prob = mac_pack_probability(&older, &near);
    float far_prob = mac_pack_probability(&older, &far);

    CHECK(near_prob > 0.8f, "a short gap yields high confidence");
    CHECK(far_prob < 0.01f, "a very long gap yields near-zero confidence");
    CHECK(near_prob > far_prob, "a shorter gap is always more confident than a longer one");
}

static void test_run_links_and_dedupes(void) {
    device_list_t list;
    memset(&list, 0, sizeof(list));

    // Same physical phone: MAC 1 active 0-100, then rotates to MAC 2 at 105-200.
    list.devices[0] = make_device(1, 0, 100, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");
    list.devices[1] = make_device(2, 105, 200, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");

    // Unrelated device, active the whole time (overlaps everything): must never be linked.
    list.devices[2] = make_device(3, 0, 200, 20, ADDRESS_TYPE_RANDOM, CATEGORY_TABLET, "");

    mac_pack_run(&list);

    CHECK(list.devices[0].has_superseded_by == true, "older MAC gets linked to its rotated successor");
    CHECK(memcmp(list.devices[0].superseded_by, list.devices[1].mac, 6) == 0,
          "the link points at the correct newer MAC");
    CHECK(list.devices[1].has_superseded_by == false, "the newer MAC is not itself marked superseded");
    CHECK(list.devices[2].has_superseded_by == false, "an overlapping unrelated device is never linked");

    device_summary_t summary;
    device_get_summary(&list, &summary);
    CHECK(summary.total_devices == 2, "deduped summary counts the rotated pair once");
}

static void test_run_chain_of_rotations(void) {
    device_list_t list;
    memset(&list, 0, sizeof(list));

    // Same physical device rotating MAC twice in a row: A0 -> A1 -> A2.
    list.devices[0] = make_device(1, 0, 100, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");
    list.devices[1] = make_device(2, 105, 150, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");
    list.devices[2] = make_device(3, 155, 200, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");

    mac_pack_run(&list);

    CHECK(list.devices[0].has_superseded_by == true, "first rotation in a chain is linked");
    CHECK(memcmp(list.devices[0].superseded_by, list.devices[1].mac, 6) == 0,
          "first device links to the second, not the third");
    CHECK(list.devices[1].has_superseded_by == true, "second rotation in a chain is linked");
    CHECK(memcmp(list.devices[1].superseded_by, list.devices[2].mac, 6) == 0,
          "second device links to the third");
    CHECK(list.devices[2].has_superseded_by == false, "the newest device in the chain has no successor");
}

static void test_run_older_device_claimed_at_most_once(void) {
    device_list_t list;
    memset(&list, 0, sizeof(list));

    // A0 is a valid, non-overlapping predecessor for both A1 and A2, but A1
    // and A2 overlap each other, so A1 is not eligible as A2's predecessor.
    // A2 (processed first, being newest) claims A0. A1 must then find no
    // available predecessor, even though it individually matches A0 fine.
    list.devices[0] = make_device(1, 0, 50, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");
    list.devices[1] = make_device(2, 60, 140, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");
    list.devices[2] = make_device(3, 130, 300, 5, ADDRESS_TYPE_RANDOM, CATEGORY_PHONE, "");

    mac_pack_run(&list);

    CHECK(mac_pack_overlaps(60, 140, 130, 300) == true, "test setup: device 1 and 2 do overlap each other");
    CHECK(list.devices[0].has_superseded_by == true, "the oldest device is claimed by exactly one successor");
    CHECK(memcmp(list.devices[0].superseded_by, list.devices[2].mac, 6) == 0,
          "the newest device claims it first, since devices are processed newest-first");
    CHECK(list.devices[1].has_superseded_by == false,
          "a device that overlaps the claimant cannot also claim the same predecessor");
}

int main(void) {
    test_overlap();
    test_blip();
    test_compatible();
    test_probability();
    test_run_links_and_dedupes();
    test_run_chain_of_rotations();
    test_run_older_device_claimed_at_most_once();

    if (failures == 0) {
        printf("\nAll tests passed.\n");
        return 0;
    }
    printf("\n%d test(s) failed.\n", failures);
    return 1;
}
