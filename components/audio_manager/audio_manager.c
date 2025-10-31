/**
 * @file audio_manager.c
 * @brief I2S Audio Manager Implementation
 */

#include "audio_manager.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "external_flash.h"
#include <string.h>
#include <math.h>
#include <inttypes.h>

static const char *TAG = "audio_mgr";

// Driver state
static i2s_chan_handle_t tx_handle = NULL;
static bool initialized = false;
static uint8_t current_volume = 100;  // 100% by default
static uint32_t total_samples_played = 0;
static uint32_t buffer_underruns = 0;

/**
 * @brief Apply software volume scaling to PCM data
 */
static void apply_volume(int16_t *samples, size_t count)
{
    if (current_volume == 100) {
        return;  // No scaling needed
    }

    if (current_volume == 0) {
        // Mute
        memset(samples, 0, count * sizeof(int16_t));
        return;
    }

    // Scale each sample
    for (size_t i = 0; i < count; i++) {
        int32_t scaled = ((int32_t)samples[i] * current_volume) / 100;
        // Clamp to prevent overflow
        if (scaled > 32767) scaled = 32767;
        if (scaled < -32768) scaled = -32768;
        samples[i] = (int16_t)scaled;
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t audio_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2S audio...");

    // Initialize amplifier SD (shutdown) pin if configured
    if (I2S_GPIO_SD >= 0) {
        ESP_LOGI(TAG, "Configuring amplifier SD pin (GPIO %d)", I2S_GPIO_SD);
        gpio_config_t sd_cfg = {
            .pin_bit_mask = (1ULL << I2S_GPIO_SD),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&sd_cfg);
        gpio_set_level(I2S_GPIO_SD, 1);  // Enable amplifier (HIGH = ON)
        ESP_LOGI(TAG, "✅ Amplifier enabled via GPIO %d", I2S_GPIO_SD);
    } else {
        ESP_LOGI(TAG, "Amplifier SD pin hardwired to VIN (always on)");
    }

    // Configure I2S channel with WiFi-safe settings
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = AUDIO_DMA_BUF_LEN;
    chan_cfg.auto_clear = true;  // Auto-clear DMA buffers (prevents data carryover)

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure I2S standard mode (Philips format)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_GPIO_BCLK,
            .ws = I2S_GPIO_LRCLK,
            .dout = I2S_GPIO_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        return ret;
    }

    // Enable the I2S channel
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        return ret;
    }

    initialized = true;
    total_samples_played = 0;
    buffer_underruns = 0;

    ESP_LOGI(TAG, "✅ I2S audio initialized successfully");
    ESP_LOGI(TAG, "   Sample Rate: %d Hz", AUDIO_SAMPLE_RATE);
    ESP_LOGI(TAG, "   Bits/Sample: %d-bit", AUDIO_BITS_PER_SAMPLE);
    ESP_LOGI(TAG, "   Channels: Mono");
    ESP_LOGI(TAG, "   DMA Buffers: %d x %d samples", AUDIO_DMA_BUF_COUNT, AUDIO_DMA_BUF_LEN);
    ESP_LOGI(TAG, "   GPIO - DOUT: %d, BCLK: %d, LRCLK: %d",
             I2S_GPIO_DOUT, I2S_GPIO_BCLK, I2S_GPIO_LRCLK);

    return ESP_OK;
}

esp_err_t audio_manager_deinit(void)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing I2S audio...");

    // Disable and delete channel
    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
    initialized = false;

    // Disable amplifier if using GPIO control
    if (I2S_GPIO_SD >= 0) {
        gpio_set_level(I2S_GPIO_SD, 0);  // Disable amplifier (LOW = OFF)
        ESP_LOGI(TAG, "Amplifier disabled via GPIO %d", I2S_GPIO_SD);
    }

    ESP_LOGI(TAG, "✅ I2S audio deinitialized");
    return ESP_OK;
}

esp_err_t audio_play_pcm(const int16_t *pcm_data, size_t sample_count)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Audio not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (pcm_data == NULL || sample_count == 0) {
        ESP_LOGE(TAG, "Invalid PCM data or sample count");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Playing %zu samples (%zu bytes)", sample_count, sample_count * 2);

    // Allocate temporary buffer for volume-scaled data
    int16_t *scaled_data = malloc(sample_count * sizeof(int16_t));
    if (scaled_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for volume scaling");
        return ESP_ERR_NO_MEM;
    }

    // Copy and apply volume
    memcpy(scaled_data, pcm_data, sample_count * sizeof(int16_t));
    apply_volume(scaled_data, sample_count);

    // Write to I2S (no WiFi power manipulation - use default like Chimes_System)
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle, scaled_data,
                                      sample_count * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY);

    free(scaled_data);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (bytes_written != sample_count * sizeof(int16_t)) {
        ESP_LOGW(TAG, "Incomplete write: %zu/%zu bytes", bytes_written, sample_count * 2);
    }

    total_samples_played += bytes_written / sizeof(int16_t);
    ESP_LOGI(TAG, "✅ Audio written to I2S (%zu bytes)", bytes_written);

    return ESP_OK;
}

esp_err_t audio_play_from_flash(uint32_t flash_addr, size_t sample_count)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Audio not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (sample_count == 0) {
        ESP_LOGE(TAG, "Invalid sample count");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Playing %zu samples from flash @ 0x%06lX", sample_count, (unsigned long)flash_addr);

    // Stream audio in chunks to save RAM
    const size_t chunk_samples = 512;  // 512 samples = 1KB per chunk
    int16_t *chunk_buffer = malloc(chunk_samples * sizeof(int16_t));

    if (chunk_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate chunk buffer");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    size_t samples_remaining = sample_count;
    uint32_t current_addr = flash_addr;

    while (samples_remaining > 0 && ret == ESP_OK) {
        // Read chunk from flash
        size_t samples_to_read = (samples_remaining > chunk_samples) ? chunk_samples : samples_remaining;
        size_t bytes_to_read = samples_to_read * sizeof(int16_t);

        // Read from external flash
        ret = external_flash_read(current_addr, chunk_buffer, bytes_to_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Flash read failed at 0x%06lX", (unsigned long)current_addr);
            break;
        }

        // Apply volume scaling
        apply_volume(chunk_buffer, samples_to_read);

        // Write to I2S
        size_t bytes_written = 0;
        ret = i2s_channel_write(tx_handle, chunk_buffer, bytes_to_read, &bytes_written, portMAX_DELAY);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
            break;
        }

        if (bytes_written != bytes_to_read) {
            ESP_LOGW(TAG, "Incomplete write: %zu/%zu bytes", bytes_written, bytes_to_read);
        }

        // Update counters
        total_samples_played += bytes_written / sizeof(int16_t);
        samples_remaining -= samples_to_read;
        current_addr += bytes_to_read;
    }

    free(chunk_buffer);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Flash playback complete (%zu samples)", sample_count - samples_remaining);

        // Write silence to clear DMA buffers
        const size_t silence_samples = AUDIO_DMA_BUF_COUNT * AUDIO_DMA_BUF_LEN;
        int16_t *silence = calloc(silence_samples, sizeof(int16_t));
        if (silence != NULL) {
            size_t bytes_written = 0;
            i2s_channel_write(tx_handle, silence, silence_samples * sizeof(int16_t),
                            &bytes_written, portMAX_DELAY);
            free(silence);
        }
    } else {
        ESP_LOGE(TAG, "❌ Flash playback failed");
    }

    return ret;
}

esp_err_t audio_play_from_file(const char *filepath)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Audio not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (filepath == NULL) {
        ESP_LOGE(TAG, "Invalid filepath");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Playing audio from file: %s", filepath);

    // Open file
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        ESP_LOGE(TAG, "Invalid file size: %ld", file_size);
        fclose(f);
        return ESP_FAIL;
    }

    size_t total_samples = file_size / sizeof(int16_t);
    ESP_LOGI(TAG, "File size: %ld bytes (%zu samples)", file_size, total_samples);

    // Stream audio in chunks
    const size_t chunk_samples = 512;  // 512 samples = 1KB per chunk
    int16_t *chunk_buffer = malloc(chunk_samples * sizeof(int16_t));

    if (chunk_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate chunk buffer");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    size_t samples_remaining = total_samples;
    size_t total_bytes_written = 0;

    while (samples_remaining > 0 && ret == ESP_OK) {
        size_t samples_to_read = (samples_remaining < chunk_samples) ? samples_remaining : chunk_samples;

        // Read chunk from file
        size_t samples_read = fread(chunk_buffer, sizeof(int16_t), samples_to_read, f);
        if (samples_read != samples_to_read) {
            ESP_LOGE(TAG, "File read error: expected %zu samples, got %zu", samples_to_read, samples_read);
            ret = ESP_FAIL;
            break;
        }

        // Apply volume scaling
        apply_volume(chunk_buffer, samples_read);

        // Write to I2S
        size_t bytes_written = 0;
        esp_err_t write_ret = i2s_channel_write(tx_handle, chunk_buffer,
                                                samples_read * sizeof(int16_t),
                                                &bytes_written, portMAX_DELAY);

        if (write_ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(write_ret));
            ret = write_ret;
            break;
        }

        if (bytes_written != samples_read * sizeof(int16_t)) {
            ESP_LOGW(TAG, "Incomplete chunk write: %zu/%zu bytes", bytes_written, samples_read * 2);
            buffer_underruns++;
        }

        total_bytes_written += bytes_written;
        total_samples_played += bytes_written / sizeof(int16_t);
        samples_remaining -= samples_read;
    }

    free(chunk_buffer);
    fclose(f);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ File playback complete (%zu samples, %zu bytes)", total_samples, total_bytes_written);

        // Write silence to clear DMA buffers
        const size_t silence_samples = AUDIO_DMA_BUF_COUNT * AUDIO_DMA_BUF_LEN;
        int16_t *silence = calloc(silence_samples, sizeof(int16_t));
        if (silence != NULL) {
            size_t bytes_written = 0;
            i2s_channel_write(tx_handle, silence, silence_samples * sizeof(int16_t),
                            &bytes_written, portMAX_DELAY);
            free(silence);
        }
    } else {
        ESP_LOGE(TAG, "❌ File playback failed");
    }

    return ret;
}

esp_err_t audio_play_test_tone(void)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Audio not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Generating 440Hz test tone (2 seconds)...");

    // Generate and play tone in chunks to save memory
    const uint32_t duration_sec = 2;
    const float frequency = 440.0f;  // Musical note A4
    const float amplitude = 0.5f;    // 50% amplitude to avoid clipping
    const uint32_t chunk_samples = 1024;  // 1024 samples = 64ms chunks at 16kHz
    const uint32_t total_samples = AUDIO_SAMPLE_RATE * duration_sec;
    const uint32_t num_chunks = (total_samples + chunk_samples - 1) / chunk_samples;

    int16_t *chunk_buffer = malloc(chunk_samples * sizeof(int16_t));
    if (chunk_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate tone buffer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Playing 440Hz tone in %" PRIu32 " chunks...", num_chunks);

    // Generate and play each chunk
    esp_err_t ret = ESP_OK;
    for (uint32_t chunk = 0; chunk < num_chunks && ret == ESP_OK; chunk++) {
        uint32_t offset = chunk * chunk_samples;
        uint32_t samples_in_chunk = (offset + chunk_samples <= total_samples) ?
                                     chunk_samples : (total_samples - offset);

        // Generate sine wave for this chunk
        for (uint32_t i = 0; i < samples_in_chunk; i++) {
            float t = (float)(offset + i) / AUDIO_SAMPLE_RATE;
            float sample = amplitude * sinf(2.0f * M_PI * frequency * t);
            chunk_buffer[i] = (int16_t)(sample * 32767.0f);
        }

        // Play this chunk
        ret = audio_play_pcm(chunk_buffer, samples_in_chunk);
    }

    free(chunk_buffer);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Test tone playback started");

        // Write silence to clear DMA buffers and prevent noise
        const size_t silence_samples = AUDIO_DMA_BUF_COUNT * AUDIO_DMA_BUF_LEN;
        int16_t *silence = calloc(silence_samples, sizeof(int16_t));  // All zeros
        if (silence != NULL) {
            size_t bytes_written = 0;
            i2s_channel_write(tx_handle, silence, silence_samples * sizeof(int16_t),
                            &bytes_written, portMAX_DELAY);
            free(silence);
            ESP_LOGD(TAG, "Written %zu bytes of silence to clear buffers", bytes_written);
        }
    }

    return ret;
}

esp_err_t audio_stop(void)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Audio not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping audio playback...");

    // Disable and re-enable to clear buffers
    esp_err_t ret = i2s_channel_disable(tx_handle);
    if (ret == ESP_OK) {
        ret = i2s_channel_enable(tx_handle);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Audio stopped");
    }

    return ret;
}

esp_err_t audio_wait_until_done(uint32_t timeout_ms)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Calculate time needed for DMA buffers to drain
    // Add extra time for the silence buffer we wrote
    uint32_t wait_time_ms = ((AUDIO_DMA_BUF_COUNT * AUDIO_DMA_BUF_LEN * 2 * 1000) / AUDIO_SAMPLE_RATE) + 100;

    if (timeout_ms > 0 && wait_time_ms > timeout_ms) {
        wait_time_ms = timeout_ms;
    }

    ESP_LOGD(TAG, "Waiting %lu ms for DMA to finish...", (unsigned long)wait_time_ms);
    vTaskDelay(pdMS_TO_TICKS(wait_time_ms));

    return ESP_OK;
}

bool audio_is_playing(void)
{
    // Simple implementation - would need DMA state tracking for accurate result
    return initialized;
}

esp_err_t audio_set_volume(uint8_t volume_percent)
{
    if (volume_percent > 100) {
        ESP_LOGE(TAG, "Invalid volume: %d (must be 0-100)", volume_percent);
        return ESP_ERR_INVALID_ARG;
    }

    current_volume = volume_percent;
    ESP_LOGI(TAG, "Volume set to %d%%", current_volume);
    return ESP_OK;
}

uint8_t audio_get_volume(void)
{
    return current_volume;
}

esp_err_t audio_get_stats(uint32_t *samples_played, uint32_t *underruns)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (samples_played) {
        *samples_played = total_samples_played;
    }

    if (underruns) {
        *underruns = buffer_underruns;
    }

    return ESP_OK;
}
