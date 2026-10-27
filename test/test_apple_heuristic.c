/**
 * Host-side unit tests for apple_heuristic.c.
 *
 * Not part of the ESP-IDF/PlatformIO firmware build. Compile and run with a
 * plain host compiler, e.g.:
 *
 *   gcc -I../include -o /tmp/test_apple_heuristic test_apple_heuristic.c \
 *       ../src/apple_heuristic.c ../src/device.c ../src/kalman.c -lm
 *   /tmp/test_apple_heuristic
 */

#include "apple_heuristic.h"
#include "device.h"

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

static ble_device_t make_blank_device(void) {
    ble_device_t d;
    memset(&d, 0, sizeof(d));
    strncpy(d.mac_str, "AA:BB:CC:DD:EE:FF", sizeof(d.mac_str) - 1);
    return d;
}

static void test_beacon_hard_sets_category(void) {
    ble_device_t d = make_blank_device();
    d.category = CATEGORY_TABLET;  // pretend something else already guessed wrong

    uint8_t payload[] = {0x02, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_BEACON, "iBeacon (0x02) hard-overwrites any existing category");
    CHECK(strcmp(d.name, "Beacon") == 0, "iBeacon (0x02) sets the name to Beacon");
}

static void test_airpods_hard_sets_headphones(void) {
    ble_device_t d = make_blank_device();
    d.category = CATEGORY_PHONE;  // pretend something else already guessed wrong

    uint8_t payload[] = {0x07, 0x00, 0x00, 0x01, 0x02, 0x03};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_HEADPHONES, "Proximity Pairing (0x07) hard-overwrites any existing category");
    CHECK(strcmp(d.name, "AirPods") == 0, "Proximity Pairing (0x07) sets the name to AirPods");
}

static void test_airdrop_soft_sets_phone(void) {
    ble_device_t d = make_blank_device();

    uint8_t payload[] = {0x05, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_PHONE, "AirDrop (0x05) soft-sets category to phone");
    CHECK(strcmp(d.name, "AirDrop") == 0, "AirDrop (0x05) sets the name");

    // Soft-set: a category that's already been claimed must not be overwritten
    ble_device_t d2 = make_blank_device();
    d2.category = CATEGORY_TABLET;
    apple_heuristic_process(&d2, payload, sizeof(payload));
    CHECK(d2.category == CATEGORY_TABLET, "AirDrop (0x05) does not overwrite an already-known category");
}

static void test_airplay_soft_sets_fixed(void) {
    ble_device_t d = make_blank_device();

    uint8_t payload[] = {0x09, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_FIXED, "AirPlay (0x09) soft-sets category to fixed");
}

static void test_magic_switch_names_iwatch(void) {
    ble_device_t d = make_blank_device();

    uint8_t payload[] = {0x0b, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_WATCH, "Magic Switch (0x0b) soft-sets category to watch");
    CHECK(strcmp(d.name, "iWatch") == 0, "Magic Switch (0x0b) names the device iWatch");
}

static void test_nearby_info_iphone(void) {
    ble_device_t d = make_blank_device();

    // device_bit=1 (bit 1 of byte 2 set), information_byte=0x1a
    uint8_t payload[] = {0x10, 0x06, 0x02, 0x1a, 0x00, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_PHONE, "Nearby Info di=1,info=0x1a decodes as a phone");
    CHECK(strncmp(d.name, "iPhone", 6) == 0, "Nearby Info di=1,info=0x1a names it an iPhone");
}

static void test_nearby_info_ipad(void) {
    ble_device_t d = make_blank_device();

    // device_bit=0, information_byte=0x1d
    uint8_t payload[] = {0x10, 0x06, 0x00, 0x1d, 0x00, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_TABLET, "Nearby Info di=0,info=0x1d decodes as a tablet");
    CHECK(strncmp(d.name, "iPad", 4) == 0, "Nearby Info di=0,info=0x1d names it an iPad");
}

static void test_nearby_info_macbook(void) {
    ble_device_t d = make_blank_device();

    // device_bit=0, information_byte=0x19 - confirmed MacBook Air (observed
    // appearing/disappearing in lockstep with toggling the laptop's
    // Bluetooth off and on).
    uint8_t payload[] = {0x10, 0x06, 0x00, 0x19, 0x00, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_COMPUTER, "Nearby Info di=0,info=0x19 decodes as a computer");
    CHECK(strncmp(d.name, "MacBook", 7) == 0, "Nearby Info di=0,info=0x19 names it a MacBook");
}

static void test_nearby_info_macbook_other_device_bit(void) {
    ble_device_t d = make_blank_device();

    // device_bit=1, information_byte=0x19 - inferred (not independently
    // confirmed like di=019 above) by the same pattern already established
    // for iPad, where a device type spans both device_bit values for the
    // same information_byte.
    uint8_t payload[] = {0x10, 0x06, 0x02, 0x19, 0x00, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_COMPUTER, "Nearby Info di=1,info=0x19 decodes as a computer");
    CHECK(strncmp(d.name, "MacBook", 7) == 0, "Nearby Info di=1,info=0x19 names it a MacBook");
}

static void test_nearby_info_watch(void) {
    ble_device_t d = make_blank_device();

    // device_bit=0, information_byte=0x00
    uint8_t payload[] = {0x10, 0x06, 0x00, 0x00, 0x00, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_WATCH, "Nearby Info di=0,info=0x00 decodes as a watch");
}

static void test_nearby_info_too_short_is_ignored(void) {
    ble_device_t d = make_blank_device();

    uint8_t payload[] = {0x10, 0x06, 0x00};  // missing the information byte
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_UNKNOWN, "a truncated Nearby Info payload is safely ignored");
    CHECK(d.name[0] == '\0', "a truncated Nearby Info payload does not set a name");
}

static void test_name_confidence_precedence(void) {
    ble_device_t d = make_blank_device();

    device_set_name(&d, "Apple", NAME_CONF_MANUFACTURER);
    CHECK(strcmp(d.name, "Apple") == 0, "a manufacturer-level name gets set");

    device_set_name(&d, "Generic", NAME_CONF_GENERIC);
    CHECK(strcmp(d.name, "Apple") == 0, "a lower-confidence name does not overwrite a higher one");

    device_set_name(&d, "AirPods", NAME_CONF_DEVICE);
    CHECK(strcmp(d.name, "AirPods") == 0, "a higher-confidence name overwrites a lower one");

    device_set_name(&d, "Ian's iPhone", NAME_CONF_KNOWN);
    CHECK(strcmp(d.name, "Ian's iPhone") == 0, "the real advertised name outranks any heuristic guess");

    device_set_name(&d, "AirDrop", NAME_CONF_MANUFACTURER);
    CHECK(strcmp(d.name, "Ian's iPhone") == 0, "a heuristic guess never overwrites the real advertised name");
}

static void test_unknown_type_is_ignored(void) {
    ble_device_t d = make_blank_device();

    uint8_t payload[] = {0xEE, 0x00};
    apple_heuristic_process(&d, payload, sizeof(payload));

    CHECK(d.category == CATEGORY_UNKNOWN, "an unrecognized Continuity type sets no category");
    CHECK(d.name[0] == '\0', "an unrecognized Continuity type sets no name");
}

static void test_empty_payload_is_ignored(void) {
    ble_device_t d = make_blank_device();
    apple_heuristic_process(&d, NULL, 0);
    CHECK(d.category == CATEGORY_UNKNOWN, "a zero-length payload is a safe no-op");
}

static void test_apple_unrecognized_first_packet_not_phone(void) {
    // Observed on hardware: an Apple TV whose first packet was an
    // unrecognized Continuity type got device_categorize()'s Apple ->
    // phone guess, which then blocked AirPlay's soft-set to fixed forever.
    static device_list_t list;
    device_list_init(&list);
    uint8_t mac[6] = {0x47, 0x86, 0x4F, 0x26, 0x3C, 0xB3};

    uint8_t unknown_type[] = {0x16, 0x00};
    ble_device_t *d = device_update(&list, mac, -70, ADDRESS_TYPE_RANDOM, NULL,
                                    0x004C /* Apple */, TX_POWER_UNKNOWN,
                                    unknown_type, sizeof(unknown_type));
    CHECK(d && d->category == CATEGORY_UNKNOWN,
          "Apple device with only an unrecognized type stays uncategorized");

    uint8_t airplay[] = {0x09, 0x06, 0x13, 0x24, 0xC0, 0xA8, 0x00, 0xF5};
    d = device_update(&list, mac, -70, ADDRESS_TYPE_RANDOM, NULL,
                      0x004C /* Apple */, TX_POWER_UNKNOWN,
                      airplay, sizeof(airplay));
    CHECK(d && d->category == CATEGORY_FIXED, "a later AirPlay packet still categorizes it as fixed");
}

static void test_unidentified_random_address_not_phone(void) {
    // Observed on hardware: an Insta360 camera, an Oura Ring and unnamed
    // IoT sensors were all counted as phones via the old random-address
    // fallback.
    static device_list_t list;
    device_list_init(&list);
    uint8_t cam[6] = {0xFD, 0xEB, 0xCC, 0x34, 0xE5, 0x5E};
    uint8_t ring[6] = {0x58, 0xCD, 0x34, 0x85, 0xE4, 0x03};
    uint8_t payload[] = {0x01, 0x00, 0x00, 0x00, 0x00};

    ble_device_t *d = device_update(&list, cam, -90, ADDRESS_TYPE_RANDOM, "GO 3 87RFAH",
                                    0x4F47, TX_POWER_UNKNOWN, payload, sizeof(payload));
    CHECK(d && d->category == CATEGORY_UNKNOWN, "unidentified random-address device is not a phone");

    d = device_update(&list, ring, -90, ADDRESS_TYPE_RANDOM, "Oura Ring 4",
                      0x02B2, TX_POWER_UNKNOWN, payload, sizeof(payload));
    CHECK(d && d->category == CATEGORY_WEARABLE, "Oura Ring is a wearable");
}

int main(void) {
    test_beacon_hard_sets_category();
    test_airpods_hard_sets_headphones();
    test_airdrop_soft_sets_phone();
    test_airplay_soft_sets_fixed();
    test_magic_switch_names_iwatch();
    test_nearby_info_iphone();
    test_nearby_info_ipad();
    test_nearby_info_macbook();
    test_nearby_info_macbook_other_device_bit();
    test_nearby_info_watch();
    test_nearby_info_too_short_is_ignored();
    test_name_confidence_precedence();
    test_unknown_type_is_ignored();
    test_empty_payload_is_ignored();
    test_apple_unrecognized_first_packet_not_phone();
    test_unidentified_random_address_not_phone();

    if (failures == 0) {
        printf("\nAll tests passed.\n");
        return 0;
    }
    printf("\n%d test(s) failed.\n", failures);
    return 1;
}
