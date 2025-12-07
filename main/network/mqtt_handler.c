#include "mqtt_handler.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"  // ESP-IDF MQTT client header
#include "cJSON.h"
#include "ui/ui_media.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "mqtt_handler";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_connected = false;

// Buffer for assembling fragmented thumbnail messages
// Note: This buffer is provided by ui_media to avoid duplicate allocation
static uint8_t *s_thumb_buffer = NULL;
static size_t s_thumb_buffer_size = 0;
static int s_thumb_offset = 0;
static int s_thumb_total_len = 0;
static bool s_receiving_thumb = false;

// Track current topic for fragmented messages
typedef enum {
    CURRENT_TOPIC_NONE = 0,
    CURRENT_TOPIC_STATE,
    CURRENT_TOPIC_THUMB
} current_topic_t;

static current_topic_t s_current_topic = CURRENT_TOPIC_NONE;

static void parse_media_state(const char *data, int data_len)
{
    // Parse JSON
    cJSON *json = cJSON_ParseWithLength(data, data_len);
    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse JSON (%d bytes)", data_len);
        return;
    }
    
    // Extract fields
    media_state_t state = {0};
    
    cJSON *title = cJSON_GetObjectItem(json, "title");
    if (title && cJSON_IsString(title)) {
        strncpy(state.title, title->valuestring, sizeof(state.title) - 1);
    }
    
    cJSON *artist = cJSON_GetObjectItem(json, "artist");
    if (artist && cJSON_IsString(artist)) {
        strncpy(state.artist, artist->valuestring, sizeof(state.artist) - 1);
    }
    
    cJSON *duration = cJSON_GetObjectItem(json, "duration");
    if (duration && cJSON_IsNumber(duration)) {
        state.duration_sec = (uint32_t)duration->valuedouble;
    }
    
    cJSON *position = cJSON_GetObjectItem(json, "currentposition");
    if (position && cJSON_IsNumber(position)) {
        state.position_sec = (uint32_t)position->valuedouble;
    }
    
    cJSON *play_state = cJSON_GetObjectItem(json, "state");
    if (play_state && cJSON_IsString(play_state)) {
        state.is_playing = (strcmp(play_state->valuestring, "playing") == 0);
    }
    
    // Only update if we have a title (valid data)
    if (strlen(state.title) > 0) {
        ESP_LOGI(TAG, "Media: '%s' by '%s' [%s] (%" PRIu32 "/%" PRIu32 "s)", 
                 state.title, state.artist, 
                 state.is_playing ? "playing" : "paused",
                 state.position_sec, state.duration_sec);
        
        // Update UI
        ui_media_update_state(&state);
    }
    
    cJSON_Delete(json);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            s_is_connected = true;
            
            // Check if thumbnail buffer was set by UI
            if (s_thumb_buffer == NULL) {
                ESP_LOGW(TAG, "Thumbnail buffer not set - thumbnails will be ignored");
            }
            
            // Subscribe to media state topic
            int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_STATE, 0);
            ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", MQTT_TOPIC_STATE, msg_id);
            
            // Subscribe to thumbnail topic (only if buffer is available)
            // NOTE: Thumbnail display disabled due to insufficient RAM for PNG decoding
            // ESP32-C6 has ~33KB free heap, but PNG decode needs 300-400KB
            // To enable: Use JPEG thumbnails OR reduce thumbnail size to <100x100
            #if 1  // Disabled thumbnail subscription
            if (s_thumb_buffer != NULL) {
                msg_id = esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_THUMB, 0);
                ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", MQTT_TOPIC_THUMB, msg_id);
            }
            #else
            ESP_LOGW(TAG, "Thumbnail display disabled - insufficient RAM for PNG decode");
            ESP_LOGI(TAG, "To enable: Switch to JPEG or reduce thumbnail size to <100x100 pixels");
            #endif
            
            // Request initial state by publishing to a status request topic (if your broker supports it)
            // Or just wait for the next state update
            ESP_LOGI(TAG, "Waiting for media state updates...");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            s_is_connected = false;
            s_current_topic = CURRENT_TOPIC_NONE;
            s_receiving_thumb = false;
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            // Debug: Log all received data events
            ESP_LOGD(TAG, "MQTT_EVENT_DATA: topic_len=%d, data_len=%d, total_len=%d, offset=%d",
                     event->topic_len, event->data_len, event->total_data_len, event->current_data_offset);
            
            // Check if this is a new message or continuation of fragmented message
            if (event->topic_len > 0) {
                // New message with topic - log the topic
                char topic_str[128] = {0};
                int copy_len = (event->topic_len < 127) ? event->topic_len : 127;
                memcpy(topic_str, event->topic, copy_len);
                ESP_LOGI(TAG, "Received message on topic: '%s' (%d bytes)", topic_str, event->data_len);
                
                // New message with topic
                if (strncmp(event->topic, MQTT_TOPIC_THUMB, event->topic_len) == 0) {
                    // New thumbnail - always accept and reset
                    s_current_topic = CURRENT_TOPIC_THUMB;
                    s_thumb_offset = 0;
                    s_thumb_total_len = event->total_data_len;
                    s_receiving_thumb = true;
                    ESP_LOGI(TAG, "Starting thumbnail reception: %d bytes total", s_thumb_total_len);
                } else if (strncmp(event->topic, MQTT_TOPIC_STATE, event->topic_len) == 0) {
                    // State message - always process (state messages are small and complete)
                    s_current_topic = CURRENT_TOPIC_STATE;
                    ESP_LOGI(TAG, "Received state message: %d bytes (total: %d, offset: %d)", 
                             event->data_len, event->total_data_len, event->current_data_offset);
                    
                    // State messages should be complete in one event
                    if (event->data_len > 0 && event->current_data_offset == 0) {
                        // Log first 100 chars of data for debugging
                        char preview[101] = {0};
                        int preview_len = (event->data_len < 100) ? event->data_len : 100;
                        memcpy(preview, event->data, preview_len);
                        ESP_LOGI(TAG, "State data: %s", preview);
                        
                        parse_media_state(event->data, event->data_len);
                    }
                    break;  // State handled, don't process further
                } else {
                    s_current_topic = CURRENT_TOPIC_NONE;
                }
            }
            
            // Handle data based on tracked topic (for fragmented messages)
            if (s_current_topic == CURRENT_TOPIC_THUMB && s_thumb_buffer != NULL && s_receiving_thumb) {
                // Accumulate thumbnail data
                int copy_len = event->data_len;
                if (s_thumb_offset + copy_len > (int)s_thumb_buffer_size) {
                    copy_len = s_thumb_buffer_size - s_thumb_offset;
                    ESP_LOGW(TAG, "Thumbnail buffer overflow, truncating");
                }
                
                if (copy_len > 0) {
                    memcpy(s_thumb_buffer + s_thumb_offset, event->data, copy_len);
                    s_thumb_offset += copy_len;
                }
                
                // Check if complete
                if (s_thumb_offset >= s_thumb_total_len) {
                    ESP_LOGI(TAG, "Thumbnail complete: %d bytes received", s_thumb_offset);
                    ui_media_update_thumbnail(s_thumb_buffer, s_thumb_offset);
                    s_receiving_thumb = false;
                    s_current_topic = CURRENT_TOPIC_NONE;
                }
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP transport error");
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused");
            }
            break;
            
        default:
            break;
    }
}

esp_err_t mqtt_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .buffer.size = 4096,  // Reduced from 8192 to save ~4KB RAM (64x64 PNG fits easily)
        .buffer.out_size = 512,  // Reduced from 1024
    };
    
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    ESP_LOGI(TAG, "MQTT client initialized");
    return ESP_OK;
}

esp_err_t mqtt_handler_start(void)
{
    ESP_LOGI(TAG, "Starting MQTT client...");
    
    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client");
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

bool mqtt_handler_is_connected(void)
{
    return s_is_connected;
}

void mqtt_handler_set_thumbnail_buffer(uint8_t *buffer, size_t size)
{
    s_thumb_buffer = buffer;
    s_thumb_buffer_size = size;
    ESP_LOGI(TAG, "Thumbnail buffer set: %p, size: %zu bytes", buffer, size);
}

esp_err_t mqtt_handler_publish(const char *topic, const char *data, int len, int qos, int retain)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }

    if (!s_is_connected) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, data, len, qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published to %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}
