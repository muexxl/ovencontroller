#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";
static const char *NVS_NAMESPACE = "wifi_config";
static const char *NVS_SSID_KEY = "ssid";
static const char *NVS_PASS_KEY = "password";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Global state
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static wifi_state_t s_wifi_state = WIFI_STATE_IDLE;
static wifi_ap_info_t s_scan_results[WIFI_MAX_SCAN_RESULTS];
static int s_scan_count = 0;
static int s_retry_num = 0;
static wifi_event_cb_t s_event_callback = NULL;
static void *s_callback_user_data = NULL;

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static void set_wifi_state(wifi_state_t new_state);

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi STA interface
    s_sta_netif = esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler, NULL));
    
    // Set WiFi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi Manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_scan(void)
{
    if (s_wifi_state == WIFI_STATE_SCANNING) {
        ESP_LOGW(TAG, "Scan already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting WiFi scan");
    set_wifi_state(WIFI_STATE_SCANNING);
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(ret));
        set_wifi_state(WIFI_STATE_IDLE);
        return ret;
    }
    
    // Get scan results
    uint16_t ap_count = WIFI_MAX_SCAN_RESULTS;
    wifi_ap_record_t ap_records[WIFI_MAX_SCAN_RESULTS];
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    
    s_scan_count = 0;
    for (int i = 0; i < ap_count && i < WIFI_MAX_SCAN_RESULTS; i++) {
        strncpy(s_scan_results[i].ssid, (char *)ap_records[i].ssid, WIFI_MAX_SSID_LEN);
        s_scan_results[i].ssid[WIFI_MAX_SSID_LEN] = '\0';
        s_scan_results[i].rssi = ap_records[i].rssi;
        s_scan_results[i].auth_mode = ap_records[i].authmode;
        s_scan_results[i].is_open = (ap_records[i].authmode == WIFI_AUTH_OPEN);
        s_scan_count++;
    }
    
    ESP_LOGI(TAG, "Found %d access points", s_scan_count);
    set_wifi_state(WIFI_STATE_IDLE);
    
    return ESP_OK;
}

int wifi_manager_get_scan_results(wifi_ap_info_t *results, int max_results)
{
    int count = (s_scan_count < max_results) ? s_scan_count : max_results;
    if (results) {
        memcpy(results, s_scan_results, count * sizeof(wifi_ap_info_t));
    }
    return count;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to '%s'", ssid);
    
    // Disconnect if already connected
    if (s_wifi_state == WIFI_STATE_CONNECTED) {
        esp_wifi_disconnect();
    }
    
    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    set_wifi_state(WIFI_STATE_CONNECTING);
    s_retry_num = 0;
    
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        set_wifi_state(WIFI_STATE_ERROR);
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting WiFi");
    set_wifi_state(WIFI_STATE_DISCONNECTED);
    return esp_wifi_disconnect();
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

bool wifi_manager_is_connected(void)
{
    return (s_wifi_state == WIFI_STATE_CONNECTED);
}

esp_err_t wifi_manager_get_ip(char *ip_str, size_t len)
{
    if (!ip_str || !s_sta_netif) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_sta_netif, &ip_info);
    if (ret != ESP_OK) {
        return ret;
    }
    
    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    if (!ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_str(nvs_handle, NVS_SSID_KEY, ssid);
    if (ret == ESP_OK && password) {
        ret = nvs_set_str(nvs_handle, NVS_PASS_KEY, password);
    }
    
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "WiFi credentials saved");
    }
    
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t wifi_manager_load_credentials(wifi_credentials_t *creds)
{
    if (!creds) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t ssid_len = sizeof(creds->ssid);
    size_t pass_len = sizeof(creds->password);
    
    ret = nvs_get_str(nvs_handle, NVS_SSID_KEY, creds->ssid, &ssid_len);
    if (ret == ESP_OK) {
        ret = nvs_get_str(nvs_handle, NVS_PASS_KEY, creds->password, &pass_len);
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            creds->password[0] = '\0';
            ret = ESP_OK;
        }
    }
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded credentials for '%s'", creds->ssid);
    }
    
    return ret;
}

esp_err_t wifi_manager_auto_connect(void)
{
    wifi_credentials_t creds;
    esp_err_t ret = wifi_manager_load_credentials(&creds);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Auto-connecting to saved network");
        return wifi_manager_connect(creds.ssid, creds.password);
    }
    
    ESP_LOGI(TAG, "No saved credentials found");
    return ret;
}

void wifi_manager_register_callback(wifi_event_cb_t callback, void *user_data)
{
    s_event_callback = callback;
    s_callback_user_data = user_data;
}

const char* wifi_manager_get_auth_mode_str(wifi_auth_mode_t auth_mode)
{
    switch (auth_mode) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

// Internal functions
static void set_wifi_state(wifi_state_t new_state)
{
    s_wifi_state = new_state;
    
    if (s_event_callback) {
        s_event_callback(new_state, s_callback_user_data);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started");
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected");
                
                if (s_wifi_state == WIFI_STATE_CONNECTING) {
                    if (s_retry_num < 5) {
                        esp_wifi_connect();
                        s_retry_num++;
                        ESP_LOGI(TAG, "Retry to connect (%d/5)", s_retry_num);
                    } else {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                        set_wifi_state(WIFI_STATE_ERROR);
                        ESP_LOGE(TAG, "Failed to connect after 5 retries");
                    }
                } else {
                    set_wifi_state(WIFI_STATE_DISCONNECTED);
                }
                break;
                
            default:
                break;
        }
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        set_wifi_state(WIFI_STATE_CONNECTED);
        s_retry_num = 0;
    }
}