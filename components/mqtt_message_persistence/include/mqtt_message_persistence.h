#ifndef MQTT_MESSAGE_PERSISTENCE_H
#define MQTT_MESSAGE_PERSISTENCE_H

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQTT Message Persistence - Reliable Message Delivery System
 * 
 * This component provides persistent message queuing with automatic retry,
 * priority handling, and failure recovery for reliable MQTT communication.
 */

// Version information
#define MQTT_MESSAGE_PERSISTENCE_VERSION_MAJOR 1
#define MQTT_MESSAGE_PERSISTENCE_VERSION_MINOR 0
#define MQTT_MESSAGE_PERSISTENCE_VERSION_PATCH 0

// Configuration limits
#define MQTT_MESSAGE_MAX_QUEUED 64            ///< Maximum messages in queue
#define MQTT_MESSAGE_MAX_TOPIC_LEN 128        ///< Maximum topic length
#define MQTT_MESSAGE_MAX_PAYLOAD_LEN 1024     ///< Maximum payload length
#define MQTT_MESSAGE_MAX_RETRY_COUNT 5        ///< Maximum retry attempts
#define MQTT_MESSAGE_RETRY_DELAY_MS 1000      ///< Base retry delay in milliseconds
#define MQTT_MESSAGE_QUEUE_TIMEOUT_MS 5000    ///< Queue operation timeout

/**
 * @brief Message priority levels
 */
typedef enum {
    MQTT_MESSAGE_PRIORITY_LOW = 0,       ///< Low priority (status updates, heartbeat)
    MQTT_MESSAGE_PRIORITY_NORMAL = 1,    ///< Normal priority (regular commands)
    MQTT_MESSAGE_PRIORITY_HIGH = 2,      ///< High priority (critical commands)
    MQTT_MESSAGE_PRIORITY_URGENT = 3     ///< Urgent priority (emergency, alerts)
} mqtt_message_priority_t;

/**
 * @brief Message delivery result
 */
typedef enum {
    MQTT_MESSAGE_RESULT_SUCCESS = 0,     ///< Message delivered successfully
    MQTT_MESSAGE_RESULT_FAILED,          ///< Message delivery failed
    MQTT_MESSAGE_RESULT_RETRY_EXCEEDED,  ///< Maximum retries exceeded
    MQTT_MESSAGE_RESULT_QUEUE_FULL,      ///< Message queue is full
    MQTT_MESSAGE_RESULT_INVALID_PARAMS,  ///< Invalid message parameters
    MQTT_MESSAGE_RESULT_SYSTEM_ERROR     ///< System error (memory, mutex, etc.)
} mqtt_message_result_t;

/**
 * @brief Message state
 */
typedef enum {
    MQTT_MESSAGE_STATE_PENDING = 0,     ///< Waiting to be sent
    MQTT_MESSAGE_STATE_SENDING,         ///< Currently being sent
    MQTT_MESSAGE_STATE_DELIVERED,       ///< Successfully delivered
    MQTT_MESSAGE_STATE_FAILED,          ///< Failed after all retries
    MQTT_MESSAGE_STATE_EXPIRED          ///< Message expired (TTL exceeded)
} mqtt_message_state_t;

/**
 * @brief Retry policy configuration
 */
typedef struct {
    uint8_t max_retries;                 ///< Maximum number of retry attempts
    uint32_t base_delay_ms;              ///< Base delay between retries
    float backoff_multiplier;            ///< Exponential backoff multiplier (1.0 = constant)
    uint32_t max_delay_ms;               ///< Maximum delay between retries
    bool exponential_backoff;            ///< Enable exponential backoff
} mqtt_retry_policy_t;

/**
 * @brief Message delivery callback function signature
 * 
 * @param topic Message topic
 * @param payload Message payload
 * @param qos QoS level for delivery
 * @param user_data User-provided callback data
 * @return true if delivery successful, false otherwise
 */
typedef bool (*mqtt_delivery_callback_t)(const char *topic, const char *payload, 
                                        int qos, void *user_data);

/**
 * @brief Persistent message structure
 */
typedef struct {
    uint32_t message_id;                              ///< Unique message identifier
    char topic[MQTT_MESSAGE_MAX_TOPIC_LEN];          ///< MQTT topic
    char payload[MQTT_MESSAGE_MAX_PAYLOAD_LEN];      ///< Message payload
    mqtt_message_priority_t priority;                 ///< Message priority
    mqtt_message_state_t state;                       ///< Current state
    int qos;                                          ///< QoS level (0, 1, 2)
    bool retain;                                      ///< Retain flag
    uint8_t retry_count;                              ///< Number of retry attempts made
    uint32_t created_time_ms;                         ///< Message creation timestamp
    uint32_t last_attempt_ms;                         ///< Last delivery attempt timestamp
    uint32_t next_retry_ms;                           ///< Next retry attempt timestamp
    uint32_t ttl_ms;                                  ///< Time to live (0 = no expiry)
    mqtt_retry_policy_t retry_policy;                 ///< Retry policy for this message
    void *user_data;                                  ///< User-provided data
} mqtt_persistent_message_t;

/**
 * @brief Message persistence statistics
 */
typedef struct {
    uint32_t total_messages_queued;                   ///< Total messages added to queue
    uint32_t messages_delivered;                      ///< Successfully delivered messages
    uint32_t messages_failed;                         ///< Failed messages (after retries)
    uint32_t messages_expired;                        ///< Expired messages (TTL exceeded)
    uint32_t total_retries_attempted;                 ///< Total retry attempts made
    uint32_t queue_full_rejections;                   ///< Messages rejected due to full queue
    uint32_t current_queue_size;                      ///< Current number of queued messages
    uint32_t peak_queue_size;                         ///< Maximum queue size reached
    uint32_t average_delivery_time_ms;                ///< Average time from queue to delivery
    uint32_t last_delivery_time_ms;                   ///< Timestamp of last delivery
} mqtt_persistence_stats_t;

/**
 * @brief Message persistence configuration
 */
typedef struct {
    mqtt_delivery_callback_t delivery_callback;       ///< Function to deliver messages
    void *callback_user_data;                         ///< User data for callback
    mqtt_retry_policy_t default_retry_policy;         ///< Default retry policy
    uint32_t queue_check_interval_ms;                 ///< How often to check queue (task)
    uint32_t message_cleanup_interval_ms;             ///< How often to clean up old messages
    bool auto_cleanup_expired;                        ///< Automatically remove expired messages
    bool priority_based_processing;                   ///< Process higher priority messages first
} mqtt_persistence_config_t;

/**
 * @brief Initialize the message persistence system
 * 
 * @param config Configuration parameters
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_message_persistence_init(const mqtt_persistence_config_t *config);

/**
 * @brief Deinitialize the message persistence system
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_message_persistence_deinit(void);

/**
 * @brief Check if message persistence system is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool mqtt_message_persistence_is_initialized(void);

/**
 * @brief Queue a message for persistent delivery
 * 
 * @param topic MQTT topic
 * @param payload Message payload
 * @param qos QoS level
 * @param retain Retain flag
 * @param priority Message priority
 * @param ttl_ms Time to live in milliseconds (0 = no expiry)
 * @param retry_policy Custom retry policy (NULL for default)
 * @param user_data User data for callback
 * @param message_id Output parameter for assigned message ID
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_message_persistence_queue_message(const char *topic,
                                                const char *payload,
                                                int qos,
                                                bool retain,
                                                mqtt_message_priority_t priority,
                                                uint32_t ttl_ms,
                                                const mqtt_retry_policy_t *retry_policy,
                                                void *user_data,
                                                uint32_t *message_id);

/**
 * @brief Remove a message from the queue by ID
 * 
 * @param message_id Message ID to remove
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if message not found
 */
esp_err_t mqtt_message_persistence_remove_message(uint32_t message_id);

/**
 * @brief Get message information by ID
 * 
 * @param message_id Message ID to look up
 * @param message Output parameter for message information
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if message not found
 */
esp_err_t mqtt_message_persistence_get_message(uint32_t message_id, 
                                              mqtt_persistent_message_t *message);

/**
 * @brief Get list of all queued messages
 * 
 * @param messages Array to store message pointers (must be large enough)
 * @param max_messages Maximum number of messages to return
 * @param actual_count Output parameter for actual number of messages returned
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_message_persistence_get_queue(const mqtt_persistent_message_t **messages,
                                            size_t max_messages,
                                            size_t *actual_count);

/**
 * @brief Clear all messages from the queue
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_message_persistence_clear_queue(void);

/**
 * @brief Get message persistence statistics
 * 
 * @param stats Pointer to stats structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_message_persistence_get_stats(mqtt_persistence_stats_t *stats);

/**
 * @brief Reset message persistence statistics
 * 
 * @return ESP_OK on success
 */
esp_err_t mqtt_message_persistence_reset_stats(void);

/**
 * @brief Update the delivery callback function
 * 
 * @param callback New delivery callback function
 * @param user_data New user data for callback
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_message_persistence_set_delivery_callback(mqtt_delivery_callback_t callback,
                                                       void *user_data);

/**
 * @brief Update the default retry policy
 * 
 * @param policy New default retry policy
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_message_persistence_set_retry_policy(const mqtt_retry_policy_t *policy);

/**
 * @brief Force processing of pending messages (useful for testing)
 * 
 * @return Number of messages processed
 */
uint32_t mqtt_message_persistence_process_queue(void);

/**
 * @brief Create a default retry policy
 * 
 * @param policy Output parameter for default policy
 */
void mqtt_message_persistence_get_default_retry_policy(mqtt_retry_policy_t *policy);

/**
 * @brief Create a high-priority retry policy for critical messages
 * 
 * @param policy Output parameter for high-priority policy
 */
void mqtt_message_persistence_get_critical_retry_policy(mqtt_retry_policy_t *policy);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_MESSAGE_PERSISTENCE_H */