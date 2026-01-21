#include "ui_calibration_screen.h"
#include "ui_manager.h"
#include "ui_common.h"
#include "pid_controller.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "UI_CALIB";

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_temp_label = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_start_btn = NULL;
static lv_obj_t *s_stop_btn = NULL;
static lv_obj_t *s_results_label = NULL;
static lv_obj_t *s_save_btn = NULL;

// Event handlers
static void start_clicked(lv_event_t *e)
{
    ESP_LOGI(TAG, "Starting calibration");
    pid_start_calibration();
    
    lv_obj_add_flag(s_start_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_results_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_save_btn, LV_OBJ_FLAG_HIDDEN);
}

static void stop_clicked(lv_event_t *e)
{
    ESP_LOGI(TAG, "Stopping calibration");
    pid_stop_calibration();
    
    lv_obj_clear_flag(s_start_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
}

static void save_clicked(lv_event_t *e)
{
    ESP_LOGI(TAG, "Calibration accepted");
    // Results already saved during calibration
    ui_manager_show_screen(UI_SCREEN_MAIN);
}

static void back_clicked(lv_event_t *e)
{
    ui_manager_show_screen(UI_SCREEN_SETTINGS);
}

lv_obj_t* ui_calibration_screen_create(void)
{
    ESP_LOGI(TAG, "Creating calibration screen");
    
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG, LV_PART_MAIN);
    
    // Header
    ui_create_header(s_screen, "Auto Calibration");
    
    // Status label
    s_status_label = lv_label_create(s_screen);
    lv_label_set_text(s_status_label, "Ready to calibrate");
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_pos(s_status_label, UI_MARGIN, UI_HEADER_HEIGHT + 5);
    
    // Temperature label
    s_temp_label = lv_label_create(s_screen);
    lv_label_set_text(s_temp_label, "Temp: -- C");
    lv_obj_set_style_text_color(s_temp_label, UI_COLOR_WARNING, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_pos(s_temp_label, UI_MARGIN, UI_HEADER_HEIGHT + 25);
    
    // Progress bar
    s_progress_bar = lv_bar_create(s_screen);
    lv_obj_set_size(s_progress_bar, UI_BUTTON_WIDTH_FULL, 15);
    lv_obj_set_pos(s_progress_bar, UI_MARGIN, UI_HEADER_HEIGHT + 50);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    
    // Results label (hidden initially)
    s_results_label = lv_label_create(s_screen);
    lv_label_set_text(s_results_label, "");
    lv_obj_set_style_text_color(s_results_label, UI_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_results_label, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_pos(s_results_label, UI_MARGIN, UI_HEADER_HEIGHT + 70);
    lv_obj_add_flag(s_results_label, LV_OBJ_FLAG_HIDDEN);
    
    // Start button
    s_start_btn = ui_create_button(s_screen, LV_SYMBOL_PLAY " Start Calibration", 
                                   UI_MARGIN, LCD_HEIGHT - 70, 
                                   UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_add_event_cb(s_start_btn, start_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_start_btn, UI_COLOR_SUCCESS, LV_PART_MAIN);
    
    // Stop button (hidden initially)
    s_stop_btn = ui_create_button(s_screen, LV_SYMBOL_STOP " Stop", 
                                  UI_MARGIN, LCD_HEIGHT - 70, 
                                  UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_add_event_cb(s_stop_btn, stop_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_stop_btn, UI_COLOR_ERROR, LV_PART_MAIN);
    lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
    
    // Save button (hidden until calibration complete)
    s_save_btn = ui_create_button(s_screen, LV_SYMBOL_OK " Accept & Use", 
                                  UI_MARGIN, LCD_HEIGHT - 70, 
                                  UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_add_event_cb(s_save_btn, save_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_save_btn, UI_COLOR_SUCCESS, LV_PART_MAIN);
    lv_obj_add_flag(s_save_btn, LV_OBJ_FLAG_HIDDEN);
    
    // Back button
    lv_obj_t *back_btn = ui_create_button(s_screen, LV_SYMBOL_LEFT " Back", 
                                          UI_MARGIN, LCD_HEIGHT - 35, 
                                          UI_BUTTON_WIDTH_FULL, 30);
    lv_obj_add_event_cb(back_btn, back_clicked, LV_EVENT_CLICKED, NULL);
    
    return s_screen;
}

void ui_calibration_screen_update(void)
{
    if (!s_screen) return;
    
    calib_state_t state = pid_get_calib_state();
    int progress = pid_get_calib_progress();
    const char *status = pid_get_calib_status();
    
    // Update status
    lv_label_set_text(s_status_label, status);
    lv_bar_set_value(s_progress_bar, progress, LV_ANIM_ON);
    
    // Update temperature from shared data
    ui_shared_data_t *data = ui_manager_get_shared_data();
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "Temp: %.1f C", data->temp1);
    lv_label_set_text(s_temp_label, temp_str);
    
    // Show results when complete
    if (state == CALIB_COMPLETE) {
        lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_save_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_results_label, LV_OBJ_FLAG_HIDDEN);
        
        system_model_t model;
        pid_params_t pid;
        if (pid_get_calib_results(&model, &pid) == ESP_OK) {
            char results[128];
            snprintf(results, sizeof(results), 
                     "K=%.2f tau=%.1fs L=%.1fs\nKp=%.2f Ki=%.2f Kd=%.2f",
                     model.K, model.tau, model.L, pid.Kp, pid.Ki, pid.Kd);
            lv_label_set_text(s_results_label, results);
        }
    }
    
    // Show start button if idle/error
    if (state == CALIB_IDLE || state == CALIB_ERROR || state == CALIB_CANCELLED) {
        lv_obj_clear_flag(s_start_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_save_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_calibration_screen_destroy(void)
{
    if (s_screen) {
        lv_obj_del(s_screen);
        s_screen = NULL;
    }
}