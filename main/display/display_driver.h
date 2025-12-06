#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "esp_lcd_panel_ops.h"
#include "esp_err.h"

/**
 * @brief Initialize the display hardware
 * 
 * @return esp_lcd_panel_handle_t Handle to the LCD panel, or NULL on failure
 */
esp_lcd_panel_handle_t display_init(void);

#endif // DISPLAY_DRIVER_H
