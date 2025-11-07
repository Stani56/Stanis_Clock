idf_component_register(
    SRCS "ota_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_https_ota
        esp_http_client
        app_update
        bootloader_support
        nvs_flash
        json
        wifi_manager
        mqtt_manager
        i2c_devices
)

# Embed GitHub root CA certificate for HTTPS
target_add_binary_data(${COMPONENT_LIB} "certs/github_root_ca.pem" TEXT)
