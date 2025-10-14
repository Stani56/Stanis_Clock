idf_component_register(
    SRCS "error_log_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_timer
)
