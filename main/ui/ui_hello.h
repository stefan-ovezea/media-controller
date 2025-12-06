#ifndef UI_HELLO_H
#define UI_HELLO_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/**
 * @brief Create and return the Hello World screen
 * 
 * @return lv_obj_t* Pointer to the created screen
 */
lv_obj_t *ui_hello_create(void);

#endif // UI_HELLO_H
