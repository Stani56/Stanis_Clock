/**
 * @file ota_manager.c
 * @brief OTA (Over-The-Air) Update Manager Implementation
 *
 * Provides secure firmware updates via HTTPS from GitHub Releases.
 * Features automatic rollback on boot failures and health check validation.
 *
 * @author Claude Code
 * @date 2025-11-07
 */

#include "ota_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <sys/time.h>

// External dependencies
extern bool thread_safe_get_wifi_connected(void);
extern bool thread_safe_get_mqtt_connected(void);
extern esp_err_t i2c_devices_check_health(void);

static const char *TAG = "ota_manager";

// GitHub root CA certificate (embedded via CMakeLists.txt)
extern const uint8_t github_root_ca_pem_start[] asm("_binary_github_root_ca_pem_start");
extern const uint8_t github_root_ca_pem_end[]   asm("_binary_github_root_ca_pem_end");

// OTA state tracking
static struct {
    ota_state_t state;
    ota_error_t error;
    uint8_t progress_percent;
    uint32_t bytes_downloaded;
    uint32_t total_bytes;
    uint32_t start_time_ms;
    SemaphoreHandle_t mutex;
    bool initialized;
} ota_context = {
    .state = OTA_STATE_IDLE,
    .error = OTA_ERROR_NONE,
    .progress_percent = 0,
    .bytes_downloaded = 0,
    .total_bytes = 0,
    .start_time_ms = 0,
    .mutex = NULL,
    .initialized = false
};

// NVS keys for OTA state
#define NVS_NAMESPACE       "ota_manager"
#define NVS_KEY_BOOT_COUNT  "boot_count"
#define NVS_KEY_FIRST_BOOT  "first_boot"

// Forward declarations
static void ota_task(void *pvParameters);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint32_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

/**
 * @brief Thread-safe state update
 */
static void update_state(ota_state_t new_state, ota_error_t error)
{
    if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ota_context.state = new_state;
        ota_context.error = error;
        xSemaphoreGive(ota_context.mutex);
    }
}

/**
 * @brief Thread-safe progress update
 */
static void update_progress(uint32_t bytes_downloaded, uint32_t total_bytes)
{
    if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ota_context.bytes_downloaded = bytes_downloaded;
        ota_context.total_bytes = total_bytes;

        if (total_bytes > 0) {
            ota_context.progress_percent = (bytes_downloaded * 100) / total_bytes;
        } else {
            ota_context.progress_percent = 0;
        }

        xSemaphoreGive(ota_context.mutex);
    }
}

/**
 * @brief HTTP event handler for OTA download
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP error during OTA download");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "Connected to OTA server");
        break;

    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP headers sent");
        break;

    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "Header: %s: %s", evt->header_key, evt->header_value);

        // Capture Content-Length for progress tracking
        if (strcasecmp(evt->header_key, "Content-Length") == 0) {
            uint32_t total = atoi(evt->header_value);
            update_progress(0, total);
            ESP_LOGI(TAG, "Firmware size: %lu bytes (%.2f MB)", total, total / 1024.0 / 1024.0);
        }
        break;

    case HTTP_EVENT_ON_DATA:
        // Progress update handled by esp_https_ota
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP download finished");
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "Disconnected from server");
        break;

    default:
        break;
    }
    return ESP_OK;
}

//=============================================================================
// Version Management
//=============================================================================

esp_err_t ota_get_current_version(firmware_version_t *version)
{
    if (version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get app description from current partition
    const esp_app_desc_t *app_desc = esp_app_get_description();

    strncpy(version->version, app_desc->version, sizeof(version->version) - 1);
    version->version[sizeof(version->version) - 1] = '\0';

    strncpy(version->build_date, app_desc->date, sizeof(version->build_date) - 1);
    version->build_date[sizeof(version->build_date) - 1] = '\0';

    strncpy(version->idf_version, app_desc->idf_ver, sizeof(version->idf_version) - 1);
    version->idf_version[sizeof(version->idf_version) - 1] = '\0';

    // Get partition size
    const esp_partition_t *running = esp_ota_get_running_partition();
    version->size_bytes = running ? running->size : 0;

    ESP_LOGI(TAG, "Current firmware: %s (built %s, IDF %s)",
             version->version, version->build_date, version->idf_version);

    return ESP_OK;
}

/**
 * @brief Buffer for version check response
 */
typedef struct {
    char *buffer;
    int buffer_len;
    int data_len;
} version_check_buffer_t;

/**
 * @brief Event handler for version check HTTP client
 */
static esp_err_t version_check_event_handler(esp_http_client_event_t *evt)
{
    version_check_buffer_t *output_buffer = (version_check_buffer_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        // Copy response data to our buffer
        if (output_buffer->buffer && evt->data_len > 0) {
            int copy_len = evt->data_len;
            if (output_buffer->data_len + copy_len > output_buffer->buffer_len - 1) {
                copy_len = output_buffer->buffer_len - output_buffer->data_len - 1;
            }
            if (copy_len > 0) {
                memcpy(output_buffer->buffer + output_buffer->data_len, evt->data, copy_len);
                output_buffer->data_len += copy_len;
                output_buffer->buffer[output_buffer->data_len] = '\0';
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t ota_check_for_updates(firmware_version_t *available_version)
{
    if (available_version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check network connectivity
    if (!thread_safe_get_wifi_connected()) {
        ESP_LOGW(TAG, "Cannot check for updates: WiFi not connected");
        return ESP_FAIL;
    }

    update_state(OTA_STATE_CHECKING, OTA_ERROR_NONE);

    esp_err_t ret = ESP_FAIL;

    // Allocate buffer for response (max 2KB for version.json)
    char *buffer = malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for response");
        update_state(OTA_STATE_IDLE, OTA_ERROR_NO_INTERNET);
        return ESP_FAIL;
    }

    // Setup buffer structure for event handler
    version_check_buffer_t output_buffer = {
        .buffer = buffer,
        .buffer_len = 2048,
        .data_len = 0
    };

    // Configure HTTP client for version check with custom event handler
    // Use ESP-IDF's built-in certificate bundle (includes GitHub's root CA)
    esp_http_client_config_t config = {
        .url = OTA_DEFAULT_VERSION_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Use ESP-IDF cert bundle
        .event_handler = version_check_event_handler, // Custom handler captures data
        .user_data = &output_buffer,                   // Pass buffer to handler
        .timeout_ms = 10000,
        .is_async = false,
        .keep_alive_enable = true,
        .max_redirection_count = 5,      // Follow GitHub redirects
        .buffer_size = 2048,              // HTTP response buffer
        .buffer_size_tx = 1024,           // HTTP request buffer
        .disable_auto_redirect = false,   // Enable auto redirect
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(buffer);
        update_state(OTA_STATE_IDLE, OTA_ERROR_NO_INTERNET);
        return ESP_FAIL;
    }

    // Perform HTTP GET with redirect support
    // Event handler will capture the data during HTTP_EVENT_ON_DATA
    esp_err_t err = esp_http_client_perform(client);

    int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);

    ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d, captured = %d bytes, err = %s",
             status, content_length, output_buffer.data_len, esp_err_to_name(err));

    if (err == ESP_OK && status == 200 && output_buffer.data_len > 0) {
        // Data was captured by event handler
        ESP_LOGI(TAG, "Successfully received version data");

        // Parse JSON
        ESP_LOGI(TAG, "Parsing JSON response (%d bytes)", output_buffer.data_len);
        ESP_LOGD(TAG, "JSON content: %s", buffer);

        cJSON *json = cJSON_Parse(buffer);
        if (json) {
            ESP_LOGI(TAG, "JSON parsed successfully");
            cJSON *version_obj = cJSON_GetObjectItem(json, "version");
            cJSON *date_obj = cJSON_GetObjectItem(json, "build_date");
            cJSON *size_obj = cJSON_GetObjectItem(json, "size_bytes");

            ESP_LOGI(TAG, "JSON fields: version=%p, date=%p, size=%p",
                    version_obj, date_obj, size_obj);

            if (version_obj && date_obj) {
                strncpy(available_version->version, version_obj->valuestring,
                        sizeof(available_version->version) - 1);
                available_version->version[sizeof(available_version->version) - 1] = '\0';

                strncpy(available_version->build_date, date_obj->valuestring,
                        sizeof(available_version->build_date) - 1);
                available_version->build_date[sizeof(available_version->build_date) - 1] = '\0';

                available_version->size_bytes = size_obj ? size_obj->valueint : 0;

                // Compare versions
                firmware_version_t current;
                ota_get_current_version(&current);

                if (strcmp(available_version->version, current.version) != 0) {
                    ESP_LOGI(TAG, "Update available: %s → %s",
                            current.version, available_version->version);
                    ret = ESP_OK;
                } else {
                    ESP_LOGI(TAG, "Already running latest version: %s", current.version);
                    ret = ESP_ERR_NOT_FOUND;
                }
            } else {
                ESP_LOGE(TAG, "Missing required JSON fields (version or build_date)");
            }

            cJSON_Delete(json);
        } else {
            ESP_LOGE(TAG, "JSON parsing failed");
        }
    } else {
        ESP_LOGE(TAG, "Invalid HTTP response: err=%s, status=%d, length=%d",
                 esp_err_to_name(err), status, content_length);
    }

    free(buffer);
    esp_http_client_cleanup(client);
    update_state(OTA_STATE_IDLE, ret == ESP_OK ? OTA_ERROR_NONE : OTA_ERROR_NO_INTERNET);

    return ret;
}

//=============================================================================
// OTA Update Process
//=============================================================================

/**
 * @brief OTA task - performs actual download and flash
 */
static void ota_task(void *pvParameters)
{
    const ota_config_t *config = (const ota_config_t *)pvParameters;
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "🔄 Starting OTA update from: %s", config->firmware_url);

    // Reset progress
    ota_context.start_time_ms = get_time_ms();
    update_progress(0, 0);
    update_state(OTA_STATE_DOWNLOADING, OTA_ERROR_NONE);

    // Configure HTTPS OTA
    // Use ESP-IDF's built-in certificate bundle (includes GitHub's root CA)
    esp_http_client_config_t http_config = {
        .url = config->firmware_url,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Use ESP-IDF cert bundle
        .event_handler = http_event_handler,
        .timeout_ms = config->timeout_ms,
        .is_async = false,
        .buffer_size = 4096,        // Larger buffer for firmware download
        .buffer_size_tx = 1024,     // TX buffer size
        .keep_alive_enable = true,
        .max_redirection_count = 5, // Follow GitHub redirects
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    ret = esp_https_ota_begin(&ota_config, &https_ota_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA begin failed: %s", esp_err_to_name(ret));
        update_state(OTA_STATE_FAILED, OTA_ERROR_DOWNLOAD_FAILED);
        vTaskDelete(NULL);
        return;
    }

    // Get and verify app descriptor BEFORE downloading
    // This must be done after begin() but before perform() loop
    esp_app_desc_t new_app_info;
    ret = esp_https_ota_get_img_desc(https_ota_handle, &new_app_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Could not get app description: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "   This usually means incomplete headers or redirect issues");
        esp_https_ota_abort(https_ota_handle);
        update_state(OTA_STATE_FAILED, OTA_ERROR_DOWNLOAD_FAILED);
        vTaskDelete(NULL);
        return;
    }

    // Verify magic word
    if (new_app_info.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "❌ Invalid magic word in new firmware: 0x%08lx", new_app_info.magic_word);
        esp_https_ota_abort(https_ota_handle);
        update_state(OTA_STATE_FAILED, OTA_ERROR_CHECKSUM_MISMATCH);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "✅ App descriptor validated");
    ESP_LOGI(TAG, "   New firmware version: %s", new_app_info.version);
    ESP_LOGI(TAG, "   Build date: %s %s", new_app_info.date, new_app_info.time);

    ESP_LOGI(TAG, "📥 Downloading firmware...");

    // Download and flash in chunks
    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);

        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // Update progress
        int bytes_downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
        update_progress(bytes_downloaded, ota_context.total_bytes);

        // Check if cancelled
        if (ota_context.state == OTA_STATE_IDLE) {
            ESP_LOGW(TAG, "⚠️ OTA cancelled by user");
            esp_https_ota_abort(https_ota_handle);
            vTaskDelete(NULL);
            return;
        }

        // Log progress every 10%
        static uint8_t last_progress = 0;
        if (ota_context.progress_percent >= last_progress + 10) {
            last_progress = ota_context.progress_percent;
            ESP_LOGI(TAG, "📊 Progress: %d%% (%lu / %lu bytes)",
                    ota_context.progress_percent,
                    ota_context.bytes_downloaded,
                    ota_context.total_bytes);
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA download failed: %s", esp_err_to_name(ret));
        esp_https_ota_abort(https_ota_handle);
        update_state(OTA_STATE_FAILED, OTA_ERROR_DOWNLOAD_FAILED);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "✅ Download complete, verifying image...");
    update_state(OTA_STATE_VERIFYING, OTA_ERROR_NONE);

    // Verify complete data received
    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        ESP_LOGE(TAG, "❌ Complete data not received");
        esp_https_ota_abort(https_ota_handle);
        update_state(OTA_STATE_FAILED, OTA_ERROR_DOWNLOAD_FAILED);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "✅ Image validation passed (already checked app descriptor)");
    update_state(OTA_STATE_FLASHING, OTA_ERROR_NONE);

    // Finish OTA (this sets boot partition)
    ret = esp_https_ota_finish(https_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA finish failed: %s", esp_err_to_name(ret));
        update_state(OTA_STATE_FAILED, OTA_ERROR_FLASH_FAILED);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "✅ OTA update successful!");
    update_state(OTA_STATE_COMPLETE, OTA_ERROR_NONE);

    uint32_t elapsed = get_time_ms() - ota_context.start_time_ms;
    ESP_LOGI(TAG, "⏱️ Update completed in %.1f seconds", elapsed / 1000.0);

    // Mark as first boot for health checks
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_KEY_FIRST_BOOT, 1);
        nvs_set_u32(nvs_handle, NVS_KEY_BOOT_COUNT, 0);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    // Auto-reboot if configured
    if (config->auto_reboot) {
        ESP_LOGI(TAG, "🔄 Rebooting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        ESP_LOGI(TAG, "ℹ️ Reboot required to apply update");
    }

    vTaskDelete(NULL);
}


esp_err_t ota_start_update(const ota_config_t *config)
{
    // Check if already running
    if (ota_context.state != OTA_STATE_IDLE && ota_context.state != OTA_STATE_FAILED) {
        ESP_LOGW(TAG, "OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    // Check WiFi
    if (!thread_safe_get_wifi_connected()) {
        ESP_LOGE(TAG, "Cannot start OTA: WiFi not connected");
        return ESP_FAIL;
    }

    // Use default config if not provided
    static ota_config_t default_config = {
        .firmware_url = OTA_DEFAULT_FIRMWARE_URL,
        .version_url = OTA_DEFAULT_VERSION_URL,
        .auto_reboot = true,
        .timeout_ms = OTA_DEFAULT_TIMEOUT_MS,
        .skip_version_check = false
    };

    if (config == NULL) {
        config = &default_config;
    }

    // Version check (unless skipped)
    if (!config->skip_version_check) {
        firmware_version_t available;
        esp_err_t ret = ota_check_for_updates(&available);

        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "Already running latest version");
            return ESP_ERR_NOT_FOUND;
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to check for updates");
            return ESP_FAIL;
        }
    }

    // Create OTA task
    BaseType_t result = xTaskCreate(
        ota_task,
        "ota_task",
        8192,  // 8KB stack
        (void *)config,
        5,  // Priority
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t ota_start_update_default(void)
{
    return ota_start_update(NULL);
}

//=============================================================================
// Progress and State
//=============================================================================

esp_err_t ota_get_progress(ota_progress_t *progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        progress->state = ota_context.state;
        progress->error = ota_context.error;
        progress->progress_percent = ota_context.progress_percent;
        progress->bytes_downloaded = ota_context.bytes_downloaded;
        progress->total_bytes = ota_context.total_bytes;
        progress->time_elapsed_ms = get_time_ms() - ota_context.start_time_ms;

        // Estimate time remaining
        if (progress->bytes_downloaded > 0 && progress->total_bytes > 0) {
            uint32_t bytes_remaining = progress->total_bytes - progress->bytes_downloaded;
            uint32_t bytes_per_ms = progress->bytes_downloaded / progress->time_elapsed_ms;
            progress->time_remaining_ms = bytes_remaining / (bytes_per_ms + 1);
        } else {
            progress->time_remaining_ms = 0;
        }

        xSemaphoreGive(ota_context.mutex);
        return ESP_OK;
    }

    return ESP_FAIL;
}

ota_state_t ota_get_state(void)
{
    ota_state_t state = OTA_STATE_IDLE;

    if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = ota_context.state;
        xSemaphoreGive(ota_context.mutex);
    }

    return state;
}

esp_err_t ota_cancel_update(void)
{
    if (ota_context.state == OTA_STATE_DOWNLOADING ||
        ota_context.state == OTA_STATE_VERIFYING) {

        ESP_LOGW(TAG, "Cancelling OTA update...");
        update_state(OTA_STATE_IDLE, OTA_ERROR_NONE);

        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

//=============================================================================
// Boot Management
//=============================================================================

bool ota_is_first_boot_after_update(void)
{
    nvs_handle_t nvs_handle;
    uint8_t first_boot = 0;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        nvs_get_u8(nvs_handle, NVS_KEY_FIRST_BOOT, &first_boot);
        nvs_close(nvs_handle);
    }

    return (first_boot == 1);
}

esp_err_t ota_mark_app_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ App marked as valid - rollback cancelled");

                // Clear first boot flag
                nvs_handle_t nvs_handle;
                if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                    nvs_set_u8(nvs_handle, NVS_KEY_FIRST_BOOT, 0);
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                }

                return ESP_OK;
            }
        }
    }

    return ESP_FAIL;
}

esp_err_t ota_trigger_rollback(void)
{
    ESP_LOGW(TAG, "⚠️ Triggering rollback to previous firmware");

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // Mark as invalid and reboot
            esp_ota_mark_app_invalid_rollback_and_reboot();
            // Never returns
        }
    }

    return ESP_FAIL;
}

const char* ota_get_running_partition_name(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running ? running->label : "unknown";
}

uint32_t ota_get_boot_count(void)
{
    nvs_handle_t nvs_handle;
    uint32_t boot_count = 0;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_get_u32(nvs_handle, NVS_KEY_BOOT_COUNT, &boot_count);
        boot_count++;
        nvs_set_u32(nvs_handle, NVS_KEY_BOOT_COUNT, boot_count);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    return boot_count;
}

//=============================================================================
// Health Checks
//=============================================================================

esp_err_t ota_perform_health_checks(void)
{
    ESP_LOGI(TAG, "🔍 Performing system health checks...");

    uint8_t passed = 0;
    uint8_t total = 5;

    // 1. WiFi connectivity
    if (thread_safe_get_wifi_connected()) {
        ESP_LOGI(TAG, "  ✅ WiFi: Connected");
        passed++;
    } else {
        ESP_LOGE(TAG, "  ❌ WiFi: Not connected");
    }

    // 2. MQTT connectivity
    if (thread_safe_get_mqtt_connected()) {
        ESP_LOGI(TAG, "  ✅ MQTT: Connected");
        passed++;
    } else {
        ESP_LOGE(TAG, "  ❌ MQTT: Not connected");
    }

    // 3. I2C devices (LED controllers + RTC)
    if (i2c_devices_check_health() == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ I2C: All devices responding");
        passed++;
    } else {
        ESP_LOGE(TAG, "  ❌ I2C: Device communication failed");
    }

    // 4. Memory check
    uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap > 50000) {  // At least 50KB free
        ESP_LOGI(TAG, "  ✅ Memory: %lu bytes free", free_heap);
        passed++;
    } else {
        ESP_LOGE(TAG, "  ❌ Memory: Only %lu bytes free (low)", free_heap);
    }

    // 5. Partition check
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL) {
        ESP_LOGI(TAG, "  ✅ Partition: Running from %s", running->label);
        passed++;
    } else {
        ESP_LOGE(TAG, "  ❌ Partition: Could not get running partition");
    }

    ESP_LOGI(TAG, "📊 Health check result: %d/%d passed", passed, total);

    return (passed == total) ? ESP_OK : ESP_FAIL;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* ota_error_to_string(ota_error_t error)
{
    switch (error) {
        case OTA_ERROR_NONE:                return "No error";
        case OTA_ERROR_NO_INTERNET:         return "No internet connection";
        case OTA_ERROR_DOWNLOAD_FAILED:     return "Download failed";
        case OTA_ERROR_CHECKSUM_MISMATCH:   return "Checksum mismatch";
        case OTA_ERROR_FLASH_FAILED:        return "Flash operation failed";
        case OTA_ERROR_NO_UPDATE_PARTITION: return "No OTA partition available";
        case OTA_ERROR_INVALID_VERSION:     return "Invalid version";
        case OTA_ERROR_LOW_MEMORY:          return "Insufficient memory";
        case OTA_ERROR_ALREADY_RUNNING:     return "Update already in progress";
        default:                            return "Unknown error";
    }
}

//=============================================================================
// Initialization
//=============================================================================

esp_err_t ota_manager_init(void)
{
    if (ota_context.initialized) {
        ESP_LOGW(TAG, "OTA manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing OTA manager...");

    // Create mutex
    ota_context.mutex = xSemaphoreCreateMutex();
    if (ota_context.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create OTA mutex");
        return ESP_FAIL;
    }

    // Check partition table
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);

    if (running == NULL || update == NULL) {
        ESP_LOGE(TAG, "❌ OTA partitions not found - OTA not available");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "📋 Running partition: %s (type %d, subtype %d, address 0x%lx, size %lu KB)",
            running->label, running->type, running->subtype,
            running->address, running->size / 1024);

    ESP_LOGI(TAG, "📋 Update partition:  %s (type %d, subtype %d, address 0x%lx, size %lu KB)",
            update->label, update->type, update->subtype,
            update->address, update->size / 1024);

    // Initialize NVS namespace
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_close(nvs_handle);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // Create namespace
        ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (ret == ESP_OK) {
            nvs_close(nvs_handle);
        }
    }

    // Check boot count
    uint32_t boot_count = ota_get_boot_count();
    ESP_LOGI(TAG, "Boot count: %lu", boot_count);

    // Check if first boot after OTA
    if (ota_is_first_boot_after_update()) {
        ESP_LOGW(TAG, "⚠️ First boot after OTA update - health checks required!");
        ESP_LOGW(TAG, "⚠️ Call ota_mark_app_valid() after verifying system health");

        // Check if boot loop (more than 3 boots without validation)
        if (boot_count > 3) {
            ESP_LOGE(TAG, "❌ CRITICAL: Boot loop detected (%lu boots without validation)", boot_count);
            ESP_LOGE(TAG, "❌ Triggering automatic rollback...");
            vTaskDelay(pdMS_TO_TICKS(5000));  // Give time to read logs
            ota_trigger_rollback();  // Never returns
        }
    }

    // Print current version
    firmware_version_t current;
    ota_get_current_version(&current);

    ota_context.initialized = true;
    ESP_LOGI(TAG, "✅ OTA manager initialized");

    return ESP_OK;
}

bool ota_manager_is_available(void)
{
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    return (update != NULL);
}
