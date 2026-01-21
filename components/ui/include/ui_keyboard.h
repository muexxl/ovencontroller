#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include "lvgl.h"

typedef void (*ui_keyboard_cb_t)(const char *text, void *user_data);

lv_obj_t* ui_keyboard_create(lv_obj_t *parent, const char *initial_text, 
                              ui_keyboard_cb_t callback, void *user_data);
void ui_keyboard_destroy(void);

#endif // UI_KEYBOARD_H