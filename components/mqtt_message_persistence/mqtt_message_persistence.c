#include "mqtt_message_persistence.h"
#include <string.h>
#include <math.h>
#include <inttypes.h>

static const char *TAG = "mqtt_message_persistence";

// Global state
static bool g_persistence_initialized = false;
static SemaphoreHandle_t g_persistence_mutex = NULL;
static TaskHandle_t g_processing_task_handle = NULL;
static bool g_processing_task_running = false;
static mqtt_persistence_config_t g_config = {0};
static mqtt_persistence_stats_t g_stats = {0};

// Message queue storage
static mqtt_persistent_message_t g_message_queue[MQTT_MESSAGE_MAX_QUEUED];
static size_t g_queue_size = 0;
static uint32_t g_next_message_id = 1;

// Forward declarations
static void message_processing_task(void *pvParameters);
static esp_err_t process_single_message(mqtt_persistent_message_t *message);
static bool is_message_expired(const mqtt_persistent_message_t *message, uint32_t current_time);
static uint32_t calculate_next_retry_delay(const mqtt_persistent_message_t *message);
// static int compare_message_priority(const void *a, const void *b);  // Unused - commented out
static esp_err_t cleanup_expired_messages(void);
static esp_err_t update_average_delivery_time(uint32_t delivery_time_ms);

/**
 * @brief Initialize the message persistence system
 */
esp_err_t mqtt_message_persistence_init(const mqtt_persistence_config_t *config)
{
    ESP_LOGI(TAG, "=== MQTT MESSAGE PERSISTENCE INIT ===");
    
    if (g_persistence_initialized) {
        ESP_LOGW(TAG, "Message persistence already initialized");
        return ESP_OK;
    }
    
    if (config == NULL || config->delivery_callback == NULL) {
        ESP_LOGE(TAG, "Invalid configuration or missing delivery callback");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create mutex for thread safety
    g_persistence_mutex = xSemaphoreCreateMutex();
    if (g_persistence_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create persistence mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Copy configuration
    memcpy(&g_config, config, sizeof(mqtt_persistence_config_t));
    
    // Initialize statistics
    memset(&g_stats, 0, sizeof(mqtt_persistence_stats_t));
    
    // Initialize message queue
    memset(g_message_queue, 0, sizeof(g_message_queue));
    g_queue_size = 0;
    g_next_message_id = 1;
    
    // Create message processing task
    BaseType_t task_ret = xTaskCreate(
        message_processing_task,
        "mqtt_persistence",
        4096,
        NULL,
        tskIDLE_PRIORITY + 2,
        &g_processing_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create message processing task");
        vSemaphoreDelete(g_persistence_mutex);
        g_persistence_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    g_processing_task_running = true;
    g_persistence_initialized = true;
    
    ESP_LOGI(TAG, "✅ MQTT Message Persistence v%d.%d.%d initialized successfully", 
             MQTT_MESSAGE_PERSISTENCE_VERSION_MAJOR,
             MQTT_MESSAGE_PERSISTENCE_VERSION_MINOR, 
             MQTT_MESSAGE_PERSISTENCE_VERSION_PATCH);
    
    return ESP_OK;
}

/**
 * @brief Deinitialize the message persistence system
 */
esp_err_t mqtt_message_persistence_deinit(void)
{
    ESP_LOGI(TAG, "=== MQTT MESSAGE PERSISTENCE DEINIT ===");
    
    if (!g_persistence_initialized) {
        ESP_LOGW(TAG, "Message persistence not initialized");
        return ESP_OK;
    }
    
    // Stop processing task
    if (g_processing_task_handle != NULL) {
        g_processing_task_running = false;
        vTaskDelete(g_processing_task_handle);
        g_processing_task_handle = NULL;
    }
    
    // Clean up mutex
    if (g_persistence_mutex != NULL) {
        vSemaphoreDelete(g_persistence_mutex);
        g_persistence_mutex = NULL;
    }
    
    // Reset statistics and queue
    memset(&g_stats, 0, sizeof(mqtt_persistence_stats_t));
    memset(&g_config, 0, sizeof(mqtt_persistence_config_t));
    memset(g_message_queue, 0, sizeof(g_message_queue));
    g_queue_size = 0;
    g_next_message_id = 1;
    
    g_persistence_initialized = false;
    
    ESP_LOGI(TAG, "✅ MQTT Message Persistence deinitialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Check if message persistence system is initialized
 */
bool mqtt_message_persistence_is_initialized(void)
{
    return g_persistence_initialized;
}

/**
 * @brief Queue a message for persistent delivery
 */
esp_err_t mqtt_message_persistence_queue_message(const char *topic,
                                                const char *payload,
                                                int qos,
                                                bool retain,
                                                mqtt_message_priority_t priority,
                                                uint32_t ttl_ms,
                                                const mqtt_retry_policy_t *retry_policy,
                                                void *user_data,
                                                uint32_t *message_id)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (topic == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(topic) >= MQTT_MESSAGE_MAX_TOPIC_LEN || 
        strlen(payload) >= MQTT_MESSAGE_MAX_PAYLOAD_LEN) {
        ESP_LOGE(TAG, "Topic or payload too long");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos < 0 || qos > 2) {
        ESP_LOGE(TAG, "Invalid QoS level: %d", qos);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Check if queue is full
    if (g_queue_size >= MQTT_MESSAGE_MAX_QUEUED) {
        g_stats.queue_full_rejections++;
        xSemaphoreGive(g_persistence_mutex);
        ESP_LOGW(TAG, "Message queue is full, rejecting message");
        return ESP_ERR_NO_MEM;
    }
    
    // Find empty slot in queue
    size_t slot_index = 0;
    for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED; i++) {
        if (g_message_queue[i].state == MQTT_MESSAGE_STATE_PENDING && 
            g_message_queue[i].message_id == 0) {
            slot_index = i;
            break;
        }
        if (g_message_queue[i].message_id == 0) {
            slot_index = i;
            break;
        }
    }
    
    // Initialize message
    mqtt_persistent_message_t *msg = &g_message_queue[slot_index];
    memset(msg, 0, sizeof(mqtt_persistent_message_t));
    
    msg->message_id = g_next_message_id++;
    strncpy(msg->topic, topic, sizeof(msg->topic) - 1);
    strncpy(msg->payload, payload, sizeof(msg->payload) - 1);
    msg->priority = priority;
    msg->state = MQTT_MESSAGE_STATE_PENDING;
    msg->qos = qos;
    msg->retain = retain;
    msg->retry_count = 0;
    msg->created_time_ms = esp_log_timestamp();
    msg->last_attempt_ms = 0;
    msg->next_retry_ms = 0;
    msg->ttl_ms = ttl_ms;
    msg->user_data = user_data;
    
    // Set retry policy
    if (retry_policy != NULL) {
        memcpy(&msg->retry_policy, retry_policy, sizeof(mqtt_retry_policy_t));
    } else {
        memcpy(&msg->retry_policy, &g_config.default_retry_policy, sizeof(mqtt_retry_policy_t));
    }
    
    g_queue_size++;
    g_stats.total_messages_queued++;
    g_stats.current_queue_size = g_queue_size;
    
    if (g_queue_size > g_stats.peak_queue_size) {
        g_stats.peak_queue_size = g_queue_size;
    }
    
    // Return message ID if requested
    if (message_id != NULL) {
        *message_id = msg->message_id;
    }
    
    xSemaphoreGive(g_persistence_mutex);
    
    ESP_LOGI(TAG, "Message queued: ID=%" PRIu32 ", topic=%s, priority=%d, qos=%d", 
             msg->message_id, topic, priority, qos);
    
    return ESP_OK;
}

/**
 * @brief Remove a message from the queue by ID
 */
esp_err_t mqtt_message_persistence_remove_message(uint32_t message_id)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find message in queue
    for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED; i++) {
        if (g_message_queue[i].message_id == message_id) {
            // Clear message slot
            memset(&g_message_queue[i], 0, sizeof(mqtt_persistent_message_t));
            g_queue_size--;
            g_stats.current_queue_size = g_queue_size;
            
            xSemaphoreGive(g_persistence_mutex);
            ESP_LOGI(TAG, "Message removed: ID=%" PRIu32, message_id);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(g_persistence_mutex);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Get message information by ID
 */
esp_err_t mqtt_message_persistence_get_message(uint32_t message_id, 
                                              mqtt_persistent_message_t *message)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find message in queue
    for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED; i++) {
        if (g_message_queue[i].message_id == message_id) {
            memcpy(message, &g_message_queue[i], sizeof(mqtt_persistent_message_t));
            xSemaphoreGive(g_persistence_mutex);
            return ESP_OK;
        }
    }
    
    xSemaphoreGive(g_persistence_mutex);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Get list of all queued messages
 */
esp_err_t mqtt_message_persistence_get_queue(const mqtt_persistent_message_t **messages,
                                            size_t max_messages,
                                            size_t *actual_count)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (messages == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    size_t count = 0;
    for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED && count < max_messages; i++) {
        if (g_message_queue[i].message_id != 0) {
            messages[count] = &g_message_queue[i];
            count++;
        }
    }
    
    *actual_count = count;
    
    xSemaphoreGive(g_persistence_mutex);
    return ESP_OK;
}

/**
 * @brief Clear all messages from the queue
 */
esp_err_t mqtt_message_persistence_clear_queue(void)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memset(g_message_queue, 0, sizeof(g_message_queue));
    g_queue_size = 0;
    g_stats.current_queue_size = 0;
    
    xSemaphoreGive(g_persistence_mutex);
    
    ESP_LOGI(TAG, "Message queue cleared");
    return ESP_OK;
}

/**
 * @brief Get message persistence statistics
 */
esp_err_t mqtt_message_persistence_get_stats(mqtt_persistence_stats_t *stats)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(stats, &g_stats, sizeof(mqtt_persistence_stats_t));
    
    xSemaphoreGive(g_persistence_mutex);
    
    return ESP_OK;
}

/**
 * @brief Reset message persistence statistics
 */
esp_err_t mqtt_message_persistence_reset_stats(void)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Reset statistics but keep current queue size and peak
    uint32_t current_queue_size = g_stats.current_queue_size;
    uint32_t peak_queue_size = g_stats.peak_queue_size;
    
    memset(&g_stats, 0, sizeof(mqtt_persistence_stats_t));
    g_stats.current_queue_size = current_queue_size;
    g_stats.peak_queue_size = peak_queue_size;
    
    xSemaphoreGive(g_persistence_mutex);
    
    ESP_LOGI(TAG, "Message persistence statistics reset");
    return ESP_OK;
}

/**
 * @brief Update the delivery callback function
 */
esp_err_t mqtt_message_persistence_set_delivery_callback(mqtt_delivery_callback_t callback,
                                                       void *user_data)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_config.delivery_callback = callback;
    g_config.callback_user_data = user_data;
    
    xSemaphoreGive(g_persistence_mutex);
    
    ESP_LOGI(TAG, "Delivery callback updated");
    return ESP_OK;
}

/**
 * @brief Update the default retry policy
 */
esp_err_t mqtt_message_persistence_set_retry_policy(const mqtt_retry_policy_t *policy)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (policy == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(&g_config.default_retry_policy, policy, sizeof(mqtt_retry_policy_t));
    
    xSemaphoreGive(g_persistence_mutex);
    
    ESP_LOGI(TAG, "Default retry policy updated");
    return ESP_OK;
}

/**
 * @brief Force processing of pending messages (useful for testing)
 */
uint32_t mqtt_message_persistence_process_queue(void)
{
    if (!g_persistence_initialized) {
        return 0;
    }
    
    uint32_t processed_count = 0;
    uint32_t current_time = esp_log_timestamp();
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return 0;
    }
    
    // Process messages based on priority if enabled
    if (g_config.priority_based_processing) {
        // Sort messages by priority (bubble sort for simplicity in embedded context)
        for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED - 1; i++) {
            for (size_t j = 0; j < MQTT_MESSAGE_MAX_QUEUED - i - 1; j++) {
                if (g_message_queue[j].message_id != 0 && g_message_queue[j + 1].message_id != 0) {
                    if (g_message_queue[j].priority < g_message_queue[j + 1].priority) {
                        mqtt_persistent_message_t temp = g_message_queue[j];
                        g_message_queue[j] = g_message_queue[j + 1];
                        g_message_queue[j + 1] = temp;
                    }
                }
            }
        }
    }
    
    // Process all eligible messages
    for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED; i++) {
        mqtt_persistent_message_t *msg = &g_message_queue[i];
        
        if (msg->message_id == 0) continue;
        
        // Check if message is expired
        if (is_message_expired(msg, current_time)) {
            ESP_LOGW(TAG, "Message expired: ID=%" PRIu32, msg->message_id);
            msg->state = MQTT_MESSAGE_STATE_EXPIRED;
            g_stats.messages_expired++;
            memset(msg, 0, sizeof(mqtt_persistent_message_t));
            g_queue_size--;
            g_stats.current_queue_size = g_queue_size;
            processed_count++;
            continue;
        }
        
        // Check if message is ready for (re)delivery
        if (msg->state == MQTT_MESSAGE_STATE_PENDING || 
            (msg->state == MQTT_MESSAGE_STATE_FAILED && 
             current_time >= msg->next_retry_ms && 
             msg->retry_count < msg->retry_policy.max_retries)) {
            
            if (process_single_message(msg) == ESP_OK) {
                processed_count++;
            }
        }
    }
    
    xSemaphoreGive(g_persistence_mutex);
    
    return processed_count;
}

/**
 * @brief Create a default retry policy
 */
void mqtt_message_persistence_get_default_retry_policy(mqtt_retry_policy_t *policy)
{
    if (policy == NULL) return;
    
    policy->max_retries = 3;
    policy->base_delay_ms = 1000;
    policy->backoff_multiplier = 2.0f;
    policy->max_delay_ms = 30000;
    policy->exponential_backoff = true;
}

/**
 * @brief Create a high-priority retry policy for critical messages
 */
void mqtt_message_persistence_get_critical_retry_policy(mqtt_retry_policy_t *policy)
{
    if (policy == NULL) return;
    
    policy->max_retries = 5;
    policy->base_delay_ms = 500;
    policy->backoff_multiplier = 1.5f;
    policy->max_delay_ms = 10000;
    policy->exponential_backoff = true;
}

/**
 * @brief Message processing task
 */
static void message_processing_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Message processing task started");
    
    TickType_t last_cleanup_time = xTaskGetTickCount();
    
    while (g_processing_task_running) {
        // Process message queue
        uint32_t processed = mqtt_message_persistence_process_queue();
        
        if (processed > 0) {
            ESP_LOGD(TAG, "Processed %" PRIu32 " messages", processed);
        }
        
        // Periodic cleanup of expired messages
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_cleanup_time) >= pdMS_TO_TICKS(g_config.message_cleanup_interval_ms)) {
            if (g_config.auto_cleanup_expired) {
                cleanup_expired_messages();
            }
            last_cleanup_time = current_time;
        }
        
        // Sleep for configured interval
        vTaskDelay(pdMS_TO_TICKS(g_config.queue_check_interval_ms));
    }
    
    ESP_LOGI(TAG, "Message processing task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Process a single message
 */
static esp_err_t process_single_message(mqtt_persistent_message_t *message)
{
    if (message == NULL || g_config.delivery_callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint32_t start_time = esp_log_timestamp();
    message->state = MQTT_MESSAGE_STATE_SENDING;
    message->last_attempt_ms = start_time;
    
    ESP_LOGD(TAG, "Attempting delivery: ID=%" PRIu32 ", attempt=%d/%d", 
             message->message_id, message->retry_count + 1, message->retry_policy.max_retries + 1);
    
    // Attempt delivery
    bool delivery_success = g_config.delivery_callback(
        message->topic,
        message->payload,
        message->qos,
        g_config.callback_user_data
    );
    
    uint32_t delivery_time = esp_log_timestamp() - start_time;
    (void)delivery_time;  // TODO: Use delivery_time for statistics
    
    if (delivery_success) {
        ESP_LOGI(TAG, "✅ Message delivered successfully: ID=%" PRIu32, message->message_id);
        message->state = MQTT_MESSAGE_STATE_DELIVERED;
        g_stats.messages_delivered++;
        g_stats.last_delivery_time_ms = esp_log_timestamp();
        
        // Update average delivery time
        update_average_delivery_time(start_time - message->created_time_ms);
        
        // Remove delivered message from queue
        memset(message, 0, sizeof(mqtt_persistent_message_t));
        g_queue_size--;
        g_stats.current_queue_size = g_queue_size;
        
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "❌ Message delivery failed: ID=%" PRIu32, message->message_id);
        message->retry_count++;
        g_stats.total_retries_attempted++;
        
        if (message->retry_count >= message->retry_policy.max_retries) {
            ESP_LOGE(TAG, "Message failed after %d retries: ID=%" PRIu32, 
                     message->retry_count, message->message_id);
            message->state = MQTT_MESSAGE_STATE_FAILED;
            g_stats.messages_failed++;
            
            // Remove failed message from queue
            memset(message, 0, sizeof(mqtt_persistent_message_t));
            g_queue_size--;
            g_stats.current_queue_size = g_queue_size;
        } else {
            // Schedule next retry
            uint32_t delay = calculate_next_retry_delay(message);
            message->next_retry_ms = esp_log_timestamp() + delay;
            message->state = MQTT_MESSAGE_STATE_PENDING;
            
            ESP_LOGI(TAG, "Retry scheduled: ID=%" PRIu32 ", delay=%" PRIu32 "ms", message->message_id, delay);
        }
        
        return ESP_FAIL;
    }
}

/**
 * @brief Check if message is expired
 */
static bool is_message_expired(const mqtt_persistent_message_t *message, uint32_t current_time)
{
    if (message == NULL || message->ttl_ms == 0) {
        return false;
    }
    
    return (current_time - message->created_time_ms) >= message->ttl_ms;
}

/**
 * @brief Calculate next retry delay based on retry policy
 */
static uint32_t calculate_next_retry_delay(const mqtt_persistent_message_t *message)
{
    if (message == NULL) {
        return MQTT_MESSAGE_RETRY_DELAY_MS;
    }
    
    uint32_t delay = message->retry_policy.base_delay_ms;
    
    if (message->retry_policy.exponential_backoff && message->retry_count > 0) {
        // Calculate exponential backoff
        float multiplier = powf(message->retry_policy.backoff_multiplier, message->retry_count);
        delay = (uint32_t)(delay * multiplier);
        
        // Clamp to maximum delay
        if (delay > message->retry_policy.max_delay_ms) {
            delay = message->retry_policy.max_delay_ms;
        }
    }
    
    return delay;
}

/**
 * @brief Cleanup expired messages from queue
 */
static esp_err_t cleanup_expired_messages(void)
{
    if (!g_persistence_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint32_t current_time = esp_log_timestamp();
    uint32_t cleaned_count = 0;
    
    if (xSemaphoreTake(g_persistence_mutex, pdMS_TO_TICKS(MQTT_MESSAGE_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    for (size_t i = 0; i < MQTT_MESSAGE_MAX_QUEUED; i++) {
        mqtt_persistent_message_t *msg = &g_message_queue[i];
        
        if (msg->message_id != 0 && is_message_expired(msg, current_time)) {
            ESP_LOGD(TAG, "Cleaning up expired message: ID=%" PRIu32, msg->message_id);
            msg->state = MQTT_MESSAGE_STATE_EXPIRED;
            g_stats.messages_expired++;
            memset(msg, 0, sizeof(mqtt_persistent_message_t));
            g_queue_size--;
            cleaned_count++;
        }
    }
    
    g_stats.current_queue_size = g_queue_size;
    
    xSemaphoreGive(g_persistence_mutex);
    
    if (cleaned_count > 0) {
        ESP_LOGI(TAG, "Cleaned up %" PRIu32 " expired messages", cleaned_count);
    }
    
    return ESP_OK;
}

/**
 * @brief Update average delivery time statistic
 */
static esp_err_t update_average_delivery_time(uint32_t delivery_time_ms)
{
    // Simple moving average calculation
    if (g_stats.messages_delivered == 1) {
        g_stats.average_delivery_time_ms = delivery_time_ms;
    } else {
        // Weighted average: 80% old average, 20% new sample
        g_stats.average_delivery_time_ms = (uint32_t)(
            0.8f * g_stats.average_delivery_time_ms + 0.2f * delivery_time_ms
        );
    }
    
    return ESP_OK;
}