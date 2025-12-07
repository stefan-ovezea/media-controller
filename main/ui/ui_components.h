#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

// Color definitions
#define COLOR_BG_PRIMARY    lv_color_hex(0x000000)  // Black
#define COLOR_TEXT_PRIMARY  lv_color_hex(0xFFFFFF)  // White
#define COLOR_TEXT_SECONDARY lv_color_hex(0x888888) // Gray
#define COLOR_TEXT_TERTIARY lv_color_hex(0x666666)  // Light gray
#define COLOR_ACCENT        lv_color_hex(0x00D9FF)  // Cyan
#define COLOR_BG_SECONDARY  lv_color_hex(0x333333)  // Dark gray
#define COLOR_BG_TERTIARY   lv_color_hex(0x222222)  // Darker gray

/**
 * @brief Create a circular media control button
 * 
 * @param parent Parent object
 * @param symbol LVGL symbol (e.g., LV_SYMBOL_PLAY)
 * @param diameter Button diameter in pixels
 * @return lv_obj_t* Created button object
 */
lv_obj_t *ui_create_media_button(lv_obj_t *parent, const char *symbol, uint16_t diameter);

/**
 * @brief Create album art placeholder
 * 
 * @param parent Parent object
 * @param size Width and height in pixels
 * @return lv_obj_t* Created placeholder object
 */
lv_obj_t *ui_create_album_art(lv_obj_t *parent, uint16_t size);

/**
 * @brief Create progress bar with time labels
 * 
 * @param parent Parent object
 * @param width Bar width in pixels
 * @return lv_obj_t* Created progress bar object
 */
lv_obj_t *ui_create_progress_bar(lv_obj_t *parent, uint16_t width);

#endif // UI_COMPONENTS_H
