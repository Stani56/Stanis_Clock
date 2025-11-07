idf_component_register(SRCS "mqtt_manager.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "../../main" "../transition_manager/include"
                    REQUIRES "mqtt" "json" "nvs_flash" "esp_wifi" "transition_manager" "brightness_config" "mqtt_discovery" "i2c_devices" "light_sensor" "adc_manager" "ntp_manager" "mqtt_schema_validator" "mqtt_command_processor" "mqtt_message_persistence" "audio_manager" "filesystem_manager" "chime_manager")