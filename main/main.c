/**
 * ESP32C6 Media Controller
 * Main application entry point
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_config.h"
#include "display/display_driver.h"
#include "display/lvgl_setup.h"
#include "ui/ui_manager.h"
#include "ui/ui_hello.h"

static const char *TAG = APP_TAG;

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32C6 Media Controller Starting ===");
    ESP_LOGI(TAG, "Display: %dx%d (%s)", 
             LCD_H_RES, LCD_V_RES, 
             DISPLAY_ORIENTATION == ORIENTATION_ROTATE ? "Landscape" : "Portrait");
    
    // Initialize display hardware
    esp_lcd_panel_handle_t panel_handle = display_init();
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    
    // Initialize LVGL
    esp_err_t ret = lvgl_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return;
    }
    
    // Initialize UI manager
    ui_manager_init();
    
    // Create and load Hello World screen
    if (lvgl_lock(-1)) {
        lv_obj_t *hello_screen = ui_hello_create();
        ui_load_screen(hello_screen);
        lvgl_unlock();
    }
    
    ESP_LOGI(TAG, "=== Media Controller Initialized Successfully ===");
    ESP_LOGI(TAG, "Phase 1 Complete: Hello World displayed");
}