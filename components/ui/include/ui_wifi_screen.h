#ifndef UI_WIFI_SCREEN_H
#define UI_WIFI_SCREEN_H

#include "lvgl.h"

lv_obj_t* ui_wifi_screen_create(void);
void ui_wifi_screen_refresh(void);
void ui_wifi_screen_destroy(void);

#endif // UI_WIFI_SCREEN_H