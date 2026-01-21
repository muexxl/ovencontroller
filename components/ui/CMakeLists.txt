idf_component_register(
    SRCS 
        "ui_calibration_screen.c"
        "ui_keyboard.c"
        "ui_manager.c"
        "ui_main_screen.c"
        "ui_settings_screen.c"
        "ui_wifi_screen.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl display touch wifi_manager pid_controller max31855 io_expander
)