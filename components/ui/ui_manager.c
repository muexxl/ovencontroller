#include "ui_manager.h"
#include "ui_main_screen.h"
#include "ui_settings_screen.h"
#include "ui_wifi_screen.h"
#include "ui_calibration_screen.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_MGR";

// Screen objects
static lv_obj_t *s_screens[UI_SCREEN_COUNT] = {NULL};
static ui_screen_id_t s_current_screen = UI_SCREEN_MAIN;

// Shared data
static ui_shared_data_t s_shared_data = {0};

// LVGL timer for updates
static lv_timer_t *s_update_timer = NULL;

static void update_timer_cb(lv_timer_t *timer);

esp_err_t ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI Manager");
    
    // Initialize shared data
    memset(&s_shared_data, 0, sizeof(ui_shared_data_t));
    strcpy(s_shared_data.wifi_ip, "0.0.0.0");
    strcpy(s_shared_data.wifi_ssid, "Not connected");
    
    // Create main screen
    s_screens[UI_SCREEN_MAIN] = ui_main_screen_create();
    lv_scr_load(s_screens[UI_SCREEN_MAIN]);
    
    // Create update timer (100ms)
    s_update_timer = lv_timer_create(update_timer_cb, 100, NULL);
    
    ESP_LOGI(TAG, "UI Manager initialized");
    return ESP_OK;
}

void ui_manager_show_screen(ui_screen_id_t screen_id)
{
    if (screen_id >= UI_SCREEN_COUNT) {
        ESP_LOGE(TAG, "Invalid screen ID: %d", screen_id);
        return;
    }
    
    if (screen_id == s_current_screen) {
        return;
    }
    
    ESP_LOGI(TAG, "Switching to screen %d", screen_id);
    
    // Destroy old screen if not main
    if (s_current_screen != UI_SCREEN_MAIN && s_screens[s_current_screen]) {
        switch (s_current_screen) {
            case UI_SCREEN_SETTINGS:
                ui_settings_screen_destroy();
                break;
            case UI_SCREEN_WIFI:
                ui_wifi_screen_destroy();
                break;
            case UI_SCREEN_CALIBRATION:
                ui_calibration_screen_destroy();
                break;
            default:
                break;
        }
        s_screens[s_current_screen] = NULL;
    }
    
    // Create new screen if needed
    if (!s_screens[screen_id]) {
        switch (screen_id) {
            case UI_SCREEN_MAIN:
                s_screens[screen_id] = ui_main_screen_create();
                break;
            case UI_SCREEN_SETTINGS:
                s_screens[screen_id] = ui_settings_screen_create();
                break;
            case UI_SCREEN_WIFI:
                s_screens[screen_id] = ui_wifi_screen_create();
                break;
            case UI_SCREEN_CALIBRATION:
                s_screens[screen_id] = ui_calibration_screen_create();
                break;
            default:
                ESP_LOGE(TAG, "Unknown screen ID");
                return;
        }
    }
    
    // Load screen
    lv_scr_load(s_screens[screen_id]);
    s_current_screen = screen_id;
}

ui_screen_id_t ui_manager_get_current_screen(void)
{
    return s_current_screen;
}

void ui_manager_update_temps(float temp1, float temp2)
{
    s_shared_data.temp1 = temp1;
    s_shared_data.temp2 = temp2;
}

void ui_manager_update_heater_state(bool h1, bool h2, bool fan)
{
    s_shared_data.heater1_on = h1;
    s_shared_data.heater2_on = h2;
    s_shared_data.fan_on = fan;
}

void ui_manager_update_wifi_state(bool connected, const char *ssid, const char *ip)
{
    s_shared_data.wifi_connected = connected;
    if (ssid) {
        strncpy(s_shared_data.wifi_ssid, ssid, sizeof(s_shared_data.wifi_ssid) - 1);
    }
    if (ip) {
        strncpy(s_shared_data.wifi_ip, ip, sizeof(s_shared_data.wifi_ip) - 1);
    }
}

ui_shared_data_t* ui_manager_get_shared_data(void)
{
    return &s_shared_data;
}

static void update_timer_cb(lv_timer_t *timer)
{
    // Update current screen
    switch (s_current_screen) {
        case UI_SCREEN_MAIN:
            ui_main_screen_update();
            break;
        case UI_SCREEN_CALIBRATION:
            ui_calibration_screen_update();
            break;
        default:
            break;
    }
}

// Common UI helpers
lv_obj_t* ui_create_header(lv_obj_t *parent, const char *title)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, LCD_WIDTH, UI_HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x002244), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    
    lv_obj_t *label = lv_label_create(header);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_center(label);
    
    return header;
}

lv_obj_t* ui_create_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return btn;
}

void ui_show_message_box(const char *title, const char *message, lv_event_cb_t close_cb)
{
    lv_obj_t *msgbox = lv_msgbox_create(NULL, title, message, NULL, true);
    lv_obj_center(msgbox);
    
    if (close_cb) {
        lv_obj_add_event_cb(msgbox, close_cb, LV_EVENT_CLICKED, NULL);
    }
}