/**
 * @file filesystem_manager.c
 * @brief File System Manager Implementation
 */

#include "filesystem_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "external_flash.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>

static const char *TAG = "filesystem_manager";

// Filesystem state
static bool s_initialized = false;
static const esp_partition_t *s_partition = NULL;

// LittleFS configuration (will be populated with partition pointer)
static esp_vfs_littlefs_conf_t s_littlefs_conf = {
    .base_path = FILESYSTEM_MOUNT_POINT,
    .partition_label = NULL,  // Will use partition pointer instead
    .partition = NULL,         // Set during init
    .format_if_mount_failed = true,
    .dont_mount = false,
};

esp_err_t filesystem_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing LittleFS filesystem");
    ESP_LOGI(TAG, "Mount point: %s", FILESYSTEM_MOUNT_POINT);

    // Step 1: Register external flash as partition
    ESP_LOGI(TAG, "Registering external flash as partition...");
    esp_err_t ret = external_flash_register_partition(&s_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register external flash partition (%s)", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "External flash partition registered successfully");

    // Step 2: Configure LittleFS to use the partition
    s_littlefs_conf.partition = s_partition;

    // Step 3: Register and mount LittleFS
    ESP_LOGI(TAG, "Mounting LittleFS...");
    ret = esp_vfs_littlefs_register(&s_littlefs_conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LittleFS mounted successfully");

    // Get filesystem info
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(s_partition->label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Filesystem size: total=%zu KB, used=%zu KB (%.1f%%)",
                 total / 1024, used / 1024, 100.0 * used / total);
    }

    // Create default directories
    ESP_LOGI(TAG, "Creating default directories");
    filesystem_create_directory(FILESYSTEM_CHIMES_DIR);
    filesystem_create_directory(FILESYSTEM_CONFIG_DIR);

    ESP_LOGI(TAG, "Filesystem manager initialized");
    return ESP_OK;
}

esp_err_t filesystem_manager_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing filesystem");

    esp_err_t ret = esp_vfs_littlefs_unregister_partition(s_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount filesystem (%s)", esp_err_to_name(ret));
        return ret;
    }

    s_partition = NULL;
    s_initialized = false;
    ESP_LOGI(TAG, "Filesystem unmounted");
    return ESP_OK;
}

bool filesystem_is_initialized(void)
{
    return s_initialized;
}

esp_err_t filesystem_list_files(const char *path,
                                 filesystem_file_info_t *file_list,
                                 size_t max_files,
                                 size_t *count)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (path == NULL || file_list == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;

    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_files) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        filesystem_file_info_t *info = &file_list[*count];

        // Set name
        strncpy(info->name, entry->d_name, FILESYSTEM_MAX_FILENAME - 1);
        info->name[FILESYSTEM_MAX_FILENAME - 1] = '\0';

        // Set full path (make sure we don't overflow)
        size_t path_len = strlen(path);
        size_t name_len = strlen(entry->d_name);
        if (path_len + name_len + 2 > FILESYSTEM_MAX_FILENAME) {
            ESP_LOGW(TAG, "Path too long, skipping: %s/%s", path, entry->d_name);
            continue;
        }
        // We've already checked the length above, so disable truncation warning
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(info->path, FILESYSTEM_MAX_FILENAME, "%s/%s", path, entry->d_name);
        #pragma GCC diagnostic pop

        // Get file info
        struct stat st;
        if (stat(info->path, &st) == 0) {
            info->size = st.st_size;
            info->is_directory = S_ISDIR(st.st_mode);
        } else {
            info->size = 0;
            info->is_directory = (entry->d_type == DT_DIR);
        }

        (*count)++;
    }

    closedir(dir);

    ESP_LOGI(TAG, "Listed %zu files in %s", *count, path);
    return ESP_OK;
}

esp_err_t filesystem_delete_file(const char *filepath)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!filesystem_file_exists(filepath)) {
        ESP_LOGW(TAG, "File does not exist: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    if (unlink(filepath) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", filepath);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleted file: %s", filepath);
    return ESP_OK;
}

esp_err_t filesystem_rename_file(const char *old_path, const char *new_path)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (old_path == NULL || new_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!filesystem_file_exists(old_path)) {
        ESP_LOGW(TAG, "Source file does not exist: %s", old_path);
        return ESP_ERR_NOT_FOUND;
    }

    if (rename(old_path, new_path) != 0) {
        ESP_LOGE(TAG, "Failed to rename %s to %s", old_path, new_path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Renamed %s to %s", old_path, new_path);
    return ESP_OK;
}

bool filesystem_file_exists(const char *filepath)
{
    if (!s_initialized || filepath == NULL) {
        return false;
    }

    struct stat st;
    return (stat(filepath, &st) == 0);
}

esp_err_t filesystem_get_file_size(const char *filepath, size_t *size)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGW(TAG, "File does not exist: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    *size = st.st_size;
    return ESP_OK;
}

esp_err_t filesystem_get_usage(size_t *total_bytes, size_t *used_bytes)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (total_bytes == NULL || used_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Use partition label from the registered partition
    esp_err_t ret = esp_littlefs_info(s_partition->label, total_bytes, used_bytes);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get filesystem info (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t filesystem_format(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(TAG, "Formatting filesystem - all data will be lost!");

    // Unmount first
    esp_err_t ret = esp_vfs_littlefs_unregister(s_partition->label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount before format (%s)", esp_err_to_name(ret));
        return ret;
    }

    // Format
    ret = esp_littlefs_format(s_partition->label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Format failed (%s)", esp_err_to_name(ret));
        // Try to remount anyway
        esp_vfs_littlefs_register(&s_littlefs_conf);
        return ESP_FAIL;
    }

    // Remount
    ret = esp_vfs_littlefs_register(&s_littlefs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remount after format (%s)", esp_err_to_name(ret));
        s_initialized = false;
        return ret;
    }

    ESP_LOGI(TAG, "Filesystem formatted successfully");

    // Recreate default directories
    filesystem_create_directory(FILESYSTEM_CHIMES_DIR);
    filesystem_create_directory(FILESYSTEM_CONFIG_DIR);

    return ESP_OK;
}

esp_err_t filesystem_create_directory(const char *path)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if already exists
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK;  // Already exists
        } else {
            ESP_LOGE(TAG, "Path exists but is not a directory: %s", path);
            return ESP_FAIL;
        }
    }

    // Create directory (mkdir doesn't create parent dirs in LittleFS)
    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Created directory: %s", path);
    return ESP_OK;
}

esp_err_t filesystem_read_file(const char *filepath, uint8_t **buffer, size_t *size)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL || buffer == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get file size
    esp_err_t ret = filesystem_get_file_size(filepath, size);
    if (ret != ESP_OK) {
        return ret;
    }

    // Allocate buffer
    *buffer = malloc(*size);
    if (*buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for file: %s", *size, filepath);
        return ESP_ERR_NO_MEM;
    }

    // Open and read file
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", filepath);
        free(*buffer);
        *buffer = NULL;
        return ESP_FAIL;
    }

    size_t read_bytes = fread(*buffer, 1, *size, f);
    fclose(f);

    if (read_bytes != *size) {
        ESP_LOGE(TAG, "Failed to read complete file: %s (read %zu of %zu bytes)",
                 filepath, read_bytes, *size);
        free(*buffer);
        *buffer = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Read file: %s (%zu bytes)", filepath, *size);
    return ESP_OK;
}

esp_err_t filesystem_write_file(const char *filepath, const uint8_t *buffer, size_t size)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL || buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Open file for writing
    FILE *f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
        return ESP_FAIL;
    }

    size_t written_bytes = fwrite(buffer, 1, size, f);
    fclose(f);

    if (written_bytes != size) {
        ESP_LOGE(TAG, "Failed to write complete file: %s (wrote %zu of %zu bytes)",
                 filepath, written_bytes, size);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Wrote file: %s (%zu bytes)", filepath, size);
    return ESP_OK;
}
