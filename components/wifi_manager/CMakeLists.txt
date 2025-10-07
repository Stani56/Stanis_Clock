idf_component_register(SRCS "wifi_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "esp_wifi" "esp_netif" "esp_event" "freertos" "nvs_manager" "status_led_manager" "ntp_manager" "mqtt_manager")