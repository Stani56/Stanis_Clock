idf_component_register(SRCS "light_sensor.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "i2c_devices" "freertos" "brightness_config")