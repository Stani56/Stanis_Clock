/**
 * @file wordclock_error_log_mqtt.c
 * @brief MQTT interface for error log manager
 *
 * Provides MQTT command handlers for querying, retrieving, and clearing
 * persistent error logs stored in NVS.
 */

#include "error_log_manager.h"
#include "mqtt_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

static const char *TAG = "error_log_mqtt";

/**
 * @brief Handle error log query command
 *
 * Expected payload (JSON):
 * {
 *   "count": 10,        // Number of recent entries to retrieve (default: 10, max: 50)
 *   "offset": 0         // Optional offset (default: 0)
 * }
 *
 * Response published to home/Clock_Stani_1/error_log/response
 */
esp_err_t handle_error_log_query(const char *payload, int payload_len) {
    ESP_LOGI(TAG, "Processing error log query");

    // Parse JSON payload
    uint16_t count = 10;  // Default

    if (payload && payload_len > 0) {
        cJSON *root = cJSON_ParseWithLength(payload, payload_len);
        if (root) {
            cJSON *count_json = cJSON_GetObjectItem(root, "count");
            if (cJSON_IsNumber(count_json)) {
                count = (uint16_t)count_json->valueint;
                if (count > 50) count = 50;  // Limit to 50 entries
            }
            cJSON_Delete(root);
        }
    }

    // Read error log entries
    error_log_entry_t *entries = malloc(count * sizeof(error_log_entry_t));
    if (!entries) {
        ESP_LOGE(TAG, "Failed to allocate memory for error log entries");
        return ESP_ERR_NO_MEM;
    }

    uint16_t actual_count = 0;
    esp_err_t ret = error_log_read_recent(entries, count, &actual_count);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read error log entries: %s", esp_err_to_name(ret));
        free(entries);
        return ret;
    }

    // Build JSON response
    cJSON *response = cJSON_CreateObject();
    cJSON *entries_array = cJSON_CreateArray();

    for (uint16_t i = 0; i < actual_count; i++) {
        error_log_entry_t *entry = &entries[i];

        cJSON *entry_json = cJSON_CreateObject();

        // Format timestamp
        char timestamp_str[32];
        struct tm timeinfo;
        localtime_r(&entry->timestamp, &timeinfo);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

        cJSON_AddStringToObject(entry_json, "timestamp", timestamp_str);
        cJSON_AddNumberToObject(entry_json, "uptime_sec", entry->uptime_sec);
        cJSON_AddStringToObject(entry_json, "source", error_log_get_source_name(entry->error_source));
        cJSON_AddStringToObject(entry_json, "severity", error_log_get_severity_name(entry->error_severity));
        cJSON_AddNumberToObject(entry_json, "code", entry->error_code);
        cJSON_AddStringToObject(entry_json, "message", entry->message);

        // Add context if error source is LED validation
        if (entry->error_source == ERROR_SOURCE_LED_VALIDATION) {
            validation_error_context_t *ctx = (validation_error_context_t *)entry->context;
            cJSON *context_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(context_json, "mismatch_count", ctx->mismatch_count);
            cJSON_AddNumberToObject(context_json, "failure_type", ctx->failure_type);

            // Decode affected rows bitmask
            cJSON *rows_array = cJSON_CreateArray();
            for (uint8_t row = 0; row < 10; row++) {
                if (ctx->affected_rows[row / 8] & (1 << (row % 8))) {
                    cJSON_AddItemToArray(rows_array, cJSON_CreateNumber(row));
                }
            }
            cJSON_AddItemToObject(context_json, "affected_rows", rows_array);
            cJSON_AddItemToObject(entry_json, "context", context_json);
        }

        cJSON_AddItemToArray(entries_array, entry_json);
    }

    cJSON_AddItemToObject(response, "entries", entries_array);
    cJSON_AddNumberToObject(response, "count", actual_count);
    cJSON_AddNumberToObject(response, "requested", count);

    char *json_str = cJSON_PrintUnformatted(response);
    if (json_str) {
        mqtt_publish_error_log_response(json_str);
        cJSON_free(json_str);
    }

    cJSON_Delete(response);
    free(entries);

    ESP_LOGI(TAG, "✅ Published %d error log entries", actual_count);
    return ESP_OK;
}

/**
 * @brief Handle error log statistics request
 *
 * Response published to home/Clock_Stani_1/error_log/stats
 */
esp_err_t handle_error_log_stats_request(void) {
    ESP_LOGI(TAG, "Processing error log statistics request");

    error_log_stats_t stats;
    esp_err_t ret = error_log_get_stats(&stats);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get error log statistics: %s", esp_err_to_name(ret));
        return ret;
    }

    // Build JSON response
    cJSON *response = cJSON_CreateObject();

    // Buffer status
    cJSON_AddNumberToObject(response, "total_entries_written", stats.total_entries_written);
    cJSON_AddNumberToObject(response, "current_entry_count", stats.current_entry_count);
    cJSON_AddNumberToObject(response, "buffer_capacity", ERROR_LOG_MAX_ENTRIES);
    cJSON_AddNumberToObject(response, "head_index", stats.head_index);

    // Total error count
    cJSON_AddNumberToObject(response, "total_errors", stats.total_errors);

    // Errors by source
    cJSON *by_source = cJSON_CreateObject();
    cJSON_AddNumberToObject(by_source, "validation", stats.validation_errors);
    cJSON_AddNumberToObject(by_source, "i2c", stats.i2c_errors);
    cJSON_AddNumberToObject(by_source, "wifi", stats.wifi_errors);
    cJSON_AddNumberToObject(by_source, "mqtt", stats.mqtt_errors);
    cJSON_AddNumberToObject(by_source, "ntp", stats.ntp_errors);
    cJSON_AddNumberToObject(by_source, "system", stats.system_errors);
    cJSON_AddItemToObject(response, "errors_by_source", by_source);

    // Errors by severity
    cJSON *by_severity = cJSON_CreateObject();
    cJSON_AddNumberToObject(by_severity, "info", stats.info_count);
    cJSON_AddNumberToObject(by_severity, "warning", stats.warning_count);
    cJSON_AddNumberToObject(by_severity, "error", stats.error_count);
    cJSON_AddNumberToObject(by_severity, "critical", stats.critical_count);
    cJSON_AddItemToObject(response, "errors_by_severity", by_severity);

    // Last error timestamps
    cJSON *last_errors = cJSON_CreateObject();
    if (stats.last_validation_error > 0) {
        char timestamp_str[32];
        struct tm timeinfo;
        localtime_r(&stats.last_validation_error, &timeinfo);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        cJSON_AddStringToObject(last_errors, "validation", timestamp_str);
    }
    if (stats.last_i2c_error > 0) {
        char timestamp_str[32];
        struct tm timeinfo;
        localtime_r(&stats.last_i2c_error, &timeinfo);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        cJSON_AddStringToObject(last_errors, "i2c", timestamp_str);
    }
    if (stats.last_wifi_error > 0) {
        char timestamp_str[32];
        struct tm timeinfo;
        localtime_r(&stats.last_wifi_error, &timeinfo);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        cJSON_AddStringToObject(last_errors, "wifi", timestamp_str);
    }
    if (stats.last_mqtt_error > 0) {
        char timestamp_str[32];
        struct tm timeinfo;
        localtime_r(&stats.last_mqtt_error, &timeinfo);
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        cJSON_AddStringToObject(last_errors, "mqtt", timestamp_str);
    }
    cJSON_AddItemToObject(response, "last_errors", last_errors);

    char *json_str = cJSON_PrintUnformatted(response);
    if (json_str) {
        mqtt_publish_error_log_stats(json_str);
        cJSON_free(json_str);
    }

    cJSON_Delete(response);
    ESP_LOGI(TAG, "✅ Published error log statistics");
    return ESP_OK;
}

/**
 * @brief Handle error log clear command
 *
 * Expected payload: "clear" or "reset_stats"
 */
esp_err_t handle_error_log_clear(const char *payload, int payload_len) {
    ESP_LOGI(TAG, "Processing error log clear command");

    if (!payload || payload_len == 0) {
        ESP_LOGW(TAG, "Empty payload for error log clear");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    if (strncmp(payload, "clear", 5) == 0) {
        ret = error_log_clear();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Error log cleared successfully");
        } else {
            ESP_LOGW(TAG, "Failed to clear error log: %s", esp_err_to_name(ret));
        }
    } else if (strncmp(payload, "reset_stats", 11) == 0) {
        ret = error_log_reset_stats();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Error log statistics reset successfully");
        } else {
            ESP_LOGW(TAG, "Failed to reset error log statistics: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "Unknown error log clear command: %.*s", payload_len, payload);
        return ESP_ERR_INVALID_ARG;
    }

    // Publish updated statistics after clear/reset
    if (ret == ESP_OK) {
        handle_error_log_stats_request();
    }

    return ret;
}
