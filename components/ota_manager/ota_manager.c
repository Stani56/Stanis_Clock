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
#include "mbedtls/sha256.h"
#include "esp_partition.h"
#include <string.h>
#include <sys/time.h>

// External dependencies
extern bool thread_safe_get_wifi_connected(void);
extern bool thread_safe_get_mqtt_connected(void);
extern esp_err_t ds3231_get_time_struct(void*);  // For I2C health check

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
    char expected_sha256[65];  // Expected SHA-256 from version.json (64 hex chars + null)
    ota_progress_callback_t progress_callback;  // Optional progress callback
} ota_context = {
    .state = OTA_STATE_IDLE,
    .error = OTA_ERROR_NONE,
    .progress_percent = 0,
    .bytes_downloaded = 0,
    .total_bytes = 0,
    .start_time_ms = 0,
    .mutex = NULL,
    .initialized = false,
    .expected_sha256 = "",
    .progress_callback = NULL
};

// NVS keys for OTA state
#define NVS_NAMESPACE       "ota_manager"
#define NVS_KEY_BOOT_COUNT  "boot_count"
#define NVS_KEY_FIRST_BOOT  "first_boot"
#define NVS_KEY_OTA_SOURCE  "ota_source"

// Health check configuration
#define OTA_HEALTH_CHECK_DELAY_MS          30000  // 30s grace period for system stabilization
#define OTA_HEALTH_CHECK_RETRY_ATTEMPTS    3      // Number of retry attempts
#define OTA_HEALTH_CHECK_RETRY_INTERVAL_MS 30000  // 30s between retry attempts
#define OTA_CRITICAL_CHECKS_REQUIRED       3      // I2C + Memory + Partition must pass

// Forward declarations
static void ota_task(void *pvParameters);
static void ota_health_check_task(void *pvParameters);
static void get_urls_for_source(ota_source_t source, const char **firmware_url, const char **version_url);

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
 * @brief Calculate SHA-256 checksum of OTA partition
 *
 * @param partition Partition to calculate checksum for
 * @param firmware_size Size of firmware in bytes (0 = use full partition size)
 * @param sha256_hex Output buffer for hex string (65 bytes minimum)
 * @return ESP_OK on success, error otherwise
 */
static esp_err_t calculate_partition_sha256(const esp_partition_t *partition, size_t firmware_size, char *sha256_hex)
{
    if (!partition || !sha256_hex) {
        return ESP_ERR_INVALID_ARG;
    }

    // Use firmware_size if provided, otherwise use full partition size
    size_t bytes_to_hash = (firmware_size > 0) ? firmware_size : partition->size;

    ESP_LOGI(TAG, "Calculating SHA-256 checksum of %lu bytes (partition size: %lu bytes)...",
             (unsigned long)bytes_to_hash, (unsigned long)partition->size);

    unsigned char sha256_output[32];  // SHA-256 produces 32 bytes
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 (not SHA-224)

    const size_t CHUNK_SIZE = 4096;  // Read 4KB at a time
    uint8_t *buffer = malloc(CHUNK_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for SHA-256 calculation");
        mbedtls_sha256_free(&ctx);
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;

    while (offset < bytes_to_hash) {
        size_t read_size = (bytes_to_hash - offset > CHUNK_SIZE) ? CHUNK_SIZE : (bytes_to_hash - offset);

        esp_err_t ret = esp_partition_read(partition, offset, buffer, read_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read partition at offset %zu: %s", offset, esp_err_to_name(ret));
            free(buffer);
            mbedtls_sha256_free(&ctx);
            return ret;
        }

        mbedtls_sha256_update(&ctx, buffer, read_size);
        offset += read_size;

        // Log progress every 100KB
        if (offset % (100 * 1024) == 0 || offset == bytes_to_hash) {
            ESP_LOGD(TAG, "SHA-256 progress: %lu / %lu bytes (%.1f%%)",
                     (unsigned long)offset, (unsigned long)bytes_to_hash, (offset * 100.0f) / bytes_to_hash);
        }
    }

    mbedtls_sha256_finish(&ctx, sha256_output);
    mbedtls_sha256_free(&ctx);
    free(buffer);

    // Convert binary hash to hex string
    for (int i = 0; i < 32; i++) {
        sprintf(&sha256_hex[i * 2], "%02x", sha256_output[i]);
    }
    sha256_hex[64] = '\0';

    ESP_LOGI(TAG, "SHA-256 calculated: %.16s...", sha256_hex);
    return ESP_OK;
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

/**
 * @brief Normalize version string to semver format (strip git metadata)
 *
 * Converts: "v2.6.3-2-g1052452-dirty" → "2.6.3"
 * Converts: "2.6.3" → "2.6.3"
 * Converts: "v2.6.3" → "2.6.3"
 *
 * @param version Input version string (may be git describe format)
 * @param normalized Output buffer for normalized version
 * @param max_len Maximum length of output buffer
 */
static void normalize_version(const char *version, char *normalized, size_t max_len)
{
    if (version == NULL || normalized == NULL || max_len == 0) {
        return;
    }

    // Skip leading 'v' if present
    const char *start = version;
    if (start[0] == 'v' || start[0] == 'V') {
        start++;
    }

    // Copy until we hit git metadata markers (-, +) or end
    size_t i = 0;
    while (start[i] != '\0' && i < (max_len - 1)) {
        // Stop at first '-' (git commit count) or '+' (build metadata)
        if (start[i] == '-' || start[i] == '+') {
            break;
        }
        normalized[i] = start[i];
        i++;
    }
    normalized[i] = '\0';

    ESP_LOGD(TAG, "Version normalized: '%s' → '%s'", version, normalized);
}

/**
 * @brief Compare two version strings (semver-aware)
 *
 * Normalizes both versions before comparison to handle git describe format.
 * Returns: 0 if equal, <0 if v1 < v2, >0 if v1 > v2
 *
 * @param v1 First version string
 * @param v2 Second version string
 * @return int Comparison result
 */
static int compare_versions(const char *v1, const char *v2)
{
    char norm1[32] = {0};
    char norm2[32] = {0};

    normalize_version(v1, norm1, sizeof(norm1));
    normalize_version(v2, norm2, sizeof(norm2));

    // Parse major.minor.patch from both versions
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;

    sscanf(norm1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(norm2, "%d.%d.%d", &major2, &minor2, &patch2);

    // Compare major version
    if (major1 != major2) {
        return major1 - major2;
    }

    // Compare minor version
    if (minor1 != minor2) {
        return minor1 - minor2;
    }

    // Compare patch version
    return patch1 - patch2;
}

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

    // Calculate SHA-256 of running firmware to get binary_hash
    if (running) {
        // Read app_desc from partition to get firmware size
        esp_app_desc_t partition_desc;
        esp_err_t err = esp_ota_get_partition_description(running, &partition_desc);
        if (err == ESP_OK) {
            // Calculate SHA-256 of the firmware image (not entire partition)
            uint8_t sha256[32];
            esp_partition_get_sha256(running, sha256);

            // Convert to hex string
            for (int i = 0; i < 32; i++) {
                sprintf(&version->sha256[i * 2], "%02x", sha256[i]);
            }
            version->sha256[64] = '\0';

            // Extract first 8 chars as binary_hash
            strncpy(version->binary_hash, version->sha256, 8);
            version->binary_hash[8] = '\0';

            ESP_LOGI(TAG, "Current firmware: %s (built %s, IDF %s, binary_hash: %s)",
                     version->version, version->build_date, version->idf_version,
                     version->binary_hash);
        } else {
            version->sha256[0] = '\0';
            version->binary_hash[0] = '\0';
            ESP_LOGW(TAG, "Could not calculate SHA-256 for running firmware");
        }
    } else {
        version->sha256[0] = '\0';
        version->binary_hash[0] = '\0';
        ESP_LOGI(TAG, "Current firmware: %s (built %s, IDF %s)",
                 version->version, version->build_date, version->idf_version);
    }

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

    // Get preferred OTA source and URLs
    ota_source_t preferred_source = ota_get_source();
    ota_source_t fallback_source = (preferred_source == OTA_SOURCE_GITHUB) ?
                                    OTA_SOURCE_CLOUDFLARE : OTA_SOURCE_GITHUB;

    const char *firmware_url, *version_url;
    get_urls_for_source(preferred_source, &firmware_url, &version_url);

    ESP_LOGI(TAG, "Checking for updates from %s (fallback: %s)",
             ota_source_to_string(preferred_source),
             ota_source_to_string(fallback_source));

    // Configure HTTP client for version check with custom event handler
    // Use ESP-IDF's built-in certificate bundle (includes GitHub's root CA)
    esp_http_client_config_t config = {
        .url = version_url,
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

    // Try failover if preferred source failed
    if (!(err == ESP_OK && status == 200 && output_buffer.data_len > 0)) {
        ESP_LOGW(TAG, "Failed to fetch from %s (status=%d), trying fallback: %s",
                 ota_source_to_string(preferred_source), status,
                 ota_source_to_string(fallback_source));

        // Clean up previous client
        esp_http_client_cleanup(client);

        // Reset buffer for second attempt
        output_buffer.data_len = 0;

        // Get fallback URLs
        get_urls_for_source(fallback_source, &firmware_url, &version_url);
        config.url = version_url;

        // Try fallback source
        client = esp_http_client_init(&config);
        if (client != NULL) {
            err = esp_http_client_perform(client);
            status = esp_http_client_get_status_code(client);
            content_length = esp_http_client_get_content_length(client);

            ESP_LOGI(TAG, "Fallback HTTP GET Status = %d, content_length = %d, captured = %d bytes",
                     status, content_length, output_buffer.data_len);
        }
    }

    if (err == ESP_OK && status == 200 && output_buffer.data_len > 0) {
        // Data was captured by event handler (from either primary or fallback)
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
            cJSON *sha256_obj = cJSON_GetObjectItem(json, "sha256");

            ESP_LOGI(TAG, "JSON fields: version=%p, date=%p, size=%p, sha256=%p",
                    version_obj, date_obj, size_obj, sha256_obj);

            if (version_obj && date_obj) {
                strncpy(available_version->version, version_obj->valuestring,
                        sizeof(available_version->version) - 1);
                available_version->version[sizeof(available_version->version) - 1] = '\0';

                strncpy(available_version->build_date, date_obj->valuestring,
                        sizeof(available_version->build_date) - 1);
                available_version->build_date[sizeof(available_version->build_date) - 1] = '\0';

                available_version->size_bytes = size_obj ? size_obj->valueint : 0;

                // Parse SHA-256 checksum if available
                if (sha256_obj && cJSON_IsString(sha256_obj)) {
                    strncpy(available_version->sha256, sha256_obj->valuestring,
                            sizeof(available_version->sha256) - 1);
                    available_version->sha256[sizeof(available_version->sha256) - 1] = '\0';

                    // Extract first 8 chars as binary_hash
                    strncpy(available_version->binary_hash, available_version->sha256, 8);
                    available_version->binary_hash[8] = '\0';

                    ESP_LOGI(TAG, "SHA-256: %.16s..., binary_hash: %s",
                            available_version->sha256, available_version->binary_hash);
                } else {
                    available_version->sha256[0] = '\0';  // Empty string = no checksum
                    available_version->binary_hash[0] = '\0';
                    ESP_LOGW(TAG, "No SHA-256 checksum in version.json");
                }

                // Get current firmware info
                firmware_version_t current;
                ota_get_current_version(&current);

                // Compare binary_hash FIRST (most reliable indicator of binary change)
                if (current.binary_hash[0] != '\0' && available_version->binary_hash[0] != '\0') {
                    if (strcmp(current.binary_hash, available_version->binary_hash) == 0) {
                        // Binary hashes match - same firmware binary
                        ESP_LOGI(TAG, "Already running same firmware binary (hash: %s, version: %s)",
                                current.binary_hash, current.version);
                        ret = ESP_ERR_NOT_FOUND;
                    } else {
                        // Binary hashes differ - different firmware binary
                        ESP_LOGI(TAG, "Update available: Binary changed (%s → %s), version %s → %s",
                                current.binary_hash, available_version->binary_hash,
                                current.version, available_version->version);
                        ret = ESP_OK;
                    }
                } else {
                    // Fallback to version comparison if binary_hash not available
                    ESP_LOGW(TAG, "Binary hash not available, falling back to version comparison");
                    int version_cmp = compare_versions(current.version, available_version->version);
                    if (version_cmp < 0) {
                        // Current version is older than available version
                        ESP_LOGI(TAG, "Update available: %s → %s",
                                current.version, available_version->version);
                        ret = ESP_OK;
                    } else if (version_cmp == 0) {
                        // Versions are equal (same major.minor.patch)
                        ESP_LOGI(TAG, "Already running latest version: %s (available: %s)",
                                current.version, available_version->version);
                        ret = ESP_ERR_NOT_FOUND;
                    } else {
                        // Current version is NEWER than available (dev build?)
                        ESP_LOGW(TAG, "Current version %s is NEWER than available %s (development build?)",
                                current.version, available_version->version);
                        ret = ESP_ERR_NOT_FOUND;
                    }
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

        // Log progress every 10% and call callback every 5%
        static uint8_t last_progress_log = 0;
        static uint8_t last_progress_callback = 0;

        if (ota_context.progress_percent >= last_progress_log + 10) {
            last_progress_log = ota_context.progress_percent;
            ESP_LOGI(TAG, "📊 Progress: %d%% (%lu / %lu bytes)",
                    ota_context.progress_percent,
                    ota_context.bytes_downloaded,
                    ota_context.total_bytes);
        }

        // Call progress callback every 5% if configured
        if (ota_context.progress_callback &&
            ota_context.progress_percent >= last_progress_callback + 5) {
            last_progress_callback = ota_context.progress_percent;

            ota_progress_t progress;
            if (ota_get_progress(&progress) == ESP_OK) {
                ota_context.progress_callback(&progress);
            }
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

    // SHA-256 verification (if checksum was provided in version.json)
    char expected_sha256[65];
    if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        strncpy(expected_sha256, ota_context.expected_sha256, sizeof(expected_sha256));
        xSemaphoreGive(ota_context.mutex);
    }

    if (expected_sha256[0] != '\0') {
        ESP_LOGI(TAG, "🔐 Verifying firmware SHA-256 checksum...");

        // Get the partition that was just written to (the next OTA partition)
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition == NULL) {
            ESP_LOGE(TAG, "❌ Failed to get update partition");
            esp_https_ota_abort(https_ota_handle);
            update_state(OTA_STATE_FAILED, OTA_ERROR_FLASH_FAILED);
            vTaskDelete(NULL);
            return;
        }

        // Get firmware size from OTA context (total_bytes set by esp_https_ota)
        uint32_t firmware_size = 0;
        if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            firmware_size = ota_context.total_bytes;
            xSemaphoreGive(ota_context.mutex);
        }

        if (firmware_size == 0) {
            ESP_LOGE(TAG, "❌ Firmware size is 0, cannot verify");
            esp_https_ota_abort(https_ota_handle);
            update_state(OTA_STATE_FAILED, OTA_ERROR_CHECKSUM_MISMATCH);
            vTaskDelete(NULL);
            return;
        }

        ESP_LOGI(TAG, "Firmware size: %lu bytes", firmware_size);

        // Calculate actual SHA-256 of downloaded firmware (only the actual firmware bytes)
        char actual_sha256[65];
        ret = calculate_partition_sha256(update_partition, firmware_size, actual_sha256);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to calculate SHA-256: %s", esp_err_to_name(ret));
            esp_https_ota_abort(https_ota_handle);
            update_state(OTA_STATE_FAILED, OTA_ERROR_CHECKSUM_MISMATCH);
            vTaskDelete(NULL);
            return;
        }

        // Compare checksums (case-insensitive)
        if (strcasecmp(expected_sha256, actual_sha256) != 0) {
            ESP_LOGE(TAG, "❌ SHA-256 CHECKSUM MISMATCH!");
            ESP_LOGE(TAG, "   Expected: %s", expected_sha256);
            ESP_LOGE(TAG, "   Actual:   %s", actual_sha256);
            esp_https_ota_abort(https_ota_handle);
            update_state(OTA_STATE_FAILED, OTA_ERROR_CHECKSUM_MISMATCH);
            vTaskDelete(NULL);
            return;
        }

        ESP_LOGI(TAG, "✅ SHA-256 verification passed");
        ESP_LOGI(TAG, "   Checksum: %.16s...", actual_sha256);
    } else {
        ESP_LOGW(TAG, "⚠️ Skipping SHA-256 verification (no checksum in version.json)");
    }

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

    // Get URLs from preferred OTA source
    ota_source_t preferred_source = ota_get_source();
    const char *firmware_url, *version_url;
    get_urls_for_source(preferred_source, &firmware_url, &version_url);

    // Use default config if not provided (with dynamically selected URLs)
    static ota_config_t default_config;
    default_config.firmware_url = firmware_url;
    default_config.version_url = version_url;
    default_config.auto_reboot = true;
    default_config.timeout_ms = OTA_DEFAULT_TIMEOUT_MS;
    default_config.skip_version_check = false;
    default_config.progress_callback = NULL;

    if (config == NULL) {
        config = &default_config;
    }

    // Store progress callback in OTA context (thread-safe)
    if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ota_context.progress_callback = config->progress_callback;
        xSemaphoreGive(ota_context.mutex);
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

        // Store expected SHA-256 for verification after download
        if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            strncpy(ota_context.expected_sha256, available.sha256, sizeof(ota_context.expected_sha256) - 1);
            ota_context.expected_sha256[sizeof(ota_context.expected_sha256) - 1] = '\0';
            xSemaphoreGive(ota_context.mutex);

            if (available.sha256[0] != '\0') {
                ESP_LOGI(TAG, "Expected firmware SHA-256: %.16s...", available.sha256);
            } else {
                ESP_LOGW(TAG, "No SHA-256 checksum available - verification will be skipped");
            }
        }
    } else {
        // If skipping version check, clear expected SHA-256
        if (xSemaphoreTake(ota_context.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ota_context.expected_sha256[0] = '\0';
            xSemaphoreGive(ota_context.mutex);
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

    esp_err_t state_ret = esp_ota_get_state_partition(running, &ota_state);
    ESP_LOGI(TAG, "📊 esp_ota_get_state_partition() returned: %s, state value: %d",
             esp_err_to_name(state_ret), ota_state);

    if (state_ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "ℹ️ Partition state not supported (fresh flash or factory partition)");

        // Clear first boot flag - this is normal for fresh flash
        nvs_handle_t nvs_handle;
        if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_u8(nvs_handle, NVS_KEY_FIRST_BOOT, 0);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }

        return ESP_OK;  // This is normal for fresh flash
    } else if (state_ret == ESP_OK) {
        ESP_LOGI(TAG, "📊 Current partition state: %d", ota_state);

        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "🔄 Partition is PENDING_VERIFY, marking as valid...");
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
            } else {
                ESP_LOGE(TAG, "❌ esp_ota_mark_app_valid_cancel_rollback() failed: %s",
                         esp_err_to_name(ret));
                return ESP_FAIL;
            }
        } else if (ota_state == ESP_OTA_IMG_VALID) {
            ESP_LOGI(TAG, "ℹ️ Partition already marked VALID (possibly by bootloader)");

            // Clear first boot flag anyway
            nvs_handle_t nvs_handle;
            if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                nvs_set_u8(nvs_handle, NVS_KEY_FIRST_BOOT, 0);
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
            }

            return ESP_OK;  // This is actually a success case
        } else if (ota_state == ESP_OTA_IMG_INVALID) {
            ESP_LOGE(TAG, "❌ Partition is marked INVALID!");
            return ESP_FAIL;
        } else if (ota_state == ESP_OTA_IMG_ABORTED) {
            ESP_LOGE(TAG, "❌ Partition update was ABORTED!");
            return ESP_FAIL;
        } else if (ota_state == ESP_OTA_IMG_UNDEFINED) {
            ESP_LOGI(TAG, "ℹ️ Partition state is UNDEFINED (fresh flash, otadata not initialized)");

            // Clear first boot flag - this is normal for fresh flash
            nvs_handle_t nvs_handle;
            if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                nvs_set_u8(nvs_handle, NVS_KEY_FIRST_BOOT, 0);
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
            }

            return ESP_OK;  // This is normal for fresh flash
        } else {
            ESP_LOGW(TAG, "⚠️ Unknown partition state: %d", ota_state);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to get partition state");
        return ESP_FAIL;
    }
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

    // 3. I2C devices (RTC check as proxy for I2C health)
    uint8_t dummy_time[7];
    if (ds3231_get_time_struct(dummy_time) == ESP_OK) {
        ESP_LOGI(TAG, "  ✅ I2C: RTC responding (I2C bus operational)");
        passed++;
    } else {
        ESP_LOGE(TAG, "  ❌ I2C: RTC communication failed");
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
// OTA Source Management (Dual Source Support)
//=============================================================================

/**
 * @brief Set preferred OTA firmware source
 */
esp_err_t ota_set_source(ota_source_t source)
{
    if (source != OTA_SOURCE_GITHUB && source != OTA_SOURCE_CLOUDFLARE) {
        ESP_LOGE(TAG, "Invalid OTA source: %d", source);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_set_u8(nvs_handle, NVS_KEY_OTA_SOURCE, (uint8_t)source);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write OTA source to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA source set to: %s", ota_source_to_string(source));
    } else {
        ESP_LOGE(TAG, "Failed to commit OTA source to NVS: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * @brief Get preferred OTA firmware source
 */
ota_source_t ota_get_source(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace, using default source");
        return OTA_DEFAULT_SOURCE;
    }

    uint8_t source_val = OTA_DEFAULT_SOURCE;
    err = nvs_get_u8(nvs_handle, NVS_KEY_OTA_SOURCE, &source_val);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No stored OTA source preference, using default");
        return OTA_DEFAULT_SOURCE;
    }

    // Validate stored value
    if (source_val != OTA_SOURCE_GITHUB && source_val != OTA_SOURCE_CLOUDFLARE) {
        ESP_LOGW(TAG, "Invalid stored OTA source (%d), using default", source_val);
        return OTA_DEFAULT_SOURCE;
    }

    return (ota_source_t)source_val;
}

/**
 * @brief Get OTA source as string
 */
const char* ota_source_to_string(ota_source_t source)
{
    switch (source) {
        case OTA_SOURCE_GITHUB:     return "github";
        case OTA_SOURCE_CLOUDFLARE: return "cloudflare";
        default:                    return "unknown";
    }
}

/**
 * @brief Parse OTA source from string
 */
esp_err_t ota_source_from_string(const char *source_str, ota_source_t *source)
{
    if (source_str == NULL || source == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcasecmp(source_str, "github") == 0) {
        *source = OTA_SOURCE_GITHUB;
        return ESP_OK;
    } else if (strcasecmp(source_str, "cloudflare") == 0) {
        *source = OTA_SOURCE_CLOUDFLARE;
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Invalid OTA source string: %s", source_str);
    return ESP_ERR_INVALID_ARG;
}

/**
 * @brief Get URLs for specified OTA source
 */
static void get_urls_for_source(ota_source_t source, const char **firmware_url, const char **version_url)
{
    switch (source) {
        case OTA_SOURCE_GITHUB:
            *firmware_url = OTA_GITHUB_FIRMWARE_URL;
            *version_url = OTA_GITHUB_VERSION_URL;
            ESP_LOGI(TAG, "Using GitHub URLs");
            break;
        case OTA_SOURCE_CLOUDFLARE:
            *firmware_url = OTA_CLOUDFLARE_FIRMWARE_URL;
            *version_url = OTA_CLOUDFLARE_VERSION_URL;
            ESP_LOGI(TAG, "Using Cloudflare R2 URLs");
            break;
        default:
            *firmware_url = OTA_GITHUB_FIRMWARE_URL;
            *version_url = OTA_GITHUB_VERSION_URL;
            ESP_LOGW(TAG, "Unknown source, defaulting to GitHub");
            break;
    }
}

//=============================================================================
// Automatic Health Validation
//=============================================================================

/**
 * @brief Automatic health check task with tiered validation and retry logic
 *
 * Runs automatically after OTA update to validate new firmware.
 * Uses tiered approach:
 *   - Critical checks: I2C, Memory, Partition (MUST pass)
 *   - Network checks: WiFi, MQTT (retried 3 times with 30s intervals)
 *
 * Accepts firmware if critical checks pass, even if network is temporarily unavailable.
 * Only triggers rollback if critical hardware/firmware checks fail.
 */
static void ota_health_check_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🏥 Automatic health validation task started");

    // Wait for system to stabilize
    ESP_LOGI(TAG, "⏳ Waiting %d seconds for system stabilization...",
             OTA_HEALTH_CHECK_DELAY_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(OTA_HEALTH_CHECK_DELAY_MS));

    // Only run if this is first boot after update
    if (!ota_is_first_boot_after_update()) {
        ESP_LOGI(TAG, "ℹ️ Not first boot after update - skipping validation");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "🔍 Starting automatic health validation with retry logic...");

    bool validation_passed = false;

    // Perform health checks with retries
    for (int attempt = 1; attempt <= OTA_HEALTH_CHECK_RETRY_ATTEMPTS; attempt++) {
        ESP_LOGI(TAG, "📊 Health check attempt %d/%d", attempt, OTA_HEALTH_CHECK_RETRY_ATTEMPTS);

        // Run full health check suite
        esp_err_t result = ota_perform_health_checks();

        if (result == ESP_OK) {
            // All checks passed (including network) - perfect!
            ESP_LOGI(TAG, "✅ All health checks passed (5/5)");
            validation_passed = true;
            break;
        }

        // Some checks failed - determine if critical or non-critical
        ESP_LOGW(TAG, "⚠️ Some health checks failed, analyzing...");

        // Re-check CRITICAL components only
        uint8_t critical_passed = 0;

        // Critical Check 1: I2C Bus (hardware drivers)
        uint8_t dummy_time[7];
        if (ds3231_get_time_struct(dummy_time) == ESP_OK) {
            ESP_LOGI(TAG, "  ✅ CRITICAL: I2C bus operational");
            critical_passed++;
        } else {
            ESP_LOGE(TAG, "  ❌ CRITICAL: I2C bus failed");
        }

        // Critical Check 2: Memory (no leaks/corruption)
        uint32_t free_heap = esp_get_free_heap_size();
        if (free_heap > 50000) {
            ESP_LOGI(TAG, "  ✅ CRITICAL: Memory OK (%lu bytes free)", free_heap);
            critical_passed++;
        } else {
            ESP_LOGE(TAG, "  ❌ CRITICAL: Low memory (%lu bytes)", free_heap);
        }

        // Critical Check 3: Partition (bootloader worked)
        const esp_partition_t *running = esp_ota_get_running_partition();
        if (running != NULL) {
            ESP_LOGI(TAG, "  ✅ CRITICAL: Partition valid (%s)", running->label);
            critical_passed++;
        } else {
            ESP_LOGE(TAG, "  ❌ CRITICAL: Partition invalid");
        }

        ESP_LOGI(TAG, "📊 Critical checks: %d/%d passed", critical_passed, OTA_CRITICAL_CHECKS_REQUIRED);

        // Evaluate results
        if (critical_passed >= OTA_CRITICAL_CHECKS_REQUIRED) {
            // Critical checks passed - firmware is functional
            ESP_LOGI(TAG, "✅ Firmware is functional (critical checks passed)");

            if (attempt < OTA_HEALTH_CHECK_RETRY_ATTEMPTS) {
                // Not final attempt - retry for network services
                ESP_LOGW(TAG, "⚠️ Network services not ready, will retry in %d seconds...",
                         OTA_HEALTH_CHECK_RETRY_INTERVAL_MS / 1000);
                vTaskDelay(pdMS_TO_TICKS(OTA_HEALTH_CHECK_RETRY_INTERVAL_MS));
                continue;
            } else {
                // Final attempt - accept firmware despite network issues
                ESP_LOGW(TAG, "⚠️ VALIDATION PASSED WITH WARNINGS:");
                ESP_LOGW(TAG, "   ✅ Firmware is functional (critical checks passed)");
                ESP_LOGW(TAG, "   ⚠️ Network services not available (non-critical)");
                ESP_LOGW(TAG, "   ℹ️ WiFi/MQTT will reconnect automatically when available");
                validation_passed = true;
                break;
            }
        } else {
            // Critical checks failed - firmware is broken
            ESP_LOGE(TAG, "❌ CRITICAL: Hardware/firmware checks failed (%d/%d)",
                     critical_passed, OTA_CRITICAL_CHECKS_REQUIRED);

            if (attempt < OTA_HEALTH_CHECK_RETRY_ATTEMPTS) {
                ESP_LOGW(TAG, "⏳ Retrying in %d seconds...",
                         OTA_HEALTH_CHECK_RETRY_INTERVAL_MS / 1000);
                vTaskDelay(pdMS_TO_TICKS(OTA_HEALTH_CHECK_RETRY_INTERVAL_MS));
            } else {
                // All attempts exhausted, critical checks still failing
                ESP_LOGE(TAG, "❌ CRITICAL: Firmware validation failed after %d attempts",
                         OTA_HEALTH_CHECK_RETRY_ATTEMPTS);
                validation_passed = false;
                break;
            }
        }
    }

    // Final decision
    if (validation_passed) {
        esp_err_t ret = ota_mark_app_valid();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ AUTOMATIC VALIDATION PASSED - App marked valid");
            ESP_LOGI(TAG, "✅ System is stable, rollback cancelled");
        } else {
            ESP_LOGW(TAG, "⚠️ Validation passed but failed to mark app valid: %s",
                     esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ AUTOMATIC VALIDATION FAILED!");
        ESP_LOGE(TAG, "❌ Critical system checks failed after %d attempts",
                 OTA_HEALTH_CHECK_RETRY_ATTEMPTS);
        ESP_LOGE(TAG, "❌ Triggering rollback to previous firmware in 5 seconds...");

        // Give time for logs to be transmitted before reboot
        vTaskDelay(pdMS_TO_TICKS(5000));

        // Trigger rollback (never returns - reboots to previous firmware)
        ota_trigger_rollback();
    }

    vTaskDelete(NULL);
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

    // Start automatic health validation task
    // This task will wait 30s, then check if this is first boot after OTA
    // If yes, it performs health checks and marks app valid or triggers rollback
    BaseType_t task_created = xTaskCreate(
        ota_health_check_task,
        "ota_health_check",
        4096,  // 4KB stack
        NULL,
        2,     // Low priority (tskIDLE_PRIORITY + 1)
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGW(TAG, "⚠️ Failed to create automatic health check task");
        ESP_LOGW(TAG, "⚠️ Manual validation required via ota_mark_app_valid()");
    } else {
        ESP_LOGI(TAG, "✅ Automatic health validation task created");
    }

    ota_context.initialized = true;
    ESP_LOGI(TAG, "✅ OTA manager initialized");

    return ESP_OK;
}

bool ota_manager_is_available(void)
{
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    return (update != NULL);
}
