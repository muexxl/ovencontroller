#include "ui_wifi_screen.h"
#include "ui_manager.h"
#include "ui_keyboard.h"
#include "ui_common.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_WIFI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_status_label = NULL;
static char s_selected_ssid[33] = {0};

// Event handlers
static void scan_clicked(lv_event_t *e)
{
    lv_label_set_text(s_status_label, "Scanning...");
    
    if (wifi_manager_scan() == ESP_OK) {
        ui_wifi_screen_refresh();
    } else {
        lv_label_set_text(s_status_label, "Scan failed");
    }
}

static void back_clicked(lv_event_t *e)
{
    ui_manager_show_screen(UI_SCREEN_SETTINGS);
}

static void keyboard_done(const char *password, void *user_data)
{
    ESP_LOGI(TAG, "Connecting to '%s' with password", s_selected_ssid);
    
    lv_label_set_text(s_status_label, "Connecting...");
    
    if (wifi_manager_connect(s_selected_ssid, password) == ESP_OK) {
        wifi_manager_save_credentials(s_selected_ssid, password);
    }
    
    ui_keyboard_destroy();
}

static void network_clicked(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    const char *text = lv_label_get_text(label);
    
    // Extract SSID (format: "SSID (RSSI) [AUTH]")
    char ssid[33];
    sscanf(text, "%32[^ (]", ssid);
    strncpy(s_selected_ssid, ssid, sizeof(s_selected_ssid) - 1);
    
    ESP_LOGI(TAG, "Selected network: %s", s_selected_ssid);
    
    // Check if network is open
    wifi_ap_info_t results[WIFI_MAX_SCAN_RESULTS];
    int count = wifi_manager_get_scan_results(results, WIFI_MAX_SCAN_RESULTS);
    
    bool is_open = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(results[i].ssid, s_selected_ssid) == 0) {
            is_open = results[i].is_open;
            break;
        }
    }
    
    if (is_open) {
        // Connect without password
        wifi_manager_connect(s_selected_ssid, NULL);
        wifi_manager_save_credentials(s_selected_ssid, NULL);
        lv_label_set_text(s_status_label, "Connecting...");
    } else {
        // Show keyboard for password
        ui_keyboard_create(s_screen, "", keyboard_done, NULL);
    }
}

lv_obj_t* ui_wifi_screen_create(void)
{
    ESP_LOGI(TAG, "Creating WiFi screen");
    
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG, LV_PART_MAIN);
    
    // Header
    ui_create_header(s_screen, "WiFi Setup");
    
    // Status label
    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_pos(s_status_label, UI_MARGIN, UI_HEADER_HEIGHT + 5);
    
    // Network list container
    s_list = lv_obj_create(s_screen);
    lv_obj_set_size(s_list, UI_BUTTON_WIDTH_FULL, 80);
    lv_obj_set_pos(s_list, UI_MARGIN, UI_HEADER_HEIGHT + 25);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x001122), LV_PART_MAIN);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    
    // Scan button
    lv_obj_t *scan_btn = ui_create_button(s_screen, LV_SYMBOL_REFRESH " Scan", 
                                          UI_MARGIN, LCD_HEIGHT - 70, 
                                          UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_add_event_cb(scan_btn, scan_clicked, LV_EVENT_CLICKED, NULL);
    
    // Back button
    lv_obj_t *back_btn = ui_create_button(s_screen, LV_SYMBOL_LEFT " Back", 
                                          UI_MARGIN, LCD_HEIGHT - 35, 
                                          UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_add_event_cb(back_btn, back_clicked, LV_EVENT_CLICKED, NULL);
    
    // Initial scan
    wifi_manager_scan();
    ui_wifi_screen_refresh();
    
    return s_screen;
}

void ui_wifi_screen_refresh(void)
{
    if (!s_list) return;
    
    // Clear list
    lv_obj_clean(s_list);
    
    // Get scan results
    wifi_ap_info_t results[WIFI_MAX_SCAN_RESULTS];
    int count = wifi_manager_get_scan_results(results, WIFI_MAX_SCAN_RESULTS);
    
    ESP_LOGI(TAG, "Displaying %d networks", count);
    
    if (count == 0) {
        lv_obj_t *label = lv_label_create(s_list);
        lv_label_set_text(label, "No networks found");
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
        return;
    }
    
    // Add networks to list
    for (int i = 0; i < count; i++) {
        char btn_text[64];
        const char *lock = results[i].is_open ? "" : LV_SYMBOL_LOCK;
        snprintf(btn_text, sizeof(btn_text), "%s %s (%d)", 
                 lock, results[i].ssid, results[i].rssi);
        
        lv_obj_t *btn = lv_btn_create(s_list);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, 30);
        lv_obj_add_event_cb(btn, network_clicked, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, btn_text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN);
    }
    
    lv_label_set_text(s_status_label, "Select network");
}

void ui_wifi_screen_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
        s_list = NULL;
    }
}