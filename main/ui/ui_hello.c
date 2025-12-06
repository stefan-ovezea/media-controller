#include "ui_hello.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "ui_hello";

lv_obj_t *ui_hello_create(void)
{
    ESP_LOGI(TAG, "Creating Hello World screen");
    
    // Create a new screen
    lv_obj_t *screen = lv_obj_create(NULL);
    
    // Set background to pure black
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    
    // Create a simple label for "Hello World"
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Hello World");
    
    // Style the label - white text
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    
    // Center the label on the screen
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    ESP_LOGI(TAG, "Hello World screen created");
    
    return screen;
}
