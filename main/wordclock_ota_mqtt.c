/**
 * @file wordclock_ota_mqtt.c
 * @brief MQTT interface implementation for OTA source management
 */

#include "wordclock_ota_mqtt.h"
#include "ota_manager.h"
#include "mqtt_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "ota_mqtt";

/**
 * @brief Handle OTA source set command
 *
 * Payload format: {"source": "github"} or {"source": "cloudflare"}
 */
esp_err_t handle_ota_source_set(const char *payload, int payload_len)
{
    if (!payload || payload_len == 0) {
        ESP_LOGE(TAG, "Invalid payload");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Processing OTA source set command");
    ESP_LOGD(TAG, "Payload: %.*s", payload_len, payload);

    // Parse JSON payload
    cJSON *json = cJSON_ParseWithLength(payload, payload_len);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON payload");
        return ESP_FAIL;
    }

    // Extract source field
    cJSON *source_obj = cJSON_GetObjectItem(json, "source");
    if (!source_obj || !cJSON_IsString(source_obj)) {
        ESP_LOGE(TAG, "Missing or invalid 'source' field");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    const char *source_str = source_obj->valuestring;
    ESP_LOGI(TAG, "Requested OTA source: %s", source_str);

    // Parse source string to enum
    ota_source_t source;
    esp_err_t ret = ota_source_from_string(source_str, &source);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid OTA source: %s (valid: github, cloudflare)", source_str);
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Set new OTA source
    ret = ota_set_source(source);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ OTA source changed to: %s", ota_source_to_string(source));

        // Publish updated status
        publish_ota_source_status();
    } else {
        ESP_LOGE(TAG, "❌ Failed to set OTA source: %s", esp_err_to_name(ret));
    }

    cJSON_Delete(json);
    return ret;
}

/**
 * @brief Publish current OTA source status
 *
 * Published to home/Clock_Stani_1/ota/source/status
 */
esp_err_t publish_ota_source_status(void)
{
    ota_source_t current_source = ota_get_source();
    const char *source_str = ota_source_to_string(current_source);

    // Create JSON response
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(json, "source", source_str);
    cJSON_AddStringToObject(json, "github_url", OTA_GITHUB_FIRMWARE_URL);
    cJSON_AddStringToObject(json, "cloudflare_url", OTA_CLOUDFLARE_FIRMWARE_URL);

    char *json_str = cJSON_PrintUnformatted(json);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize JSON");
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    // Publish via MQTT
    extern esp_err_t mqtt_publish_ota_source_status(const char *json_payload);
    esp_err_t ret = mqtt_publish_ota_source_status(json_str);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Published OTA source status: %s", source_str);
    } else {
        ESP_LOGE(TAG, "❌ Failed to publish OTA source status");
    }

    cJSON_free(json_str);
    cJSON_Delete(json);

    return ret;
}
