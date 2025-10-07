idf_component_register(SRCS "mqtt_discovery.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "mqtt" "json" "esp_system" "mqtt_manager")