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
#include "pngle.h"  // Lightweight PNG decoder

static const char *TAG = "ui_media";

// UI element references
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_bg_img = NULL;  // Background image for album art
static lv_obj_t *g_title_label = NULL;
static lv_obj_t *g_artist_label = NULL;
static lv_obj_t *g_play_btn = NULL;
static lv_obj_t *g_play_label = NULL;
static lv_obj_t *g_progress_bar = NULL;

// Thumbnail image data - must persist for LVGL and shared with MQTT
#define MAX_THUMBNAIL_SIZE (160 * 1024)  // 160KB max to handle larger album art
static uint8_t *g_thumbnail_data = NULL;
static int g_thumbnail_len = 0;
static lv_img_dsc_t g_thumbnail_dsc;

// Pngle decoder context for incremental decoding
typedef struct {
    lv_img_dsc_t *img_dsc;
    lv_color_t *line_buf;  // Single line buffer for RGB565 conversion
    uint32_t width;
    uint32_t height;
    uint32_t current_y;
    uint8_t *decoded_data;  // RGB565 output buffer
} pngle_decode_ctx_t;

static pngle_decode_ctx_t g_pngle_ctx = {0};

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

// Pngle callback for PNG initialization
static void pngle_on_init(pngle_t *pngle, uint32_t w, uint32_t h)
{
    pngle_decode_ctx_t *ctx = (pngle_decode_ctx_t *)pngle_get_user_data(pngle);
    
    ESP_LOGI(TAG, "PNG init: %" PRIu32 "x%" PRIu32, w, h);
    ctx->width = w;
    ctx->height = h;
    ctx->current_y = 0;
    
    // Allocate output buffer for RGB565 data (2 bytes per pixel)
    size_t img_size = w * h * sizeof(lv_color_t);
    ctx->decoded_data = heap_caps_malloc(img_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ctx->decoded_data == NULL) {
        ctx->decoded_data = malloc(img_size);
    }
    
    if (ctx->decoded_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for decoded image", img_size);
        return;
    }
    
    ESP_LOGI(TAG, "Allocated %zu bytes for decoded image", img_size);
    
    // Allocate line buffer for RGB conversion (RGBA input)
    ctx->line_buf = malloc(w * sizeof(lv_color_t));
    if (ctx->line_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        free(ctx->decoded_data);
        ctx->decoded_data = NULL;
        return;
    }
}

// Pngle callback for each decoded pixel
static void pngle_on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4])
{
    pngle_decode_ctx_t *ctx = (pngle_decode_ctx_t *)pngle_get_user_data(pngle);
    
    if (ctx->decoded_data == NULL || ctx->line_buf == NULL) {
        return;
    }
    
    // Convert RGBA8888 to RGB565 and store in output buffer
    lv_color_t *pixel = (lv_color_t *)(ctx->decoded_data + (y * ctx->width + x) * sizeof(lv_color_t));
    
    // Convert RGBA to RGB565 (LVGL color format)
    // Note: LVGL RGB565 format has different layouts on different platforms
    #if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0
        pixel->full = ((rgba[0] & 0xF8) << 8) | ((rgba[1] & 0xFC) << 3) | (rgba[2] >> 3);
    #else
        pixel->ch.red = rgba[0] >> 3;      // 8-bit to 5-bit
        pixel->ch.green_h = rgba[1] >> 2;    // 8-bit to 6-bit  
        pixel->ch.blue = rgba[2] >> 3;     // 8-bit to 5-bit
    #endif
    // Alpha channel (rgba[3]) is ignored for now
}

// Pngle callback when decoding is done
static void pngle_on_done(pngle_t *pngle)
{
    pngle_decode_ctx_t *ctx = (pngle_decode_ctx_t *)pngle_get_user_data(pngle);
    ESP_LOGI(TAG, "PNG decode complete: %" PRIu32 "x%" PRIu32, ctx->width, ctx->height);
    
    // Free line buffer, we don't need it anymore
    if (ctx->line_buf) {
        free(ctx->line_buf);
        ctx->line_buf = NULL;
    }
}

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
    
    // Gradient overlay (fade from left to right)
    lv_obj_t *gradient = lv_obj_create(g_screen);
    lv_obj_set_size(gradient, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_bg_opa(gradient, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(gradient, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_bg_color(gradient, COLOR_BG_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(gradient, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_stop(gradient, 180, LV_PART_MAIN);  // Fade starts at 70%
    lv_obj_set_style_border_width(gradient, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(gradient, 0, LV_PART_MAIN);
    lv_obj_align(gradient, LV_ALIGN_CENTER, 0, 0);
    
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
    lv_obj_align(prev_btn, LV_ALIGN_BOTTOM_LEFT, 20, -50);
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
    lv_obj_align(g_play_btn, LV_ALIGN_BOTTOM_LEFT, 75, -47);
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
    lv_obj_align(next_btn, LV_ALIGN_BOTTOM_LEFT, 140, -50);
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
    // Display-only mode - buttons do nothing for now
    ESP_LOGI(TAG, "Play/Pause button clicked (no action)");
}

static void prev_event_cb(lv_event_t *e)
{
    // Display-only mode - buttons do nothing for now
    ESP_LOGI(TAG, "Previous button clicked (no action)");
}

static void next_event_cb(lv_event_t *e)
{
    // Display-only mode - buttons do nothing for now
    ESP_LOGI(TAG, "Next button clicked (no action)");
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
    
    // Check available heap before decoding
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Updating thumbnail: %d bytes (free heap: %" PRIu32 ", min: %" PRIu32 ")", 
             data_len, (uint32_t)free_heap, (uint32_t)min_free_heap);
    
    // Debug: Check first few bytes to verify image format
    if (data_len >= 4) {
        ESP_LOGI(TAG, "Thumbnail header: %02X %02X %02X %02X", 
                 data[0], data[1], data[2], data[3]);
        
        // Detect format - pngle only supports PNG
        if (data[0] == 0xFF && data[1] == 0xD8) {
            ESP_LOGW(TAG, "JPEG format detected, but pngle only supports PNG");
            ESP_LOGW(TAG, "Please configure Home Assistant to send PNG thumbnails");
            return;
        } else if (data[0] != 0x89 || data[1] != 0x50 || data[2] != 0x4E || data[3] != 0x47) {
            ESP_LOGW(TAG, "Unknown image format (not PNG)");
            return;
        }
        ESP_LOGI(TAG, "Detected PNG format - decoding with pngle");
    }
    
    // Initialize pngle decoder
    ESP_LOGI(TAG, "Creating pngle decoder (free heap: %" PRIu32 " bytes)...", (uint32_t)esp_get_free_heap_size());
    pngle_t *pngle = pngle_new();
    if (pngle == NULL) {
        ESP_LOGE(TAG, "Failed to create pngle decoder (out of memory)");
        ESP_LOGE(TAG, "Free heap: %" PRIu32 " bytes", (uint32_t)esp_get_free_heap_size());
        return;
    }
    ESP_LOGI(TAG, "pngle decoder created successfully");
    
    // Set up context and callbacks
    memset(&g_pngle_ctx, 0, sizeof(g_pngle_ctx));
    g_pngle_ctx.img_dsc = &g_thumbnail_dsc;
    pngle_set_user_data(pngle, &g_pngle_ctx);
    pngle_set_init_callback(pngle, pngle_on_init);
    pngle_set_draw_callback(pngle, pngle_on_draw);
    pngle_set_done_callback(pngle, pngle_on_done);
    
    // Feed PNG data to pngle incrementally
    ESP_LOGI(TAG, "Feeding %d bytes to pngle decoder...", data_len);
    int fed = pngle_feed(pngle, data, data_len);
    
    if (fed < 0) {
        ESP_LOGE(TAG, "pngle_feed failed: %d (%s)", fed, pngle_error(pngle));
        pngle_destroy(pngle);
        if (g_pngle_ctx.decoded_data) {
            free(g_pngle_ctx.decoded_data);
            g_pngle_ctx.decoded_data = NULL;
        }
        return;
    }
    
    ESP_LOGI(TAG, "PNG decoded successfully: %" PRIu32 "x%" PRIu32 ", consumed %d bytes", 
             (uint32_t)g_pngle_ctx.width, (uint32_t)g_pngle_ctx.height, fed);
    
    // Setup LVGL image descriptor with decoded RGB565 data
    g_thumbnail_dsc.header.always_zero = 0;
    g_thumbnail_dsc.header.w = g_pngle_ctx.width;
    g_thumbnail_dsc.header.h = g_pngle_ctx.height;
    g_thumbnail_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565
    g_thumbnail_dsc.data_size = g_pngle_ctx.width * g_pngle_ctx.height * sizeof(lv_color_t);
    g_thumbnail_dsc.data = g_pngle_ctx.decoded_data;
    
    // Update the background image
    if (lvgl_lock(100)) {
        lv_img_set_src(g_bg_img, &g_thumbnail_dsc);
        
        // Make image visible and configure appearance
        lv_obj_clear_flag(g_bg_img, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_zoom(g_bg_img, 256);  // 1x zoom initially
        lv_obj_set_style_img_opa(g_bg_img, LV_OPA_40, LV_PART_MAIN);
        
        lvgl_unlock();
        ESP_LOGI(TAG, "Thumbnail displayed on screen");
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock for thumbnail update");
    }
    
    // Clean up pngle decoder
    pngle_destroy(pngle);
    
    // Check heap after decode
    ESP_LOGI(TAG, "Free heap after decode: %" PRIu32 " bytes", (uint32_t)esp_get_free_heap_size());
}
