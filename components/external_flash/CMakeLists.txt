idf_component_register(
    SRCS "external_flash_test.c" "external_flash.c"
    INCLUDE_DIRS "include"
    REQUIRES driver spi_flash esp_common esp_partition freertos
)
