idf_component_register(
    SRCS "max31855.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_system freertos io_expander
)