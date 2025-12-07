#ifndef UI_MEDIA_H
#define UI_MEDIA_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

// Media state structure
typedef struct {
    char title[128];
    char artist[128];
    uint32_t duration_sec;  // Total duration in seconds
    uint32_t position_sec;  // Current position in seconds
    bool is_playing;
} media_state_t;

/**
 * @brief Create and return the media player screen
 * 
 * @return lv_obj_t* Pointer to the created screen
 */
lv_obj_t *ui_media_create(void);

/**
 * @brief Update media state (for future backend integration)
 * 
 * @param state New media state
 */
void ui_media_update_state(const media_state_t *state);

/**
 * @brief Update album art thumbnail from JPEG data
 * 
 * @param data JPEG image data
 * @param data_len Length of image data
 */
void ui_media_update_thumbnail(const uint8_t *data, int data_len);

/**
 * @brief Get the thumbnail buffer pointer and size for MQTT to use
 * This avoids duplicate buffer allocation
 * 
 * @param size Output parameter for buffer size
 * @return uint8_t* Pointer to thumbnail buffer
 */
uint8_t *ui_media_get_thumbnail_buffer(size_t *size);

#endif // UI_MEDIA_H
