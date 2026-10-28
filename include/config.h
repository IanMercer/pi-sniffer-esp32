/**
 * ESP32 BLE Sniffer Configuration
 * Modify these settings to match your environment
 */

#ifndef CONFIG_H
#define CONFIG_H

// Board-specific capability flags (REPORT_TRANSPORT, ENABLE_CELLULAR,
// ENABLE_GNSS) below are #ifndef-guarded so a PlatformIO env can override
// them via build_flags - e.g. env:esp32c5 has no SIM7670G modem, so it
// builds WiFi-only. The values here are the defaults for the S3 + modem board.

// Deployment-specific network endpoints (API_URL, MQTT_BROKER_*,
// CELLULAR_APN) live in secrets.h, which is gitignored - not checked into
// source control. If this doesn't compile because secrets.h is missing,
// copy include/secrets.h.example to include/secrets.h and fill in your own
// values.
#include "secrets.h"

// ============================================================================
// WiFi Configuration
// ============================================================================
// WiFi credentials are now configured via captive portal
// On first boot or double-press BOOT button, device creates "DC Sniffer" AP
// Connect to it and open http://192.168.4.1 to configure WiFi

#define WIFI_MAX_RETRY      10

// Use WIFI_SSID/WIFI_PASSWORD from secrets.h instead of the captive-portal
// provisioning flow - skips the boot double-press window and the "DC
// Sniffer" AP entirely, and disables the runtime double/long-press
// credential reset. Set per board via build_flags (env:esp32c5).
#ifndef USE_STATIC_WIFI_CREDENTIALS
#define USE_STATIC_WIFI_CREDENTIALS 0
#endif

#if USE_STATIC_WIFI_CREDENTIALS && (!defined(WIFI_SSID) || !defined(WIFI_PASSWORD))
#error "USE_STATIC_WIFI_CREDENTIALS needs WIFI_SSID and WIFI_PASSWORD in secrets.h"
#endif

// ============================================================================
// Reporting Configuration
// ============================================================================
// Which transport report_task() publishes device reports over, selected per
// deployment - some units have WiFi, some are permanently off-grid on
// cellular only. Exactly one of these ever runs: run_normal_mode() only
// starts the one background task (mqtt_report_task, cellular_task, or
// cellular_http_task) this setting actually needs, never more than one (see
// plans/4g-integration.md's "Resolved decisions").
typedef enum {
    REPORT_TRANSPORT_WIFI,
    REPORT_TRANSPORT_CELLULAR_MQTT,      // MQTT over cellular (AT+CMQTT*, cellular.c)
    REPORT_TRANSPORT_CELLULAR_HTTP,      // HTTP POST over cellular (AT+HTTP*, cellular_http.c) -
                                         // legacy full-device JSON format, see cellular_http.h
} report_transport_t;

<<<<<<< HEAD
#ifndef REPORT_TRANSPORT
#define REPORT_TRANSPORT           REPORT_TRANSPORT_CELLULAR_HTTP
#endif
=======
#define REPORT_TRANSPORT           REPORT_TRANSPORT_WIFI
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)

// Disable sending device reports entirely (set to 1 to disable, 0 to enable)
#define DISABLE_API_SEND    0

// Default API endpoint placeholder if needed by legacy HTTP implementations
#ifndef API_URL
#define API_URL             "your_api_endpoint_here"
#endif

// MQTT broker - both the WiFi path (esp-mqtt, needs a full URI) and the
// cellular path (AT+CMQTTCONNECT, needs host/port split out) connect here.
<<<<<<< HEAD
// Publicly reachable hostname (not a LAN-only address) - needed for the
// cellular path, which isn't on the same network as a local broker would
// be. Plain (unencrypted) MQTT on the standard port for now; TLS is still an
// open question, see plans/4g-integration.md.
// MQTT_BROKER_HOST/PORT/URI are defined in secrets.h (gitignored) - see
// this file's top comment. Optional MQTT_AUTH_USER/MQTT_AUTH_PASS, also in
// secrets.h, add username/password auth on both paths (mqtt_report.c's
// esp_mqtt_client_config_t credentials, cellular.c's AT+CMQTTCONNECT
// trailing username/password params) only when both are defined.

// Full publish topic is "<prefix>/<device_id>" - one topic per unit, same
// JSON body device_json_build() produces today regardless of transport.
=======
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#define MQTT_TOPIC_PREFIX           "reports"
#define MQTT_QOS                    1
#define MQTT_KEEPALIVE_SEC          60

#define WIFI_RECONNECT_INTERVAL_SEC 300

// ============================================================================
// Cellular Configuration (SIM7670G modem-side AT+CMQTT*)
// ============================================================================
<<<<<<< HEAD
// Enable the cellular MQTT transport (cellular.c). Independent of
// ENABLE_GNSS - both share the modem's UART via modem_uart.h - but there's
// no point bringing up the modem's data connection on boards/deployments
// that only need WiFi.
#ifndef ENABLE_CELLULAR
#define ENABLE_CELLULAR             1
#endif

// CELLULAR_APN is defined in secrets.h (gitignored) - see this file's top
// comment. No PDP username/password required for this deployment's SIM.

=======
#define ENABLE_CELLULAR             1
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#define CELLULAR_AT_TIMEOUT_MS       10000
#define CELLULAR_MQTT_CLIENT_INDEX   0
#define CELLULAR_CMQTTSTART_TIMEOUT_MS 12000
#define CELLULAR_RECONNECT_BACKOFF_SEC 300
#define CELLULAR_MQTT_MAX_PAYLOAD_LEN 4096
#define CELLULAR_POSITION_UNCHANGED_THRESHOLD_M 20.0f
#define CELLULAR_HEARTBEAT_INTERVAL_SEC          3600

<<<<<<< HEAD
// ----------------------------------------------------------------------
// Cellular HTTP reporting (SIM7670G modem-side AT+HTTP*, cellular_http.c) -
// alternative to REPORT_TRANSPORT_CELLULAR_MQTT (MQTT) above for the same
// metered SIM. POSTs the legacy full-device JSON (device_json_build(), see
// device_json.h) to API_URL (defined in secrets.h, gitignored - see this
// file's top comment), using the modem's own AT+HTTP* command set instead
// of esp_http_client (which needs a PPP/TCP-IP stack this project
// deliberately doesn't bring up over cellular - see
// plans/4g-integration.md's transport decision). API_URL/API_TIMEOUT_MS are
// also still referenced by http_client.c's now-unused WiFi/esp_http_client
// path (http_send_devices(), no longer called by report_task()) - kept as
// one shared endpoint/timeout rather than duplicated, since both are
// posting the same JSON shape to the same place. Throttled by the same
// CELLULAR_POSITION_UNCHANGED_THRESHOLD_M/CELLULAR_HEARTBEAT_INTERVAL_SEC
// above (see report_throttle.h) - the legacy payload is much larger than
// cellular MQTT's minimal one, so sending it unconditionally every
// REPORT_INTERVAL_SEC isn't viable on a metered plan.
=======
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#define API_TIMEOUT_MS      10000
#define API_SKIP_CERT_CHECK 1
<<<<<<< HEAD

// Optional HTTP Basic Auth for API_URL - API_AUTH_USER/API_AUTH_PASS, defined
// in secrets.h only when the endpoint requires it. cellular_http.c sends it via
// AT+HTTPPARA="USERDATA",... only when both are defined, so deployments
// without them are unaffected.

// AT+HTTPACTION's result arrives later as an unsolicited "+HTTPACTION" URC,
// not synchronously with the command's own OK - this bounds how long
// cellular_http_task waits for that URC before giving up. Needs on-device
// verification, same as CELLULAR_CMQTTSTART_TIMEOUT_MS was - starting
// conservative per SIMCom's HTTP(S) application note's documented worst case
// (a slow far-end server/DNS lookup).
=======
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#define CELLULAR_HTTP_ACTION_TIMEOUT_MS 120000

// ============================================================================
// BLE Scanning Configuration
// ============================================================================
#define SCAN_DURATION_SEC       10
#define REPORT_INTERVAL_SEC     30
#define MAX_DEVICES             128
#define MAX_DEVICE_AGE_SEC      300
#define DEVICE_MIN_SEEN_COUNT_FOR_SUMMARY 2
#define MIN_RSSI_FOR_SUMMARY -105

// ============================================================================
// RSSI to Distance Configuration
// ============================================================================
#define RSSI_ONE_METER          -80
#define PATH_LOSS_EXPONENT      2.5f
#define MAX_DISTANCE_METERS     50.0f
#define DISTANCE_BUCKET_NEAR_M   5.0f
#define DISTANCE_BUCKET_MID_M    10.0f

// ============================================================================
// Device Identification
// ============================================================================
#define ENABLE_NAME_RESOLUTION  1
#define ENABLE_CATEGORIZATION   1
#define ENABLE_APPLE_HEURISTICS 1

// ============================================================================
// MAC-Rotation Packing Configuration
// ============================================================================
#define ENABLE_MAC_PACKING             1
#define MAC_PACK_BLIP_MIN_GAP_SEC      2
#define MAC_PACK_BLIP_MAX_GAP_SEC      90
#define MAC_PACK_TIME_CONSTANT_SEC     30.0f
#define MAC_PACK_PROBABILITY_THRESHOLD 0.05f
#define MAC_PACK_MIN_PAYLOAD_MATCH_LEN 3
#define MAC_PACK_MAX_TRACKED_STREAMS    4

// ============================================================================
// GNSS Configuration (SIM7670G onboard modem)
// ============================================================================
<<<<<<< HEAD
// Poll the SIM7670G's GNSS receiver over UART via AT commands and log the
// fix (lat/long/height/time) on a regular interval.
#ifndef ENABLE_GNSS
#define ENABLE_GNSS              1
#endif

// gnss_poll_now() is called exactly once per report cycle, synchronously,
// from report_task() (main.c) - there's no independent GNSS polling task,
// so this happens on report_task's own thread, once every
// REPORT_INTERVAL_SEC, whichever transport (WiFi or cellular) is actually
// configured to send.

// UART peripheral and pins used to talk to the modem's AT command port.
// Board docs give GPIO17/18 for this link but swapped from our original
// guess: GPIO18 is the ESP32-S3's TX (into the modem's RX), GPIO17 is the
// ESP32-S3's RX (from the modem's TX). UART_NUM_1, 115200 baud, 1024-byte
// RX buffer confirmed correct.
=======
#define ENABLE_GNSS              1
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#define GNSS_UART_NUM            UART_NUM_1
#define GNSS_UART_TX_PIN         18   // ESP32-S3 -> modem RX
#define GNSS_UART_RX_PIN         17   // modem TX -> ESP32-S3
#define GNSS_UART_BAUD           115200
#define GNSS_POWERON_WAIT_SEC    10
#define GNSS_AT_SYNC_ATTEMPTS    20
#define GNSS_AT_SYNC_RETRY_MS    2000
#define GNSS_CLOCK_SYNC_THRESHOLD_SEC 2

// ============================================================================
// Debugging
// ============================================================================
<<<<<<< HEAD
// Log individual device discoveries - the per-packet "Device: ..." line in
// on_device_discovered() (main.c). Off by default: at any real device
// density this is by far the noisiest log source, one line per BLE
// advertisement seen. Independent of ble_scanner.c's own (lower-level, less
// detailed) discovery line, which this same flag also gates.
#ifndef DEBUG_DEVICE_DISCOVERY
#define DEBUG_DEVICE_DISCOVERY  1
#endif

// Append each device's per-Continuity-type stream windows (see
// stream_window_t in device.h) to its line in the device summary, e.g.
// "streams: 0c[150s-90s] 10[150s-1s]". Lets you directly see, for two
// devices that mac_pack chose not to link, whether they share a stream
// type with overlapping activity (a real conflict) or genuinely disjoint
// stream types/windows (a possible missed ragged-handover link) - useful
// when investigating a device count that looks too high or too low.
=======
#define DEBUG_DEVICE_DISCOVERY  0
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#define DEBUG_STREAM_WINDOWS    1
#define PHONES_ONLY             0

<<<<<<< HEAD
// Only log devices categorized as CATEGORY_PHONE - suppresses the per-scan
// "Device: ..." discovery line (main.c) and the per-device row in the
// summary listing (main.c) for everything else (tvs, beacons, tablets,
// appliances, unknown, etc.), for a much shorter log when all you care about
// is phones. Does not affect the summary's aggregate category counts
// (Total/Phones/Computers/.../Beacons/Other) - those stay in the log as a
// compact crowd overview regardless of this setting.
#define PHONES_ONLY             0

=======
>>>>>>> 55484d1 (Resolved merge conflicts between master and upstream/master)
#endif // CONFIG_H