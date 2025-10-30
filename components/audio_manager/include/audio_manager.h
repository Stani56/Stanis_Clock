/**
 * @file audio_manager.h
 * @brief I2S Audio Manager for ESP32 Chime System
 *
 * Manages I2S audio output to MAX98357A amplifier.
 * Supports 16kHz, 16-bit mono PCM audio playback with DMA.
 *
 * Hardware Connections (ESP32-PICO-KIT v4.1 → MAX98357A):
 * - GPIO 32 → DIN (I2S Data)
 * - GPIO 33 → BCLK (Bit Clock)
 * - GPIO 27 → LRC (Left/Right Clock / Word Select)
 * - 3.3V → VIN (Power)
 * - GND → GND
 * - 3.3V → SD (Shutdown - always on)
 * - GAIN → Float (12dB gain, or GND for 9dB, or VIN for 15dB)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio Configuration
 */
#define AUDIO_SAMPLE_RATE       16000  // 16 kHz
#define AUDIO_BITS_PER_SAMPLE   16     // 16-bit
#define AUDIO_CHANNELS          1      // Mono
#define AUDIO_DMA_BUF_COUNT     8      // Number of DMA buffers
#define AUDIO_DMA_BUF_LEN       256    // Samples per DMA buffer

/**
 * @brief GPIO Pin Definitions for I2S
 */
#define I2S_GPIO_DOUT           32     // I2S Data Out (DIN on MAX98357A)
#define I2S_GPIO_BCLK           33     // I2S Bit Clock
#define I2S_GPIO_LRCLK          27     // I2S Left/Right Clock (Word Select)

/**
 * @brief MAX98357A Amplifier Control
 *
 * SD (Shutdown) pin control:
 * - If connected to VIN (always on), set to -1
 * - If connected to GPIO, set GPIO number
 */
#define I2S_GPIO_SD             -1     // SD pin: -1=hardwired to VIN, or GPIO number

/**
 * @brief Initialize I2S audio subsystem
 *
 * Configures and starts the I2S peripheral for audio output.
 * - Sample Rate: 16 kHz
 * - Bit Depth: 16-bit
 * - Channels: Mono (left channel)
 * - DMA: 8 buffers × 256 samples
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Already initialized
 *      - ESP_ERR_NO_MEM: Insufficient memory for DMA buffers
 *      - ESP_FAIL: I2S driver installation failed
 */
esp_err_t audio_manager_init(void);

/**
 * @brief Deinitialize I2S audio subsystem
 *
 * Stops I2S and frees resources.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t audio_manager_deinit(void);

/**
 * @brief Play PCM audio from memory buffer
 *
 * Plays 16-bit mono PCM audio at 16kHz sample rate.
 * This function blocks until all audio is written to I2S DMA buffers.
 *
 * Note: Actual playback continues in background via DMA after function returns.
 * Use audio_wait_until_done() to wait for playback completion.
 *
 * @param pcm_data Pointer to PCM audio data (16-bit signed samples)
 * @param sample_count Number of samples to play
 *
 * @return
 *      - ESP_OK: Success (data written to DMA buffers)
 *      - ESP_ERR_INVALID_STATE: Audio not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid data pointer or sample count
 *      - ESP_FAIL: I2S write failed
 */
esp_err_t audio_play_pcm(const int16_t *pcm_data, size_t sample_count);

/**
 * @brief Play PCM audio from external flash memory
 *
 * Streams audio data from external flash to I2S in chunks.
 * More memory-efficient than loading entire audio into RAM.
 *
 * @param flash_addr Starting address in external flash
 * @param sample_count Number of samples to play
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Audio not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid address or sample count
 *      - ESP_FAIL: Flash read or I2S write failed
 */
esp_err_t audio_play_from_flash(uint32_t flash_addr, size_t sample_count);

/**
 * @brief Play PCM audio from filesystem
 *
 * Streams audio data from filesystem to I2S in chunks.
 * Memory-efficient - loads and plays in small chunks instead of entire file.
 *
 * @param filepath Path to PCM file in filesystem (e.g., "/storage/chimes/quarter.pcm")
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Audio not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid filepath
 *      - ESP_ERR_NOT_FOUND: File not found
 *      - ESP_FAIL: File read or I2S write failed
 */
esp_err_t audio_play_from_file(const char *filepath);

/**
 * @brief Generate and play a test tone
 *
 * Generates a 440Hz sine wave (musical note A4) for 2 seconds.
 * Useful for testing I2S audio output and speaker connection.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Audio not initialized
 *      - ESP_ERR_NO_MEM: Failed to allocate tone buffer
 *      - ESP_FAIL: Playback failed
 */
esp_err_t audio_play_test_tone(void);

/**
 * @brief Stop audio playback
 *
 * Stops current audio playback and clears I2S buffers.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Audio not initialized
 */
esp_err_t audio_stop(void);

/**
 * @brief Wait until current audio playback completes
 *
 * Blocks until all audio data in DMA buffers has been transmitted.
 * Use this after audio_play_pcm() to ensure audio finishes before continuing.
 *
 * @param timeout_ms Maximum time to wait in milliseconds (0 = wait forever)
 *
 * @return
 *      - ESP_OK: Playback completed
 *      - ESP_ERR_TIMEOUT: Timeout reached
 *      - ESP_ERR_INVALID_STATE: Audio not initialized
 */
esp_err_t audio_wait_until_done(uint32_t timeout_ms);

/**
 * @brief Check if audio is currently playing
 *
 * @return
 *      - true: Audio is playing (DMA buffers not empty)
 *      - false: Audio is idle or not initialized
 */
bool audio_is_playing(void);

/**
 * @brief Set audio volume (software scaling)
 *
 * Scales PCM samples by the specified percentage.
 * Note: This is SOFTWARE volume control. Hardware gain is set via GAIN pin.
 *
 * @param volume_percent Volume level (0-100%)
 *                       0 = mute, 100 = full scale
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Volume out of range
 */
esp_err_t audio_set_volume(uint8_t volume_percent);

/**
 * @brief Get current audio volume
 *
 * @return Current volume level (0-100%)
 */
uint8_t audio_get_volume(void);

/**
 * @brief Get I2S driver statistics
 *
 * Returns information about DMA buffer usage, underruns, etc.
 *
 * @param[out] samples_played Total samples played since init
 * @param[out] buffer_underruns Number of DMA buffer underruns (should be 0)
 *
 * @return ESP_OK on success
 */
esp_err_t audio_get_stats(uint32_t *samples_played, uint32_t *buffer_underruns);

#ifdef __cplusplus
}
#endif
