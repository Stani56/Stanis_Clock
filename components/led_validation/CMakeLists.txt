idf_component_register(SRCS "led_validation.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "i2c_devices" "wordclock_time" "nvs_flash" "mqtt_manager" "main")
