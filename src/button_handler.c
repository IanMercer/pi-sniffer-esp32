/**
 * Button Handler Implementation
 * Detects button presses including double-press for entering config mode
 */

#include "button_handler.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "BUTTON_HANDLER";

static button_callback_t button_callback = NULL;
static QueueHandle_t button_queue = NULL;
static volatile uint32_t last_press_time = 0;
static volatile int press_count = 0;

/**
 * GPIO ISR handler
 */
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(button_queue, &gpio_num, NULL);
}

/**
 * Button task - processes button presses
 */
static void button_task(void *arg) {
    uint32_t gpio_num;
    uint32_t current_time;
    button_event_t event;
    
    while (1) {
        if (xQueueReceive(button_queue, &gpio_num, portMAX_DELAY)) {
            // Debounce
            vTaskDelay(DEBOUNCE_TIME_MS / portTICK_PERIOD_MS);
            
            // Check if button is still pressed
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                
                // Check for double press
                if (current_time - last_press_time < DOUBLE_PRESS_TIMEOUT_MS) {
                    press_count++;
                } else {
                    press_count = 1;
                }
                last_press_time = current_time;
                
                // Wait for button release to determine press type
                uint32_t press_start = current_time;
                while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    if (xTaskGetTickCount() * portTICK_PERIOD_MS - press_start > 2000) {
                        // Long press detected
                        event = BUTTON_EVENT_LONG_PRESS;
                        ESP_LOGI(TAG, "Long press detected");
                        if (button_callback) {
                            button_callback(event);
                        }
                        press_count = 0;
                        break;
                    }
                }
                
                // If not a long press, wait to see if it's a double press
                if (press_count > 0) {
                    vTaskDelay(DOUBLE_PRESS_TIMEOUT_MS / portTICK_PERIOD_MS);
                    
                    if (press_count >= 2) {
                        event = BUTTON_EVENT_DOUBLE_PRESS;
                        ESP_LOGI(TAG, "Double press detected");
                    } else {
                        event = BUTTON_EVENT_SINGLE_PRESS;
                        ESP_LOGI(TAG, "Single press detected");
                    }
                    
                    if (button_callback) {
                        button_callback(event);
                    }
                    press_count = 0;
                }
            }
        }
    }
}

bool button_handler_init(void) {
    // Configure button GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);
    
    // Create queue for button events
    button_queue = xQueueCreate(10, sizeof(uint32_t));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return false;
    }
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_BUTTON_GPIO, gpio_isr_handler, (void *)BOOT_BUTTON_GPIO);
    
    // Create button task
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Button handler initialized on GPIO%d", BOOT_BUTTON_GPIO);
    return true;
}

void button_handler_register_callback(button_callback_t callback) {
    button_callback = callback;
}

bool button_check_double_press_boot(int timeout_ms) {
    // Configure button GPIO for polling (before ISR setup)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "Checking for double-press to enter config mode...");
    ESP_LOGI(TAG, "Press BOOT button twice within %dms to enter config mode", timeout_ms);
    
    int presses = 0;
    int elapsed = 0;
    bool button_was_pressed = false;
    
    while (elapsed < timeout_ms) {
        bool button_pressed = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);
        
        if (button_pressed && !button_was_pressed) {
            // Button just pressed
            presses++;
            ESP_LOGI(TAG, "Button press %d detected", presses);
            
            if (presses >= 2) {
                ESP_LOGI(TAG, "Double-press detected! Entering config mode...");
                return true;
            }
        }
        
        button_was_pressed = button_pressed;
        vTaskDelay(50 / portTICK_PERIOD_MS);
        elapsed += 50;
    }
    
    ESP_LOGI(TAG, "No double-press detected, continuing normal boot");
    return false;
}

void button_handler_deinit(void) {
    gpio_isr_handler_remove(BOOT_BUTTON_GPIO);
    gpio_uninstall_isr_service();
    
    if (button_queue) {
        vQueueDelete(button_queue);
        button_queue = NULL;
    }
    
    button_callback = NULL;
    ESP_LOGI(TAG, "Button handler deinitialized");
}
