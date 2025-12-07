/**
 * ESP32C6 Media Controller
 * Main application entry point
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_config.h"
#include "network/wifi_manager.h"
#include "network/mqtt_handler.h"
#include "display/display_driver.h"
#include "display/lvgl_setup.h"
#include "ui/ui_manager.h"
#include "ui/ui_media.h"

static const char *TAG = APP_TAG;

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32C6 Media Controller Starting ===");
    ESP_LOGI(TAG, "Display: %dx%d (%s)", 
             LCD_H_RES, LCD_V_RES, 
             DISPLAY_ORIENTATION == ORIENTATION_ROTATE ? "Landscape" : "Portrait");
    
    // Initialize display FIRST to secure DMA memory before WiFi/MQTT
    ESP_LOGI(TAG, "Initializing display...");
    esp_lcd_panel_handle_t panel_handle = display_init();
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    
    // Initialize LVGL early to allocate DMA buffers
    esp_err_t ret = lvgl_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return;
    }
    
    // Initialize UI manager and create UI (allocates thumbnail buffer)
    ui_manager_init();
    
    // Create Media Player screen early to allocate thumbnail buffer
    lv_obj_t *media_screen = NULL;
    if (lvgl_lock(-1)) {
        media_screen = ui_media_create();
        lvgl_unlock();
    }
    
    // NOW initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return;
    }
    
    // Connect to WiFi
    ret = wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return;
    }
    
    // Initialize MQTT after display is ready
    ESP_LOGI(TAG, "Initializing MQTT...");
    ret = mqtt_handler_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT");
        return;
    }
    
    // Share the thumbnail buffer with MQTT handler to avoid duplicate allocation
    size_t thumb_buf_size = 0;
    uint8_t *thumb_buf = ui_media_get_thumbnail_buffer(&thumb_buf_size);
    mqtt_handler_set_thumbnail_buffer(thumb_buf, thumb_buf_size);
    ESP_LOGI(TAG, "Shared thumbnail buffer: %p (%zu bytes)", thumb_buf, thumb_buf_size);
    
    // Start MQTT client
    ret = mqtt_handler_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT");
        return;
    }
    
    // Load the media screen now that everything is initialized
    if (media_screen != NULL && lvgl_lock(-1)) {
        ui_load_screen(media_screen);
        lvgl_unlock();
    }
    
    ESP_LOGI(TAG, "=== Media Controller Initialized Successfully ===");
}