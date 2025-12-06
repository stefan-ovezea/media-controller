#include "lvgl_setup.h"
#include "app_config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "lvgl_setup";
static SemaphoreHandle_t lvgl_mux = NULL;

// Forward declarations
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_tick_timer_cb(void *arg);
static void lvgl_task(void *arg);

// Store panel handle for flush callback
static esp_lcd_panel_handle_t g_panel_handle = NULL;

esp_err_t lvgl_init(esp_lcd_panel_handle_t panel_handle)
{
    ESP_LOGI(TAG, "Initializing LVGL");
    
    g_panel_handle = panel_handle;
    
    // Initialize LVGL
    lv_init();
    
    // Allocate display buffers (double buffering)
    lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * LCD_DMA_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (buf1 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffer 1");
        return ESP_ERR_NO_MEM;
    }
    
    lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * LCD_DMA_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffer 2");
        free(buf1);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize display buffer
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * LCD_DMA_LINES);
    
    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);
    
    // Create LVGL tick timer
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_timer_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
    
    // Create mutex for LVGL thread safety
    lvgl_mux = xSemaphoreCreateMutex();
    if (lvgl_mux == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Create LVGL task
    xTaskCreate(lvgl_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
    
    ESP_LOGI(TAG, "LVGL initialized successfully");
    return ESP_OK;
}

bool lvgl_lock(int timeout_ms)
{
    if (lvgl_mux == NULL) {
        ESP_LOGE(TAG, "LVGL mutex not initialized");
        return false;
    }
    
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    if (lvgl_mux == NULL) {
        ESP_LOGE(TAG, "LVGL mutex not initialized");
        return;
    }
    xSemaphoreGive(lvgl_mux);
}

// LVGL flush callback - sends buffer to display
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    
    // Apply display offset based on orientation
#if (DISPLAY_ORIENTATION == ORIENTATION_NORMAL)
    int offsetx1 = area->x1 + 35;
    int offsetx2 = area->x2 + 35;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
#elif (DISPLAY_ORIENTATION == ORIENTATION_ROTATE)
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1 + 35;
    int offsety2 = area->y2 + 35;
#endif
    
    // Draw bitmap to display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    
    // CRITICAL: Tell LVGL we're done flushing
    lv_disp_flush_ready(drv);
}

// LVGL tick timer callback
static void lvgl_tick_timer_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// LVGL task - handles LVGL timer and rendering
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    
    while (1) {
        // Lock mutex for LVGL operations
        if (lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            lvgl_unlock();
        }
        
        // Clamp delay to reasonable bounds
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}
