idf_component_register(
    SRCS "mqtt_command_processor.c"
    INCLUDE_DIRS "include"
    REQUIRES "freertos" "json" "mqtt_schema_validator"
)