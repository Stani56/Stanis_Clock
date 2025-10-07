idf_component_register(SRCS "wordclock_display.c"
                       INCLUDE_DIRS "."
                       REQUIRES "i2c_devices" "wordclock_time")