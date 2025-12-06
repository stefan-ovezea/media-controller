#ifndef LVGL_SETUP_H
#define LVGL_SETUP_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief Initialize LVGL library and create display driver
 * 
 * @param panel_handle Handle to the LCD panel from display_init()
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_init(esp_lcd_panel_handle_t panel_handle);

/**
 * @brief Lock LVGL mutex for thread-safe operations
 * 
 * @param timeout_ms Timeout in milliseconds, -1 for infinite
 * @return true if lock acquired, false otherwise
 */
bool lvgl_lock(int timeout_ms);

/**
 * @brief Unlock LVGL mutex
 */
void lvgl_unlock(void);

#endif // LVGL_SETUP_H
