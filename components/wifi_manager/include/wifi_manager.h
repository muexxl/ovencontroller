#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>

#define WIFI_MAX_SSID_LEN       32
#define WIFI_MAX_PASSWORD_LEN   64
#define WIFI_MAX_SCAN_RESULTS   20

// WiFi connection state
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_ERROR
} wifi_state_t;

// Scanned AP info
typedef struct {
    char ssid[WIFI_MAX_SSID_LEN + 1];
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
    bool is_open;
} wifi_ap_info_t;

// Stored credentials
typedef struct {
    char ssid[WIFI_MAX_SSID_LEN + 1];
    char password[WIFI_MAX_PASSWORD_LEN + 1];
} wifi_credentials_t;

// Callback types
typedef void (*wifi_event_cb_t)(wifi_state_t state, void *user_data);

// Function declarations
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_scan(void);
int wifi_manager_get_scan_results(wifi_ap_info_t *results, int max_results);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
esp_err_t wifi_manager_disconnect(void);
wifi_state_t wifi_manager_get_state(void);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len);
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);
esp_err_t wifi_manager_load_credentials(wifi_credentials_t *creds);
esp_err_t wifi_manager_auto_connect(void);
void wifi_manager_register_callback(wifi_event_cb_t callback, void *user_data);
const char* wifi_manager_get_auth_mode_str(wifi_auth_mode_t auth_mode);

#endif // WIFI_MANAGER_H