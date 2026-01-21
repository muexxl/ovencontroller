#ifndef UI_CALIBRATION_SCREEN_H
#define UI_CALIBRATION_SCREEN_H

#include "lvgl.h"

lv_obj_t* ui_calibration_screen_create(void);
void ui_calibration_screen_update(void);
void ui_calibration_screen_destroy(void);

#endif // UI_CALIBRATION_SCREEN_H