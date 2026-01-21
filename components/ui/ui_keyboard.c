#include "ui_keyboard.h"
#include "ui_common.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_KEYBOARD";

static lv_obj_t *s_keyboard_container = NULL;
static lv_obj_t *s_textarea = NULL;
static lv_obj_t *s_keyboard = NULL;
static ui_keyboard_cb_t s_callback = NULL;
static void *s_user_data = NULL;

static void done_clicked(lv_event_t *e)
{
    const char *text = lv_textarea_get_text(s_textarea);
    
    ESP_LOGI(TAG, "Keyboard done, text length: %d", strlen(text));
    
    if (s_callback) {
        s_callback(text, s_user_data);
    }
}

static void cancel_clicked(lv_event_t *e)
{
    ESP_LOGI(TAG, "Keyboard cancelled");
    ui_keyboard_destroy();
}

lv_obj_t* ui_keyboard_create(lv_obj_t *parent, const char *initial_text, 
                             ui_keyboard_cb_t callback, void *user_data)
{
    ESP_LOGI(TAG, "Creating keyboard");
    
    s_callback = callback;
    s_user_data = user_data;
    
    // Create modal container (overlay)
    s_keyboard_container = lv_obj_create(parent);
    lv_obj_set_size(s_keyboard_container, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(s_keyboard_container, 0, 0);
    lv_obj_set_style_bg_color(s_keyboard_container, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_keyboard_container, LV_OPA_80, LV_PART_MAIN);
    
    // Text area for password input
    s_textarea = lv_textarea_create(s_keyboard_container);
    lv_obj_set_size(s_textarea, LCD_WIDTH - 20, 30);
    lv_obj_set_pos(s_textarea, 10, 5);
    lv_textarea_set_placeholder_text(s_textarea, "Enter password...");
    lv_textarea_set_password_mode(s_textarea, true);
    lv_textarea_set_one_line(s_textarea, true);
    
    if (initial_text) {
        lv_textarea_set_text(s_textarea, initial_text);
    }
    
    // Keyboard
    s_keyboard = lv_keyboard_create(s_keyboard_container);
    lv_obj_set_size(s_keyboard, LCD_WIDTH, 100);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_keyboard, s_textarea);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    // Done/Cancel buttons
    lv_obj_t *done_btn = lv_btn_create(s_keyboard_container);
    lv_obj_set_size(done_btn, 70, 25);
    lv_obj_set_pos(done_btn, LCD_WIDTH - 150, 40);
    lv_obj_add_event_cb(done_btn, done_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(done_btn, UI_COLOR_SUCCESS, LV_PART_MAIN);
    
    lv_obj_t *done_label = lv_label_create(done_btn);
    lv_label_set_text(done_label, "Connect");
    lv_obj_center(done_label);
    
    lv_obj_t *cancel_btn = lv_btn_create(s_keyboard_container);
    lv_obj_set_size(cancel_btn, 70, 25);
    lv_obj_set_pos(cancel_btn, LCD_WIDTH - 75, 40);
    lv_obj_add_event_cb(cancel_btn, cancel_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(cancel_btn, UI_COLOR_ERROR, LV_PART_MAIN);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    
    return s_keyboard_container;
}

void ui_keyboard_destroy(void)
{
    if (s_keyboard_container) {
        lv_obj_del(s_keyboard_container);
        s_keyboard_container = NULL;
        s_textarea = NULL;
        s_keyboard = NULL;
        s_callback = NULL;
        s_user_data = NULL;
    }
}