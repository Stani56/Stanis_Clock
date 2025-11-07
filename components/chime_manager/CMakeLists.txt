idf_component_register(
    SRCS "chime_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash audio_manager sdcard_manager nvs_manager
)
