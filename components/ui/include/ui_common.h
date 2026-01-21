#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl.h"
#include "display.h"

// Screen IDs
typedef enum {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_WIFI,
    UI_SCREEN_CALIBRATION,
    UI_SCREEN_COUNT
} ui_screen_id_t;

// Common colors
#define UI_COLOR_BG         lv_color_hex(0x003a57)
#define UI_COLOR_PRIMARY    lv_color_hex(0x0088CC)
#define UI_COLOR_TEXT       lv_color_white()
#define UI_COLOR_TEXT_DIM   lv_color_hex(0x888888)
#define UI_COLOR_WARNING    lv_color_hex(0xFFAA00)
#define UI_COLOR_ERROR      lv_color_hex(0xFF3333)
#define UI_COLOR_SUCCESS    lv_color_hex(0x33FF33)

// Common dimensions (for 320x170 display)
#define UI_HEADER_HEIGHT    20
#define UI_FOOTER_HEIGHT    15
#define UI_MARGIN           5
#define UI_BUTTON_HEIGHT    40
#define UI_BUTTON_WIDTH_FULL (LCD_WIDTH - 2*UI_MARGIN)
#define UI_BUTTON_WIDTH_HALF ((LCD_WIDTH - 3*UI_MARGIN) / 2)

// Shared data structure for inter-screen communication
typedef struct {
    float temp1;
    float temp2;
    bool heater1_on;
    bool heater2_on;
    bool fan_on;
    bool wifi_connected;
    char wifi_ip[16];
    char wifi_ssid[33];
} ui_shared_data_t;

// Common functions
lv_obj_t* ui_create_header(lv_obj_t *parent, const char *title);
lv_obj_t* ui_create_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h);
void ui_show_message_box(const char *title, const char *message, lv_event_cb_t close_cb);

#endif // UI_COMMON_H