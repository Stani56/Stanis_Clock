/**
 * @file filesystem_manager.h
 * @brief File System Manager for Westminster Chime System
 *
 * Provides high-level API for managing audio files on external flash using LittleFS.
 * Handles file operations (list, read, write, delete) and filesystem mounting.
 *
 * Usage:
 * 1. Call filesystem_manager_init() at startup
 * 2. Use standard POSIX functions (fopen, fread, fwrite) for file I/O
 * 3. Use helper functions for listing, deleting, and checking files
 * 4. Call filesystem_manager_deinit() before shutdown (optional)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of files that can be listed at once
 */
#define FILESYSTEM_MAX_FILES 32

/**
 * @brief Maximum filename length (including path)
 */
#define FILESYSTEM_MAX_FILENAME 256

/**
 * @brief Mount point for the filesystem
 */
#define FILESYSTEM_MOUNT_POINT "/storage"

/**
 * @brief Chimes directory path
 */
#define FILESYSTEM_CHIMES_DIR "/storage/chimes"

/**
 * @brief Configuration directory path
 */
#define FILESYSTEM_CONFIG_DIR "/storage/config"

/**
 * @brief File information structure
 */
typedef struct {
    char name[FILESYSTEM_MAX_FILENAME];  // File name (without path)
    char path[FILESYSTEM_MAX_FILENAME];  // Full path
    size_t size;                          // File size in bytes
    bool is_directory;                    // true if directory
} filesystem_file_info_t;

/**
 * @brief Initialize the file system manager
 *
 * Mounts LittleFS on external flash at /storage.
 * Creates default directories (/storage/chimes, /storage/config).
 * If filesystem is not formatted, it will be formatted automatically.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Already initialized
 *      - ESP_ERR_NO_MEM: Not enough memory
 *      - ESP_FAIL: Mount or format failed
 */
esp_err_t filesystem_manager_init(void);

/**
 * @brief Deinitialize the file system manager
 *
 * Unmounts filesystem and frees resources.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t filesystem_manager_deinit(void);

/**
 * @brief Check if filesystem is initialized
 *
 * @return
 *      - true: Initialized and mounted
 *      - false: Not initialized
 */
bool filesystem_is_initialized(void);

/**
 * @brief List files in a directory
 *
 * Returns information about all files and subdirectories in the specified path.
 *
 * @param path Directory path (e.g., "/storage/chimes")
 * @param[out] file_list Array to store file information
 * @param max_files Maximum number of files to list
 * @param[out] count Number of files found
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer or invalid path
 *      - ESP_ERR_NOT_FOUND: Directory does not exist
 *      - ESP_FAIL: Failed to read directory
 */
esp_err_t filesystem_list_files(const char *path,
                                 filesystem_file_info_t *file_list,
                                 size_t max_files,
                                 size_t *count);

/**
 * @brief Delete a file
 *
 * @param filepath Full path to file (e.g., "/storage/chimes/test.pcm")
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer or invalid path
 *      - ESP_ERR_NOT_FOUND: File does not exist
 *      - ESP_FAIL: Failed to delete
 */
esp_err_t filesystem_delete_file(const char *filepath);

/**
 * @brief Rename a file
 *
 * @param old_path Current file path
 * @param new_path New file path
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer or invalid path
 *      - ESP_ERR_NOT_FOUND: Source file does not exist
 *      - ESP_FAIL: Failed to rename
 */
esp_err_t filesystem_rename_file(const char *old_path, const char *new_path);

/**
 * @brief Check if a file exists
 *
 * @param filepath Full path to file
 *
 * @return
 *      - true: File exists
 *      - false: File does not exist or filesystem not initialized
 */
bool filesystem_file_exists(const char *filepath);

/**
 * @brief Get file size
 *
 * @param filepath Full path to file
 * @param[out] size File size in bytes
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer
 *      - ESP_ERR_NOT_FOUND: File does not exist
 *      - ESP_FAIL: Failed to get file info
 */
esp_err_t filesystem_get_file_size(const char *filepath, size_t *size);

/**
 * @brief Get filesystem usage statistics
 *
 * Returns total and used space on the filesystem.
 *
 * @param[out] total_bytes Total filesystem size in bytes
 * @param[out] used_bytes Used space in bytes
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer
 *      - ESP_FAIL: Failed to get usage info
 */
esp_err_t filesystem_get_usage(size_t *total_bytes, size_t *used_bytes);

/**
 * @brief Format the filesystem
 *
 * WARNING: This erases all files on the filesystem!
 * Use only for factory reset or recovery.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_FAIL: Format failed
 */
esp_err_t filesystem_format(void);

/**
 * @brief Create a directory
 *
 * Creates a directory and all parent directories if needed.
 *
 * @param path Directory path to create
 *
 * @return
 *      - ESP_OK: Success (directory created or already exists)
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer or invalid path
 *      - ESP_FAIL: Failed to create directory
 */
esp_err_t filesystem_create_directory(const char *path);

/**
 * @brief Read entire file into buffer
 *
 * Convenience function to read a complete file into memory.
 * Caller must free the returned buffer.
 *
 * @param filepath Full path to file
 * @param[out] buffer Pointer to buffer (allocated by function)
 * @param[out] size Size of file/buffer in bytes
 *
 * @return
 *      - ESP_OK: Success (buffer allocated and filled)
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer
 *      - ESP_ERR_NOT_FOUND: File does not exist
 *      - ESP_ERR_NO_MEM: Not enough memory
 *      - ESP_FAIL: Read failed
 */
esp_err_t filesystem_read_file(const char *filepath, uint8_t **buffer, size_t *size);

/**
 * @brief Write buffer to file
 *
 * Convenience function to write a complete buffer to a file.
 * Creates parent directories if needed.
 *
 * @param filepath Full path to file
 * @param buffer Data to write
 * @param size Size of data in bytes
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Filesystem not initialized
 *      - ESP_ERR_INVALID_ARG: NULL pointer
 *      - ESP_FAIL: Write failed
 */
esp_err_t filesystem_write_file(const char *filepath, const uint8_t *buffer, size_t size);

#ifdef __cplusplus
}
#endif
