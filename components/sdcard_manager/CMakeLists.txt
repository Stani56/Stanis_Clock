idf_component_register(
    SRCS "sdcard_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs sdmmc driver
)
