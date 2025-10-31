idf_component_register(
    SRCS "audio_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2s external_flash
)
