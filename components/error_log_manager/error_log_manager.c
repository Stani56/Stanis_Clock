#include "error_log_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "error_log";

// ============================================================================
// Internal State
// ============================================================================

static SemaphoreHandle_t error_log_mutex = NULL;
static nvs_handle_t error_log_nvs_handle = 0;
static bool error_log_initialized = false;

// Cached statistics (loaded from NVS at init, updated in RAM, persisted on change)
static error_log_stats_t cached_stats = {0};

// ============================================================================
// String Conversion Functions
// ============================================================================

const char* error_log_get_source_name(error_source_t source) {
    switch (source) {
        case ERROR_SOURCE_LED_VALIDATION:   return "LED_VALIDATION";
        case ERROR_SOURCE_I2C_BUS:          return "I2C_BUS";
        case ERROR_SOURCE_WIFI:             return "WIFI";
        case ERROR_SOURCE_MQTT:             return "MQTT";
        case ERROR_SOURCE_NTP:              return "NTP";
        case ERROR_SOURCE_SYSTEM:           return "SYSTEM";
        case ERROR_SOURCE_POWER:            return "POWER";
        case ERROR_SOURCE_SENSOR:           return "SENSOR";
        case ERROR_SOURCE_DISPLAY:          return "DISPLAY";
        case ERROR_SOURCE_TRANSITION:       return "TRANSITION";
        case ERROR_SOURCE_BRIGHTNESS:       return "BRIGHTNESS";
        default:                            return "UNKNOWN";
    }
}

const char* error_log_get_severity_name(error_severity_t severity) {
    switch (severity) {
        case ERROR_SEVERITY_INFO:           return "INFO";
        case ERROR_SEVERITY_WARNING:        return "WARNING";
        case ERROR_SEVERITY_ERROR:          return "ERROR";
        case ERROR_SEVERITY_CRITICAL:       return "CRITICAL";
        default:                            return "UNKNOWN";
    }
}

// ============================================================================
// NVS Key Generation
// ============================================================================

static void get_entry_key(uint16_t index, char *key_buf, size_t buf_size) {
    snprintf(key_buf, buf_size, "entry_%03d", index);
}

// ============================================================================
// Statistics Persistence
// ============================================================================

static esp_err_t load_stats_from_nvs(void) {
    esp_err_t ret;

    // Load statistics blob from NVS
    size_t required_size = sizeof(error_log_stats_t);
    ret = nvs_get_blob(error_log_nvs_handle, "stats", &cached_stats, &required_size);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // First time init - zero out stats
        memset(&cached_stats, 0, sizeof(error_log_stats_t));
        ESP_LOGI(TAG, "Initialized new error log statistics");
        return ESP_OK;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load stats from NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Loaded error log stats: %lu total entries, %u in buffer",
             cached_stats.total_entries_written, cached_stats.current_entry_count);

    return ESP_OK;
}

static esp_err_t save_stats_to_nvs(void) {
    esp_err_t ret = nvs_set_blob(error_log_nvs_handle, "stats",
                                  &cached_stats, sizeof(error_log_stats_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save stats to NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(error_log_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit stats to NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ============================================================================
// Initialization
// ============================================================================

esp_err_t error_log_init(void) {
    if (error_log_initialized) {
        ESP_LOGW(TAG, "Error log already initialized");
        return ESP_OK;
    }

    // Create mutex
    error_log_mutex = xSemaphoreCreateMutex();
    if (error_log_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create error log mutex");
        return ESP_FAIL;
    }

    // Open NVS namespace
    esp_err_t ret = nvs_open(ERROR_LOG_NVS_NAMESPACE, NVS_READWRITE, &error_log_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        vSemaphoreDelete(error_log_mutex);
        return ret;
    }

    // Load statistics from NVS
    ret = load_stats_from_nvs();
    if (ret != ESP_OK) {
        nvs_close(error_log_nvs_handle);
        vSemaphoreDelete(error_log_mutex);
        return ret;
    }

    error_log_initialized = true;
    ESP_LOGI(TAG, "✅ Error log manager initialized (buffer: %u/%u entries)",
             cached_stats.current_entry_count, ERROR_LOG_MAX_ENTRIES);

    return ESP_OK;
}

// ============================================================================
// Write Error Entry
// ============================================================================

esp_err_t error_log_write(const error_log_entry_t *entry) {
    if (!error_log_initialized) {
        ESP_LOGE(TAG, "Error log not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(error_log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire error log mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    // Generate NVS key for this entry
    char key[16];
    get_entry_key(cached_stats.head_index, key, sizeof(key));

    // Write entry to NVS
    ret = nvs_set_blob(error_log_nvs_handle, key, entry, sizeof(error_log_entry_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write entry to NVS: %s", esp_err_to_name(ret));
        xSemaphoreGive(error_log_mutex);
        return ret;
    }

    // Update statistics
    cached_stats.total_entries_written++;
    if (cached_stats.current_entry_count < ERROR_LOG_MAX_ENTRIES) {
        cached_stats.current_entry_count++;
    }

    // Update error counts by source
    cached_stats.total_errors++;
    switch (entry->error_source) {
        case ERROR_SOURCE_LED_VALIDATION:
            cached_stats.validation_errors++;
            cached_stats.last_validation_error = entry->timestamp;
            break;
        case ERROR_SOURCE_I2C_BUS:
            cached_stats.i2c_errors++;
            cached_stats.last_i2c_error = entry->timestamp;
            break;
        case ERROR_SOURCE_WIFI:
            cached_stats.wifi_errors++;
            cached_stats.last_wifi_error = entry->timestamp;
            break;
        case ERROR_SOURCE_MQTT:
            cached_stats.mqtt_errors++;
            cached_stats.last_mqtt_error = entry->timestamp;
            break;
        case ERROR_SOURCE_NTP:
            cached_stats.ntp_errors++;
            break;
        case ERROR_SOURCE_SYSTEM:
            cached_stats.system_errors++;
            break;
        default:
            break;
    }

    // Update error counts by severity
    switch (entry->error_severity) {
        case ERROR_SEVERITY_INFO:
            cached_stats.info_count++;
            break;
        case ERROR_SEVERITY_WARNING:
            cached_stats.warning_count++;
            break;
        case ERROR_SEVERITY_ERROR:
            cached_stats.error_count++;
            break;
        case ERROR_SEVERITY_CRITICAL:
            cached_stats.critical_count++;
            break;
    }

    // Advance head pointer (circular)
    cached_stats.head_index = (cached_stats.head_index + 1) % ERROR_LOG_MAX_ENTRIES;

    // Save updated statistics
    ret = save_stats_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save updated stats, continuing anyway");
    }

    // Commit the entry write
    ret = nvs_commit(error_log_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit entry to NVS: %s", esp_err_to_name(ret));
    }

    xSemaphoreGive(error_log_mutex);

    // Log the error entry
    ESP_LOGI(TAG, "📝 Error logged: [%s/%s] %s (code: 0x%04X)",
             error_log_get_source_name(entry->error_source),
             error_log_get_severity_name(entry->error_severity),
             entry->message,
             entry->error_code);

    return ret;
}

// ============================================================================
// Read Error Entries
// ============================================================================

esp_err_t error_log_read_recent(error_log_entry_t *entries,
                                uint16_t max_count,
                                uint16_t *actual_count) {
    if (!error_log_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (entries == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(error_log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Determine how many entries to read
    uint16_t entries_to_read = (max_count < cached_stats.current_entry_count) ?
                                max_count : cached_stats.current_entry_count;
    *actual_count = entries_to_read;

    if (entries_to_read == 0) {
        xSemaphoreGive(error_log_mutex);
        return ESP_OK;
    }

    // Read entries in reverse chronological order (newest first)
    // Head index points to next write position, so head-1 is newest entry
    esp_err_t ret = ESP_OK;
    for (uint16_t i = 0; i < entries_to_read; i++) {
        // Calculate index: head-1, head-2, head-3, ... (wrap around)
        int16_t read_index = cached_stats.head_index - 1 - i;
        if (read_index < 0) {
            read_index += ERROR_LOG_MAX_ENTRIES;
        }

        char key[16];
        get_entry_key((uint16_t)read_index, key, sizeof(key));

        size_t required_size = sizeof(error_log_entry_t);
        esp_err_t read_ret = nvs_get_blob(error_log_nvs_handle, key, &entries[i], &required_size);

        if (read_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read entry %s: %s", key, esp_err_to_name(read_ret));
            *actual_count = i;  // Return what we successfully read
            ret = read_ret;
            break;
        }
    }

    xSemaphoreGive(error_log_mutex);
    return ret;
}

esp_err_t error_log_read_entry(uint16_t index, error_log_entry_t *entry) {
    if (!error_log_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(error_log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (index >= cached_stats.current_entry_count) {
        xSemaphoreGive(error_log_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate actual NVS index
    // If buffer not full: index 0 is at position 0
    // If buffer full: index 0 is at position head_index (oldest entry)
    uint16_t nvs_index;
    if (cached_stats.current_entry_count < ERROR_LOG_MAX_ENTRIES) {
        nvs_index = index;
    } else {
        nvs_index = (cached_stats.head_index + index) % ERROR_LOG_MAX_ENTRIES;
    }

    char key[16];
    get_entry_key(nvs_index, key, sizeof(key));

    size_t required_size = sizeof(error_log_entry_t);
    esp_err_t ret = nvs_get_blob(error_log_nvs_handle, key, entry, &required_size);

    xSemaphoreGive(error_log_mutex);
    return ret;
}

// ============================================================================
// Get Statistics
// ============================================================================

esp_err_t error_log_get_stats(error_log_stats_t *stats) {
    if (!error_log_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(error_log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(stats, &cached_stats, sizeof(error_log_stats_t));

    xSemaphoreGive(error_log_mutex);
    return ESP_OK;
}

// ============================================================================
// Clear and Reset Functions
// ============================================================================

esp_err_t error_log_clear(void) {
    if (!error_log_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(error_log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Clearing all error log entries...");

    // Erase all entry keys from NVS
    esp_err_t ret = ESP_OK;
    for (uint16_t i = 0; i < ERROR_LOG_MAX_ENTRIES; i++) {
        char key[16];
        get_entry_key(i, key, sizeof(key));

        esp_err_t erase_ret = nvs_erase_key(error_log_nvs_handle, key);
        if (erase_ret != ESP_OK && erase_ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to erase key %s: %s", key, esp_err_to_name(erase_ret));
            ret = erase_ret;
        }
    }

    // Reset buffer metadata
    cached_stats.current_entry_count = 0;
    cached_stats.head_index = 0;

    // Save updated stats (preserves counters)
    esp_err_t save_ret = save_stats_to_nvs();
    if (save_ret != ESP_OK) {
        ret = save_ret;
    }

    // Commit changes
    esp_err_t commit_ret = nvs_commit(error_log_nvs_handle);
    if (commit_ret != ESP_OK) {
        ret = commit_ret;
    }

    xSemaphoreGive(error_log_mutex);

    ESP_LOGI(TAG, "✅ Error log cleared (statistics preserved)");
    return ret;
}

esp_err_t error_log_reset_stats(void) {
    if (!error_log_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(error_log_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Resetting error log statistics...");

    // Preserve buffer metadata, reset counters
    uint16_t saved_count = cached_stats.current_entry_count;
    uint16_t saved_head = cached_stats.head_index;
    uint32_t saved_total = cached_stats.total_entries_written;

    memset(&cached_stats, 0, sizeof(error_log_stats_t));

    cached_stats.current_entry_count = saved_count;
    cached_stats.head_index = saved_head;
    cached_stats.total_entries_written = saved_total;

    esp_err_t ret = save_stats_to_nvs();

    xSemaphoreGive(error_log_mutex);

    ESP_LOGI(TAG, "✅ Error log statistics reset");
    return ret;
}
