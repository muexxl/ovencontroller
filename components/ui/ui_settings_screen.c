#include "ui_settings_screen.h"
#include "ui_manager.h"
#include "ui_common.h"
#include "esp_log.h"

static const char *TAG = "UI_SETTINGS";

static lv_obj_t *s_screen = NULL;

// Event handlers
static void wifi_clicked(lv_event_t *e)
{
    ui_manager_show_screen(UI_SCREEN_WIFI);
}

static void calibration_clicked(lv_event_t *e)
{
    ui_manager_show_screen(UI_SCREEN_CALIBRATION);
}

static void back_clicked(lv_event_t *e)
{
    ui_manager_show_screen(UI_SCREEN_MAIN);
}

lv_obj_t* ui_settings_screen_create(void)
{
    ESP_LOGI(TAG, "Creating settings screen");
    
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG, LV_PART_MAIN);
    
    // Header
    ui_create_header(s_screen, "Settings");
    
    // Settings buttons
    int y = UI_HEADER_HEIGHT + 10;
    int btn_h = 35;
    int spacing = 5;
    
    // WiFi button
    lv_obj_t *wifi_btn = ui_create_button(s_screen, LV_SYMBOL_WIFI " WiFi Setup", 
                                          UI_MARGIN, y, UI_BUTTON_WIDTH_FULL, btn_h);
    lv_obj_add_event_cb(wifi_btn, wifi_clicked, LV_EVENT_CLICKED, NULL);
    y += btn_h + spacing;
    
    // Calibration button
    lv_obj_t *calib_btn = ui_create_button(s_screen, LV_SYMBOL_SETTINGS " Auto Calibrate", 
                                           UI_MARGIN, y, UI_BUTTON_WIDTH_FULL, btn_h);
    lv_obj_add_event_cb(calib_btn, calibration_clicked, LV_EVENT_CLICKED, NULL);
    y += btn_h + spacing;
    
    // OTA Update button (placeholder for now)
    lv_obj_t *ota_btn = ui_create_button(s_screen, LV_SYMBOL_DOWNLOAD " OTA Update", 
                                         UI_MARGIN, y, UI_BUTTON_WIDTH_FULL, btn_h);
    lv_obj_set_style_bg_color(ota_btn, lv_color_hex(0x444444), LV_PART_MAIN);
    // TODO: Add OTA event handler
    
    // Back button
    lv_obj_t *back_btn = ui_create_button(s_screen, LV_SYMBOL_LEFT " Back", 
                                          UI_MARGIN, LCD_HEIGHT - btn_h - UI_MARGIN, 
                                          UI_BUTTON_WIDTH_FULL, btn_h);
    lv_obj_add_event_cb(back_btn, back_clicked, LV_EVENT_CLICKED, NULL);
    
    return s_screen;
}

void ui_settings_screen_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}