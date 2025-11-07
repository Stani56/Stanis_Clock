/**
 * @file sdcard_manager.c
 * @brief SD Card Manager Implementation
 */

#include "sdcard_manager.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

static const char *TAG = "sdcard_mgr";

static bool initialized = false;
static sdmmc_card_t *card = NULL;

esp_err_t sdcard_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "SD card already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing SD card (SPI mode)");
    ESP_LOGI(TAG, "GPIO: MOSI=%d, MISO=%d, CLK=%d, CS=%d",
             SDCARD_GPIO_MOSI, SDCARD_GPIO_MISO, SDCARD_GPIO_CLK, SDCARD_GPIO_CS);

    // Configure SPI bus
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = SDCARD_MAX_FILES,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SDCARD_GPIO_MOSI,
        .miso_io_num = SDCARD_GPIO_MISO,
        .sclk_io_num = SDCARD_GPIO_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "SPI bus already initialized - reusing existing bus");
    }

    // Configure SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SDCARD_GPIO_CS;
    slot_config.host_id = host.slot;

    // Mount filesystem
    ESP_LOGI(TAG, "Mounting SD card filesystem at %s", SDCARD_MOUNT_POINT);
    ret = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Is SD card inserted?");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));
        }
        spi_bus_free(host.slot);
        return ret;
    }

    initialized = true;

    // Print card info
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD card mounted successfully at %s", SDCARD_MOUNT_POINT);

    return ESP_OK;
}

esp_err_t sdcard_manager_deinit(void)
{
    if (!initialized) {
        ESP_LOGW(TAG, "SD card not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Unmounting SD card");
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    // Note: Don't free SPI bus as it might be shared with other components
    // esp_vfs_fat_sdcard_unmount() already handles necessary cleanup
    initialized = false;
    card = NULL;

    ESP_LOGI(TAG, "SD card unmounted successfully");
    return ESP_OK;
}

bool sdcard_file_exists(const char *filepath)
{
    if (!initialized || !filepath) {
        return false;
    }

    struct stat st;
    return (stat(filepath, &st) == 0);
}

esp_err_t sdcard_get_file_size(const char *filepath, size_t *size)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!filepath || !size) {
        return ESP_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(filepath, &st) != 0) {
        return ESP_ERR_NOT_FOUND;
    }

    *size = st.st_size;
    return ESP_OK;
}

esp_err_t sdcard_list_files(const char *dirpath, char ***files, size_t *count)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!dirpath || !files || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", dirpath);
        return ESP_ERR_NOT_FOUND;
    }

    // First pass: count files
    *count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Regular file
            (*count)++;
        }
    }

    if (*count == 0) {
        closedir(dir);
        *files = NULL;
        return ESP_OK;
    }

    // Allocate array for filenames
    *files = calloc(*count, sizeof(char*));
    if (!*files) {
        closedir(dir);
        return ESP_ERR_NO_MEM;
    }

    // Second pass: store filenames
    rewinddir(dir);
    size_t idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < *count) {
        if (entry->d_type == DT_REG) {
            (*files)[idx] = strdup(entry->d_name);
            if (!(*files)[idx]) {
                // Cleanup on allocation failure
                for (size_t i = 0; i < idx; i++) {
                    free((*files)[i]);
                }
                free(*files);
                closedir(dir);
                return ESP_ERR_NO_MEM;
            }
            idx++;
        }
    }

    closedir(dir);
    return ESP_OK;
}

FILE* sdcard_open_file(const char *filepath)
{
    if (!initialized || !filepath) {
        return NULL;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
    }
    return file;
}

void sdcard_close_file(FILE *file)
{
    if (file) {
        fclose(file);
    }
}

bool sdcard_is_initialized(void)
{
    return initialized;
}

esp_err_t sdcard_get_info(uint64_t *total_bytes, uint64_t *used_bytes)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!card) {
        return ESP_FAIL;
    }

    // Get total capacity
    if (total_bytes) {
        *total_bytes = ((uint64_t)card->csd.capacity) * card->csd.sector_size;
    }

    // Get used space (requires filesystem info)
    if (used_bytes) {
        FATFS *fs;
        DWORD fre_clust;
        if (f_getfree("0:", &fre_clust, &fs) == FR_OK) {
            uint64_t total_sectors = (fs->n_fatent - 2) * fs->csize;
            uint64_t free_sectors = fre_clust * fs->csize;
            *used_bytes = (total_sectors - free_sectors) * fs->ssize;
        } else {
            *used_bytes = 0;
        }
    }

    return ESP_OK;
}
