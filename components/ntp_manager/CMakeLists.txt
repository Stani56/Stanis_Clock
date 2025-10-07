idf_component_register(SRCS "ntp_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "lwip" "wifi_manager" "i2c_devices")