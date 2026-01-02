/**
 * Button Handler Interface
 * Handles button press detection including double-press for config mode
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

// Boot button GPIO 
// ESP32-C3: GPIO9 is the boot button
// ESP32: GPIO0 is the boot button
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C6
#define BOOT_BUTTON_GPIO        9
#else
#define BOOT_BUTTON_GPIO        0
#endif

// Double-press timing (milliseconds)
#define DOUBLE_PRESS_TIMEOUT_MS 500
#define DEBOUNCE_TIME_MS        50

// Button events
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SINGLE_PRESS,
    BUTTON_EVENT_DOUBLE_PRESS,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

/**
 * Callback function type for button events
 */
typedef void (*button_callback_t)(button_event_t event);

/**
 * Initialize the button handler
 * @return true on success
 */
bool button_handler_init(void);

/**
 * Register a callback for button events
 * @param callback Function to call on button events
 */
void button_handler_register_callback(button_callback_t callback);

/**
 * Check if double-press occurred during boot
 * Must be called early in startup before button_handler_init
 * @param timeout_ms Time to wait for second press
 * @return true if double-press detected
 */
bool button_check_double_press_boot(int timeout_ms);

/**
 * Deinitialize button handler
 */
void button_handler_deinit(void);

#endif // BUTTON_HANDLER_H
