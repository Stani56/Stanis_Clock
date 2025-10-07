#ifndef TRANSITION_MANAGER_H
#define TRANSITION_MANAGER_H

#include "transition_types.h"
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the transition manager system
 * 
 * Sets up data structures, timers, and default configuration.
 * GUARANTEED FALLBACK: If initialization fails, system operates in instant mode.
 * 
 * @return ESP_OK on success, error code on failure (fallback mode enabled)
 */
esp_err_t transition_manager_init(void);

/**
 * @brief Deinitialize transition manager and cleanup resources
 */
void transition_manager_deinit(void);

/**
 * @brief Start the animation task for smooth transitions
 * 
 * GUARANTEED FALLBACK: If task creation fails, transitions are disabled.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t transition_manager_start(void);

/**
 * @brief Stop the animation task
 */
void transition_manager_stop(void);

/**
 * @brief Request a transition for a specific LED
 * 
 * GUARANTEED FALLBACK: If system is in fallback mode or error occurs,
 * LED is updated instantly to target brightness.
 * 
 * @param request Transition parameters
 * @return ESP_OK if transition started, ESP_ERR_* if fallback used
 */
esp_err_t transition_request_led(const transition_request_t *request);

/**
 * @brief Request a transition for multiple LEDs (batch)
 * 
 * GUARANTEED FALLBACK: Any failed transitions fallback to instant updates.
 * 
 * @param requests Array of transition requests
 * @param count Number of requests
 * @return ESP_OK if all succeeded, ESP_ERR_* if some failed
 */
esp_err_t transition_request_batch(const transition_request_t *requests, size_t count);

/**
 * @brief Check if transition system is currently active
 * 
 * @return true if any transitions are running, false otherwise
 */
bool transition_manager_is_active(void);

/**
 * @brief Force all transitions to complete instantly
 * 
 * Emergency function to immediately complete all transitions.
 * Useful for system shutdown or error recovery.
 */
void transition_manager_complete_all(void);

/**
 * @brief Enable or disable transition system
 * 
 * @param enabled true to enable transitions, false for instant mode
 */
void transition_manager_set_enabled(bool enabled);

/**
 * @brief Check if transitions are enabled
 * 
 * @return true if enabled, false if in instant/fallback mode
 */
bool transition_manager_is_enabled(void);

/**
 * @brief Set transition configuration
 * 
 * @param config New configuration settings
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid settings
 */
esp_err_t transition_manager_set_config(const transition_config_t *config);

/**
 * @brief Get current transition configuration
 * 
 * @param config Pointer to store current configuration
 * @return ESP_OK on success
 */
esp_err_t transition_manager_get_config(transition_config_t *config);

/**
 * @brief Check if system is in fallback mode
 * 
 * @return true if in emergency fallback mode (instant updates only)
 */
bool transition_manager_is_fallback_mode(void);

/**
 * @brief Force system into fallback mode
 * 
 * Emergency function to disable all transitions and use instant updates.
 * Can be called from error handlers or watchdog functions.
 */
void transition_manager_enable_fallback_mode(void);

/**
 * @brief Try to exit fallback mode (if conditions allow)
 * 
 * @return ESP_OK if successfully exited fallback, ESP_ERR_* if staying in fallback
 */
esp_err_t transition_manager_try_exit_fallback(void);

/**
 * @brief Get system performance statistics
 * 
 * @param active_transitions Number of currently active transitions
 * @param update_rate Current animation update rate (Hz)
 * @param cpu_usage Estimated CPU usage percentage (0-100)
 * @return ESP_OK on success
 */
esp_err_t transition_manager_get_stats(uint8_t *active_transitions, 
                                      float *update_rate, 
                                      uint8_t *cpu_usage);

#ifdef __cplusplus
}
#endif

#endif // TRANSITION_MANAGER_H