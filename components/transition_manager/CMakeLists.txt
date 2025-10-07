idf_component_register(SRCS "transition_manager.c" "animation_curves.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "freertos" "i2c_devices")