#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize WiFi in station mode
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_init(void);

/**
 * @brief Connect to WiFi access point
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_connect(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_is_connected(void);

#endif // WIFI_MANAGER_H
