idf_component_register(
    SRCS "filesystem_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES joltwallet__littlefs external_flash spi_flash
)
