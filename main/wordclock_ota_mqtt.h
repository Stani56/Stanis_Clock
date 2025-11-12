/**
 * @file wordclock_ota_mqtt.h
 * @brief MQTT interface for OTA source management
 */

#ifndef WORDCLOCK_OTA_MQTT_H
#define WORDCLOCK_OTA_MQTT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle OTA source set command
 * @param payload JSON payload with source selection
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t handle_ota_source_set(const char *payload, int payload_len);

/**
 * @brief Publish current OTA source status
 * @return ESP_OK on success
 */
esp_err_t publish_ota_source_status(void);

#ifdef __cplusplus
}
#endif

#endif  // WORDCLOCK_OTA_MQTT_H
