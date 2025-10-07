/**
 * @file wordclock_mqtt_handlers.h
 * @brief MQTT command handling for wordclock
 * 
 * This module provides centralized MQTT command processing,
 * separating the command logic from the MQTT transport layer.
 */

#ifndef WORDCLOCK_MQTT_HANDLERS_H
#define WORDCLOCK_MQTT_HANDLERS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MQTT handlers
 * @return ESP_OK on success
 */
esp_err_t mqtt_handlers_init(void);

/**
 * @brief Handle incoming MQTT command
 * @param payload Command payload
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t mqtt_handle_command(const char* payload, int payload_len);

/**
 * @brief Handle transition control commands
 * @param payload JSON payload with transition settings
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t mqtt_handle_transition_control(const char* payload, int payload_len);

/**
 * @brief Handle brightness control commands
 * @param payload JSON payload with brightness settings
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t mqtt_handle_brightness_control(const char* payload, int payload_len);

/**
 * @brief Handle brightness configuration set commands
 * @param payload JSON payload with brightness configuration
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t mqtt_handle_brightness_config_set(const char* payload, int payload_len);

/**
 * @brief Handle brightness configuration get commands
 * @param payload Command payload (usually empty)
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t mqtt_handle_brightness_config_get(const char* payload, int payload_len);

/**
 * @brief Handle brightness configuration reset
 * @return ESP_OK on success
 */
esp_err_t mqtt_handle_brightness_config_reset(void);

// Status publishing functions (Phase 2.2 interface - prefixed to avoid conflicts)
esp_err_t wordclock_mqtt_publish_system_status(void);
esp_err_t wordclock_mqtt_publish_sensor_status(void);
esp_err_t wordclock_mqtt_publish_heartbeat_with_ntp(void);
esp_err_t wordclock_mqtt_publish_brightness_status(uint8_t individual, uint8_t global);
esp_err_t wordclock_mqtt_publish_brightness_config_status(void);

#ifdef __cplusplus
}
#endif

#endif // WORDCLOCK_MQTT_HANDLERS_H