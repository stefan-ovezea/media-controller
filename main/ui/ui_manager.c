#include "ui_manager.h"
#include "esp_log.h"

static const char *TAG = "ui_manager";

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "UI Manager initialized");
}

void ui_load_screen(lv_obj_t *screen)
{
    if (screen == NULL) {
        ESP_LOGE(TAG, "Cannot load NULL screen");
        return;
    }
    
    lv_scr_load(screen);
    ESP_LOGI(TAG, "Screen loaded");
}
