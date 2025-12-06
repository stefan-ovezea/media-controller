#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/**
 * @brief Initialize UI manager
 */
void ui_manager_init(void);

/**
 * @brief Load a specific screen
 * 
 * @param screen LVGL screen object to load
 */
void ui_load_screen(lv_obj_t *screen);

#endif // UI_MANAGER_H
