#include "ui_components.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "ui_components";

lv_obj_t *ui_create_media_button(lv_obj_t *parent, const char *symbol, uint16_t diameter)
{
    // Create button
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, diameter, diameter);
    
    // Style button as circular
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    
    // Create label with symbol
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(label);
    
    ESP_LOGI(TAG, "Created media button: %s, diameter: %d", symbol, diameter);
    
    return btn;
}

lv_obj_t *ui_create_album_art(lv_obj_t *parent, uint16_t size)
{
    // Create container for album art
    lv_obj_t *art = lv_obj_create(parent);
    lv_obj_set_size(art, size, size);
    
    // Style as rounded rectangle
    lv_obj_set_style_radius(art, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(art, COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(art, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(art, 0, LV_PART_MAIN);
    
    // Add music note icon in center
    lv_obj_t *icon = lv_label_create(art);
    lv_label_set_text(icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(icon, COLOR_TEXT_TERTIARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_center(icon);
    
    ESP_LOGI(TAG, "Created album art placeholder: %dx%d", size, size);
    
    return art;
}

lv_obj_t *ui_create_progress_bar(lv_obj_t *parent, uint16_t width)
{
    // Create progress bar
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, width, 4);
    
    // Style progress bar
    lv_obj_set_style_bg_color(bar, COLOR_BG_TERTIARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    
    // Set initial value
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_bar_set_range(bar, 0, 100);
    
    ESP_LOGI(TAG, "Created progress bar: %dpx wide", width);
    
    return bar;
}
