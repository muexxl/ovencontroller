#ifndef UI_MAIN_SCREEN_H
#define UI_MAIN_SCREEN_H

#include "lvgl.h"

lv_obj_t* ui_main_screen_create(void);
void ui_main_screen_update(void);
void ui_main_screen_destroy(void);

#endif // UI_MAIN_SCREEN_H