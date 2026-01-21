#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "esp_err.h"
#include "ui_common.h"

// Initialize UI system
esp_err_t ui_manager_init(void);

// Screen navigation
void ui_manager_show_screen(ui_screen_id_t screen_id);
ui_screen_id_t ui_manager_get_current_screen(void);

// Update shared data
void ui_manager_update_temps(float temp1, float temp2);
void ui_manager_update_heater_state(bool h1, bool h2, bool fan);
void ui_manager_update_wifi_state(bool connected, const char *ssid, const char *ip);

// Get shared data
ui_shared_data_t* ui_manager_get_shared_data(void);

#endif // UI_MANAGER_H