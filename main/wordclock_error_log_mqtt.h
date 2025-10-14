/**
 * @file wordclock_error_log_mqtt.h
 * @brief MQTT interface for error log manager
 */

#ifndef WORDCLOCK_ERROR_LOG_MQTT_H
#define WORDCLOCK_ERROR_LOG_MQTT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle error log query command
 * @param payload JSON payload with query parameters
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t handle_error_log_query(const char *payload, int payload_len);

/**
 * @brief Handle error log statistics request
 * @return ESP_OK on success
 */
esp_err_t handle_error_log_stats_request(void);

/**
 * @brief Handle error log clear command
 * @param payload Command ("clear" or "reset_stats")
 * @param payload_len Length of payload
 * @return ESP_OK on success
 */
esp_err_t handle_error_log_clear(const char *payload, int payload_len);

#ifdef __cplusplus
}
#endif

#endif  // WORDCLOCK_ERROR_LOG_MQTT_H
