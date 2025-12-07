#include "ui_media.h"
#include "ui_components.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "display/lvgl_setup.h"
#include "esp_lv_decoder.h"  // ESP LVGL decoder for JPEG/PNG
#include "network/mqtt_handler.h"
#include "cJSON.h"

static const char *TAG = "ui_media";

// UI element references
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_bg_img = NULL;  // Background image for album art
static lv_obj_t *g_gradient = NULL;  // Initial gradient background (deleted when first thumbnail arrives)
static lv_obj_t *g_img_gradient = NULL;  // Gradient overlay on image (persists)
static lv_obj_t *g_title_label = NULL;
static lv_obj_t *g_artist_label = NULL;
static lv_obj_t *g_play_btn = NULL;
static lv_obj_t *g_play_label = NULL;
static lv_obj_t *g_progress_bar = NULL;

// Thumbnail image data - must persist for LVGL and shared with MQTT
// Reduced to 20KB to fit 64x64 PNG (typically 3-8KB compressed)
// For larger thumbnails, reduce quality or size in Home Assistant
#define MAX_THUMBNAIL_SIZE (20 * 1024)  // 20KB max for compressed PNG
static uint8_t *g_thumbnail_data = NULL;
static int g_thumbnail_len = 0;
static lv_img_dsc_t g_thumbnail_dsc;

// Decoded image data (persists for LVGL to display)
static uint8_t *g_decoded_image_data = NULL;

// Media state
static media_state_t g_media_state = {
    .title = "Waiting for data...",
    .artist = "Connect to MQTT",
    .duration_sec = 0,
    .position_sec = 0,
    .is_playing = false
};

// Timer for progress updates
static TimerHandle_t g_progress_timer = NULL;

// Forward declarations
static void play_pause_event_cb(lv_event_t *e);
static void prev_event_cb(lv_event_t *e);
static void next_event_cb(lv_event_t *e);
static void update_ui(void);
static void format_time(char *buf, uint32_t seconds);
static void progress_timer_cb(TimerHandle_t timer);

// No callback functions needed for esp_lv_decoder - it's simpler!

lv_obj_t *ui_media_create(void)
{
    ESP_LOGI(TAG, "Creating media player screen");
    
    // Create screen
    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // === BACKGROUND: Gradient (thumbnail disabled due to RAM constraints) ===
    
    // Note: Album art thumbnail disabled - ESP32-C6 doesn't have enough RAM for PNG decode
    // PNG needs ~360KB for 300x300 RGBA image, but we only have ~33KB free
    
    // Create a nice gradient background instead
    g_bg_img = lv_obj_create(g_screen);
    lv_obj_set_size(g_bg_img, LCD_H_RES, LCD_V_RES);
    lv_obj_align(g_bg_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_bg_img, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_bg_img, lv_color_hex(0x16213e), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(g_bg_img, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_clear_flag(g_bg_img, LV_OBJ_FLAG_SCROLLABLE);
    
    // Allocate thumbnail buffer (for potential future use with JPEG)
    if (g_thumbnail_data == NULL) {
        g_thumbnail_data = heap_caps_malloc(MAX_THUMBNAIL_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (g_thumbnail_data == NULL) {
            // Fallback to regular RAM if no PSRAM
            g_thumbnail_data = malloc(MAX_THUMBNAIL_SIZE);
        }
        ESP_LOGI(TAG, "Thumbnail buffer allocated: %p (display disabled)", g_thumbnail_data);
    }
    
    // Gradient overlay (fade from left to right) - will be deleted when thumbnail arrives
    g_gradient = lv_obj_create(g_screen);
    lv_obj_set_size(g_gradient, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_bg_opa(g_gradient, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(g_gradient, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_gradient, COLOR_BG_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_gradient, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_stop(g_gradient, 180, LV_PART_MAIN);  // Fade starts at 70%
    lv_obj_set_style_border_width(g_gradient, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_gradient, 0, LV_PART_MAIN);
    lv_obj_align(g_gradient, LV_ALIGN_CENTER, 0, 0);
    
    // === LEFT SIDE: Song Info + Controls ===
    
    // Song title (top left)
    g_title_label = lv_label_create(g_screen);
    lv_label_set_text(g_title_label, g_media_state.title);
    lv_obj_set_style_text_color(g_title_label, COLOR_TEXT_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_title_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(g_title_label, LV_ALIGN_TOP_LEFT, 20, 30);
    
    // Artist name (below title)
    g_artist_label = lv_label_create(g_screen);
    lv_label_set_text(g_artist_label, g_media_state.artist);
    lv_obj_set_style_text_color(g_artist_label, COLOR_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_artist_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(g_artist_label, LV_ALIGN_TOP_LEFT, 20, 58);
    
    // === MEDIA CONTROLS (Icon only, no circles) ===
    
    // Previous button (icon only)
    lv_obj_t *prev_btn = lv_btn_create(g_screen);
    lv_obj_set_size(prev_btn, 40, 40);
    lv_obj_set_style_bg_opa(prev_btn, LV_OPA_TRANSP, LV_PART_MAIN);  // Transparent background
    lv_obj_set_style_shadow_width(prev_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(prev_btn, 0, LV_PART_MAIN);
    lv_obj_align(prev_btn, LV_ALIGN_BOTTOM_LEFT, 20, -40);
    lv_obj_add_event_cb(prev_btn, prev_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *prev_icon = lv_label_create(prev_btn);
    lv_label_set_text(prev_icon, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(prev_icon, COLOR_TEXT_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(prev_icon, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(prev_icon);

    // Play/Pause button (icon only, larger)
    g_play_btn = lv_btn_create(g_screen);
    lv_obj_set_size(g_play_btn, 50, 50);
    lv_obj_set_style_bg_opa(g_play_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_play_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_play_btn, 0, LV_PART_MAIN);
    lv_obj_align(g_play_btn, LV_ALIGN_BOTTOM_LEFT, 75, -37);
    lv_obj_add_event_cb(g_play_btn, play_pause_event_cb, LV_EVENT_CLICKED, NULL);

    g_play_label = lv_label_create(g_play_btn);
    lv_label_set_text(g_play_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(g_play_label, COLOR_TEXT_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_play_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(g_play_label);

    // Next button (icon only)
    lv_obj_t *next_btn = lv_btn_create(g_screen);
    lv_obj_set_size(next_btn, 40, 40);
    lv_obj_set_style_bg_opa(next_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(next_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(next_btn, 0, LV_PART_MAIN);
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_LEFT, 140, -40);
    lv_obj_add_event_cb(next_btn, next_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *next_icon = lv_label_create(next_btn);
    lv_label_set_text(next_icon, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(next_icon, COLOR_TEXT_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(next_icon, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(next_icon);
    
    // === PROGRESS BAR (Bottom, full width) ===
    
    g_progress_bar = ui_create_progress_bar(g_screen, LCD_H_RES - 40);
    lv_obj_align(g_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -15);
    
    // Create progress timer (1 second interval)
    g_progress_timer = xTimerCreate("progress", pdMS_TO_TICKS(1000), pdTRUE, NULL, progress_timer_cb);
    
    ESP_LOGI(TAG, "Media player screen created");
    
    return g_screen;
}

static void play_pause_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Play/Pause button clicked");

    // Send play or pause command based on current state
    const char *command = g_media_state.is_playing ? "pause" : "play";

    // Create JSON payload: {"command": "play", "data": null}
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", command);
    cJSON_AddNullToObject(json, "data");

    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        mqtt_handler_publish(MQTT_TOPIC_CMD, json_str, strlen(json_str), 0, 0);
        free(json_str);
    }
    cJSON_Delete(json);
}

static void prev_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Previous button clicked");

    // Create JSON payload: {"command": "previous", "data": null}
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", "previous");
    cJSON_AddNullToObject(json, "data");

    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        mqtt_handler_publish(MQTT_TOPIC_CMD, json_str, strlen(json_str), 0, 0);
        free(json_str);
    }
    cJSON_Delete(json);
}

static void next_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Next button clicked");

    // Create JSON payload: {"command": "next", "data": null}
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", "next");
    cJSON_AddNullToObject(json, "data");

    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        mqtt_handler_publish(MQTT_TOPIC_CMD, json_str, strlen(json_str), 0, 0);
        free(json_str);
    }
    cJSON_Delete(json);
}

static void progress_timer_cb(TimerHandle_t timer)
{
    // Progress is now controlled by MQTT updates
    // This timer is kept for potential future use
}

static void update_ui(void)
{
    // Update song info
    lv_label_set_text(g_title_label, g_media_state.title);
    lv_label_set_text(g_artist_label, g_media_state.artist);
    
    // Reset progress
    lv_bar_set_value(g_progress_bar, 0, LV_ANIM_OFF);
}

static void format_time(char *buf, uint32_t seconds)
{
    uint32_t mins = seconds / 60;
    uint32_t secs = seconds % 60;
    snprintf(buf, 16, "%" PRIu32 ":%02" PRIu32, mins, secs);
}

void ui_media_update_state(const media_state_t *state)
{
    // Safety check - make sure UI is initialized
    if (g_title_label == NULL || g_artist_label == NULL || g_play_label == NULL || g_progress_bar == NULL) {
        ESP_LOGW(TAG, "UI not initialized yet, skipping update");
        return;
    }
    
    if (state) {
        // Copy state
        strncpy(g_media_state.title, state->title, sizeof(g_media_state.title) - 1);
        strncpy(g_media_state.artist, state->artist, sizeof(g_media_state.artist) - 1);
        g_media_state.duration_sec = state->duration_sec;
        g_media_state.position_sec = state->position_sec;
        g_media_state.is_playing = state->is_playing;

        // Truncate title if too long (max 25 chars, truncate to 22 + "...")
        size_t title_len = strlen(g_media_state.title);
        if (title_len > 25) {
            g_media_state.title[22] = '.';
            g_media_state.title[23] = '.';
            g_media_state.title[24] = '.';
            g_media_state.title[25] = '\0';
        }

        // Update UI elements
        lv_label_set_text(g_title_label, g_media_state.title);
        lv_label_set_text(g_artist_label, g_media_state.artist);
        
        // Update play/pause icon
        if (g_media_state.is_playing) {
            lv_label_set_text(g_play_label, LV_SYMBOL_PAUSE);
        } else {
            lv_label_set_text(g_play_label, LV_SYMBOL_PLAY);
        }
        
        // Update progress bar
        if (g_media_state.duration_sec > 0) {
            uint32_t progress = (g_media_state.position_sec * 100) / g_media_state.duration_sec;
            lv_bar_set_value(g_progress_bar, progress, LV_ANIM_OFF);
        }
        
        ESP_LOGI(TAG, "UI updated: %s - %s [%s]", 
                 g_media_state.title, g_media_state.artist,
                 g_media_state.is_playing ? "playing" : "paused");
    }
}

uint8_t *ui_media_get_thumbnail_buffer(size_t *size)
{
    if (size != NULL) {
        *size = MAX_THUMBNAIL_SIZE;
    }
    return g_thumbnail_data;
}

void ui_media_update_thumbnail(const uint8_t *data, int data_len)
{
    if (data == NULL || data_len <= 0) {
        ESP_LOGW(TAG, "Invalid thumbnail data");
        return;
    }

    if (g_thumbnail_data == NULL) {
        ESP_LOGE(TAG, "Thumbnail buffer not allocated");
        return;
    }

    if (data_len > MAX_THUMBNAIL_SIZE) {
        ESP_LOGW(TAG, "Thumbnail too large: %d bytes (max %d)", data_len, MAX_THUMBNAIL_SIZE);
        return;
    }

    // Check available heap
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Updating thumbnail: %d bytes (free heap: %" PRIu32 ")",
             data_len, (uint32_t)free_heap);

    // Detect image format
    if (data_len >= 4) {
        ESP_LOGI(TAG, "Thumbnail header: %02X %02X %02X %02X",
                 data[0], data[1], data[2], data[3]);

        if (data[0] == 0xFF && data[1] == 0xD8) {
            ESP_LOGI(TAG, "Detected JPEG format");
        } else if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
            ESP_LOGI(TAG, "Detected PNG format");
        } else {
            ESP_LOGW(TAG, "Unknown image format");
            return;
        }
    }

    // CRITICAL: Copy JPEG data to persistent buffer!
    // The 'data' pointer from MQTT will be freed after this function returns
    memcpy(g_thumbnail_data, data, data_len);
    g_thumbnail_len = data_len;

    if (lvgl_lock(1000)) {
        // Delete the gradient overlay on first thumbnail
        if (g_gradient != NULL) {
            lv_obj_del(g_gradient);
            g_gradient = NULL;
            ESP_LOGI(TAG, "Removed gradient overlay");
        }

        // Clean up old image widget
        if (g_bg_img != NULL) {
            lv_obj_del(g_bg_img);
            g_bg_img = NULL;
        }

        // Free old decoded image data from decoder
        if (g_decoded_image_data != NULL) {
            free(g_decoded_image_data);
            g_decoded_image_data = NULL;
            ESP_LOGI(TAG, "Freed old decoded image data");
        }

        // Create new image widget
        g_bg_img = lv_img_create(g_screen);
        if (g_bg_img == NULL) {
            ESP_LOGE(TAG, "Failed to create image object");
            lvgl_unlock();
            return;
        }

        // Setup LVGL image descriptor pointing to PERSISTENT buffer
        g_thumbnail_dsc.header.always_zero = 0;
        g_thumbnail_dsc.header.cf = LV_IMG_CF_RAW;  // Raw compressed data (JPEG/PNG)
        g_thumbnail_dsc.header.w = 0;  // Will be determined by decoder
        g_thumbnail_dsc.header.h = 0;
        g_thumbnail_dsc.data_size = g_thumbnail_len;
        g_thumbnail_dsc.data = g_thumbnail_data;  // Points to persistent buffer!

        // Set the image source - LVGL will decode it automatically!
        lv_img_set_src(g_bg_img, &g_thumbnail_dsc);

        // Position image on the RIGHT side of screen, filling the height
        // Your screen is 320x170 (landscape), so make image fill the right side
        lv_obj_set_size(g_bg_img, LCD_V_RES, LCD_V_RES);  // 170x170 square on right
        lv_obj_align(g_bg_img, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_clear_flag(g_bg_img, LV_OBJ_FLAG_SCROLLABLE);

        // Enable image scaling to fill the area
        lv_obj_set_style_transform_pivot_x(g_bg_img, LCD_V_RES / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(g_bg_img, LCD_V_RES / 2, LV_PART_MAIN);

        // Full opacity for album art (no transparency)
        lv_obj_set_style_img_opa(g_bg_img, LV_OPA_COVER, LV_PART_MAIN);

        // Move to background (behind text/controls)
        lv_obj_move_background(g_bg_img);

        // Create gradient overlay ONLY ONCE (not every time!)
        if (g_img_gradient == NULL) {
            g_img_gradient = lv_obj_create(g_screen);
            lv_obj_set_size(g_img_gradient, LCD_H_RES, LCD_V_RES);
            lv_obj_set_style_bg_opa(g_img_gradient, LV_OPA_TRANSP, LV_PART_MAIN);  // Transparent background
            lv_obj_set_style_bg_grad_dir(g_img_gradient, LV_GRAD_DIR_HOR, LV_PART_MAIN);
            lv_obj_set_style_bg_color(g_img_gradient, COLOR_BG_PRIMARY, LV_PART_MAIN);  // Solid on left
            lv_obj_set_style_bg_grad_color(g_img_gradient, lv_color_hex(0x000000), LV_PART_MAIN);  // Fade to transparent on right
            lv_obj_set_style_bg_grad_stop(g_img_gradient, 180, LV_PART_MAIN);  // Gradient starts at ~70%
            lv_obj_set_style_border_width(g_img_gradient, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(g_img_gradient, 0, LV_PART_MAIN);
            lv_obj_align(g_img_gradient, LV_ALIGN_CENTER, 0, 0);

            // Position it between image and text (above image, below UI controls)
            lv_obj_move_to_index(g_img_gradient, 1);

            ESP_LOGI(TAG, "Created gradient overlay");
        }

        lvgl_unlock();
        ESP_LOGI(TAG, "Thumbnail displayed");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock");
    }

    ESP_LOGI(TAG, "Free heap after decode: %" PRIu32 " bytes", (uint32_t)esp_get_free_heap_size());
}
