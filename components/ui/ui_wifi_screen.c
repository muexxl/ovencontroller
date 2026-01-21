#include "ui_wifi_screen.h"
#include "ui_manager.h"
#include "ui_keyboard.h"
#include "ui_common.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include <string.h>

// Define lock symbol if not available in LVGL version
#ifndef LV_SYMBOL_LOCK
#define LV_SYMBOL_LOCK "\xEF\x80\xA3"  // Unicode lock symbol (U+F023)
#endif

static const char *TAG = "UI_WIFI";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_status_label = NULL;
static char s_selected_ssid[33] = {0};
static int s_network_indices[WIFI_MAX_SCAN_RESULTS];

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

    // Get network index from user data
    int *index_ptr = (int *)lv_event_get_user_data(e);
    if (!index_ptr) {
        ESP_LOGE(TAG, "Failed to get network index");
        return;
    }
    int network_index = *index_ptr;

    // Get scan results
    wifi_ap_info_t results[WIFI_MAX_SCAN_RESULTS];
    int count = wifi_manager_get_scan_results(results, WIFI_MAX_SCAN_RESULTS);

    if (network_index < 0 || network_index >= count) {
        ESP_LOGE(TAG, "Invalid network index: %d", network_index);
        return;
    }

    // Get selected network info
    strncpy(s_selected_ssid, results[network_index].ssid, sizeof(s_selected_ssid) - 1);
    s_selected_ssid[sizeof(s_selected_ssid) - 1] = '\0';
    bool is_open = results[network_index].is_open;

    ESP_LOGI(TAG, "Selected network: %s", s_selected_ssid);

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

    // Header
    lv_obj_t *header_label = lv_label_create(s_screen);
    lv_label_set_text(header_label, "WiFi Setup");
    lv_obj_set_pos(header_label, UI_MARGIN, 5);

    // Status label
    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Ready");
    lv_obj_set_pos(s_status_label, UI_MARGIN, UI_HEADER_HEIGHT + 5);

    // Network list container
    s_list = lv_obj_create(s_screen);
    lv_obj_set_size(s_list, UI_BUTTON_WIDTH_FULL, 80);
    lv_obj_set_pos(s_list, UI_MARGIN, UI_HEADER_HEIGHT + 25);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    
    // Scan button
    lv_obj_t *scan_btn = lv_btn_create(s_screen);
    lv_obj_set_size(scan_btn, UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_set_pos(scan_btn, UI_MARGIN, LCD_HEIGHT - 70);
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan Networks");
    lv_obj_add_event_cb(scan_btn, scan_clicked, LV_EVENT_CLICKED, NULL);

    // Back button
    lv_obj_t *back_btn = lv_btn_create(s_screen);
    lv_obj_set_size(back_btn, UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_set_pos(back_btn, UI_MARGIN, LCD_HEIGHT - 35);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "< Back");
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
        return;
    }
    
    // Add networks to list
    for (int i = 0; i < count && i < 10; i++) {  // Limit to 10 networks
        // Validate SSID
        if (results[i].ssid[0] == '\0') {
            ESP_LOGW(TAG, "Empty SSID at index %d, skipping", i);
            continue;
        }

        // Ensure SSID is null-terminated
        results[i].ssid[WIFI_MAX_SSID_LEN] = '\0';

        char btn_text[64];
        const char *lock_prefix = results[i].is_open ? " " : "[*] ";
        int text_len = snprintf(btn_text, sizeof(btn_text), "%s%s (%d)",
                                lock_prefix, results[i].ssid, results[i].rssi);

        if (text_len < 0 || text_len >= sizeof(btn_text)) {
            ESP_LOGW(TAG, "Network name too long, skipping");
            continue;
        }

        s_network_indices[i] = i;

        lv_obj_t *btn = lv_btn_create(s_list);
        if (!btn) {
            ESP_LOGE(TAG, "Failed to create button for network %d", i);
            continue;
        }

        lv_obj_set_size(btn, UI_BUTTON_WIDTH_FULL - 10, 30);
        lv_obj_add_event_cb(btn, network_clicked, LV_EVENT_CLICKED, &s_network_indices[i]);

        lv_obj_t *label = lv_label_create(btn);
        if (!label) {
            ESP_LOGE(TAG, "Failed to create label for network %d", i);
            lv_obj_del(btn);
            continue;
        }

        lv_label_set_text(label, btn_text);
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