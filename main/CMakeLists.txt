idf_component_register(
    SRCS "ovenctrl.c"
    INCLUDE_DIRS "."
    REQUIRES 
        display 
        touch 
        lvgl 
        max31855 
        io_expander 
        pid_controller
        wifi_manager
        ui
        nvs_flash
)