#include "ui_main_screen.h"
#include "ui_manager.h"
#include "ui_common.h"
#include "max31855.h"
#include "io_expander.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "UI_MAIN";

// UI elements
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_temp1_label = NULL;
static lv_obj_t *s_temp2_label = NULL;
static lv_obj_t *s_heater1_btn = NULL;
static lv_obj_t *s_heater2_btn = NULL;
static lv_obj_t *s_fan_btn = NULL;
static lv_obj_t *s_wifi_status = NULL;
static lv_obj_t *s_settings_btn = NULL;

// State
static bool s_heater1_on = false;
static bool s_heater2_on = false;
static bool s_fan_on = false;

// Event handlers
static void heater1_clicked(lv_event_t *e)
{
    s_heater1_on = !s_heater1_on;
    io_expander_set_pin(HEATER_1_PIN, s_heater1_on);
    
    lv_obj_t *label = lv_obj_get_child(s_heater1_btn, 0);
    lv_label_set_text(label, s_heater1_on ? "H1:ON" : "H1:OFF");
    
    if (s_heater1_on) {
        lv_obj_set_style_bg_color(s_heater1_btn, UI_COLOR_WARNING, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(s_heater1_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    }
    
    ui_manager_update_heater_state(s_heater1_on, s_heater2_on, s_fan_on);
}

static void heater2_clicked(lv_event_t *e)
{
    s_heater2_on = !s_heater2_on;
    io_expander_set_pin(HEATER_2_PIN, s_heater2_on);
    
    lv_obj_t *label = lv_obj_get_child(s_heater2_btn, 0);
    lv_label_set_text(label, s_heater2_on ? "H2:ON" : "H2:OFF");
    
    if (s_heater2_on) {
        lv_obj_set_style_bg_color(s_heater2_btn, UI_COLOR_WARNING, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(s_heater2_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    }
    
    ui_manager_update_heater_state(s_heater1_on, s_heater2_on, s_fan_on);
}

static void fan_clicked(lv_event_t *e)
{
    s_fan_on = !s_fan_on;
    io_expander_set_pin(FAN_PIN, s_fan_on);
    
    lv_obj_t *label = lv_obj_get_child(s_fan_btn, 0);
    lv_label_set_text(label, s_fan_on ? "FAN:ON" : "FAN:OFF");
    
    if (s_fan_on) {
        lv_obj_set_style_bg_color(s_fan_btn, UI_COLOR_PRIMARY, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(s_fan_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    }
    
    ui_manager_update_heater_state(s_heater1_on, s_heater2_on, s_fan_on);
}

static void settings_clicked(lv_event_t *e)
{
    ui_manager_show_screen(UI_SCREEN_SETTINGS);
}

lv_obj_t* ui_main_screen_create(void)
{
    ESP_LOGI(TAG, "Creating main screen");
    
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG, LV_PART_MAIN);
    
    // Title with settings button
    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Oven Control");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_pos(title, UI_MARGIN, 2);
    
    // Settings button (gear icon - using symbol)
    s_settings_btn = lv_btn_create(s_screen);
    lv_obj_set_size(s_settings_btn, 30, 20);
    lv_obj_align(s_settings_btn, LV_ALIGN_TOP_RIGHT, -UI_MARGIN, 0);
    lv_obj_add_event_cb(s_settings_btn, settings_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *settings_label = lv_label_create(s_settings_btn);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_label);
    
    // WiFi status indicator
    s_wifi_status = lv_label_create(s_screen);
    lv_label_set_text(s_wifi_status, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_status, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_align(s_wifi_status, LV_ALIGN_TOP_RIGHT, -40, 2);
    
    // Temperature displays - side by side
    s_temp1_label = lv_label_create(s_screen);
    lv_label_set_text(s_temp1_label, "TC1: --");
    lv_obj_set_style_text_color(s_temp1_label, UI_COLOR_WARNING, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_temp1_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(s_temp1_label, UI_MARGIN, 25);
    
    s_temp2_label = lv_label_create(s_screen);
    lv_label_set_text(s_temp2_label, "TC2: --");
    lv_obj_set_style_text_color(s_temp2_label, UI_COLOR_WARNING, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_temp2_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(s_temp2_label, 170, 25);
    
    // Control buttons - 3 buttons in a row
    int btn_width = 100;
    int btn_y = 60;
    
    s_heater1_btn = ui_create_button(s_screen, "H1:OFF", UI_MARGIN, btn_y, btn_width, 45);
    lv_obj_add_event_cb(s_heater1_btn, heater1_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_heater1_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    
    s_heater2_btn = ui_create_button(s_screen, "H2:OFF", UI_MARGIN + btn_width + 5, btn_y, btn_width, 45);
    lv_obj_add_event_cb(s_heater2_btn, heater2_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_heater2_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    
    s_fan_btn = ui_create_button(s_screen, "FAN:OFF", UI_MARGIN + 2*(btn_width + 5), btn_y, btn_width, 45);
    lv_obj_add_event_cb(s_fan_btn, fan_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_fan_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    
    // Footer status
    lv_obj_t *footer = lv_label_create(s_screen);
    lv_label_set_text(footer, "v1.0");
    lv_obj_set_style_text_color(footer, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_RIGHT, -UI_MARGIN, -2);
    
    return s_screen;
}

void ui_main_screen_update(void)
{
    if (!s_screen) return;
    
    ui_shared_data_t *data = ui_manager_get_shared_data();
    
    // Update temperature labels
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "TC1: %.1f C", data->temp1);
    lv_label_set_text(s_temp1_label, temp_str);
    
    snprintf(temp_str, sizeof(temp_str), "TC2: %.1f C", data->temp2);
    lv_label_set_text(s_temp2_label, temp_str);
    
    // Update WiFi status
    if (data->wifi_connected) {
        lv_obj_set_style_text_color(s_wifi_status, UI_COLOR_SUCCESS, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_color(s_wifi_status, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
    }
}

void ui_main_screen_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}