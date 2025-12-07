#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize MQTT client
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_handler_init(void);

/**
 * @brief Start MQTT client and connect to broker
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_handler_start(void);

/**
 * @brief Check if MQTT is connected
 * 
 * @return true if connected, false otherwise
 */
bool mqtt_handler_is_connected(void);

/**
 * @brief Set the thumbnail buffer for MQTT to use
 * This avoids allocating a separate buffer in MQTT handler
 *
 * @param buffer Pointer to thumbnail buffer
 * @param size Size of the buffer in bytes
 */
void mqtt_handler_set_thumbnail_buffer(uint8_t *buffer, size_t size);

/**
 * @brief Publish a message to an MQTT topic
 *
 * @param topic Topic to publish to
 * @param data Data to publish
 * @param len Length of data
 * @param qos Quality of Service (0, 1, or 2)
 * @param retain Retain flag
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_handler_publish(const char *topic, const char *data, int len, int qos, int retain);

#endif // MQTT_HANDLER_H
