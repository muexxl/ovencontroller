#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "display.h"
#include "touch.h"
#include "max31855.h"
#include "io_expander.h"

static const char *TAG = "MAIN";

// Component handles
static max31855_handle_t tc1_handle;
static max31855_handle_t tc2_handle;

// UI elements
static lv_obj_t *temp1_label;
static lv_obj_t *temp2_label;
static lv_obj_t *heater1_btn;
static lv_obj_t *heater2_btn;
static lv_obj_t *fan_btn;
static lv_obj_t *status_label;

// State variables
static bool heater1_state = false;
static bool heater2_state = false;
static bool fan_state = false;

// Button event handlers
static void heater1_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        heater1_state = !heater1_state;
        io_expander_set_pin(HEATER_1_PIN, heater1_state);
        
        lv_obj_t *label = lv_obj_get_child(heater1_btn, 0);
        lv_label_set_text(label, heater1_state ? "H1:ON" : "H1:OFF");
        
        ESP_LOGI(TAG, "Heater 1: %s", heater1_state ? "ON" : "OFF");
    }
}

static void heater2_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        heater2_state = !heater2_state;
        io_expander_set_pin(HEATER_2_PIN, heater2_state);
        
        lv_obj_t *label = lv_obj_get_child(heater2_btn, 0);
        lv_label_set_text(label, heater2_state ? "H2:ON" : "H2:OFF");
        
        ESP_LOGI(TAG, "Heater 2: %s", heater2_state ? "ON" : "OFF");
    }
}

static void fan_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        fan_state = !fan_state;
        io_expander_set_pin(FAN_PIN, fan_state);
        
        lv_obj_t *label = lv_obj_get_child(fan_btn, 0);
        lv_label_set_text(label, fan_state ? "FAN:ON" : "FAN:OFF");
        
        ESP_LOGI(TAG, "Fan: %s", fan_state ? "ON" : "OFF");
    }
}

// Task to read thermocouples periodically
static void thermocouple_task(void *arg)
{
    max31855_data_t tc1_data, tc2_data;
    char temp_str[64];
    int read_count = 0;
    
    ESP_LOGI(TAG, "Thermocouple reading task started");
    
    while (1) {
        read_count++;
        
        // Read thermocouple 1
        if (max31855_read_temp(&tc1_handle, &tc1_data) == ESP_OK) {
            if (tc1_data.valid) {
                snprintf(temp_str, sizeof(temp_str), "TC1: %.1f C", 
                         tc1_data.thermocouple_temp);
                lv_label_set_text(temp1_label, temp_str);
            } else {
                lv_label_set_text(temp1_label, "TC1: FAULT");
            }
        } else {
            lv_label_set_text(temp1_label, "TC1: ERROR");
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Read thermocouple 2
        if (max31855_read_temp(&tc2_handle, &tc2_data) == ESP_OK) {
            if (tc2_data.valid) {
                snprintf(temp_str, sizeof(temp_str), "TC2: %.1f C", 
                         tc2_data.thermocouple_temp);
                lv_label_set_text(temp2_label, temp_str);
            } else {
                lv_label_set_text(temp2_label, "TC2: FAULT");
            }
        } else {
            lv_label_set_text(temp2_label, "TC2: ERROR");
        }
        
        // Update status (compact for 170px height)
        snprintf(temp_str, sizeof(temp_str), "Reads:%d", read_count);
        lv_label_set_text(status_label, temp_str);
        
        vTaskDelay(pdMS_TO_TICKS(450));
    }
}

// LVGL tick timer
static void lv_tick_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}

// LVGL handler task
static void lvgl_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Oven Controller Starting...");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize display
    ESP_LOGI(TAG, "Initializing display...");
    ESP_ERROR_CHECK(display_init());
    
    // Initialize touch
    ESP_LOGI(TAG, "Initializing touch controller...");
    ESP_ERROR_CHECK(touch_init());
    
    // Initialize MAX31855 thermocouples
    ESP_LOGI(TAG, "Initializing MAX31855 thermocouples...");
    i2c_master_bus_handle_t i2c_bus = touch_get_i2c_bus();
    ESP_ERROR_CHECK(max31855_init(i2c_bus, &tc1_handle, &tc2_handle));
    
    // Initialize LVGL
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();
    
    // Create display buffer
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf1[LCD_WIDTH * 20];
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LCD_WIDTH * 20);
    
    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = display_lvgl_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Register touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_lvgl_read;
    lv_indev_drv_register(&indev_drv);
    
    ESP_LOGI(TAG, "Creating UI for 320x170 display...");
    
    // ========== Create UI optimized for 320x170 ==========
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);
    
    // Title - compact at top
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Oven Ctrl");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 5, 2);
    
    // Temperature displays - side by side at top
    temp1_label = lv_label_create(scr);
    lv_label_set_text(temp1_label, "TC1: --");
    lv_obj_set_style_text_color(temp1_label, lv_color_hex(0xFFAA00), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp1_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(temp1_label, 5, 22);
    
    temp2_label = lv_label_create(scr);
    lv_label_set_text(temp2_label, "TC2: --");
    lv_obj_set_style_text_color(temp2_label, lv_color_hex(0xFFAA00), LV_PART_MAIN);
    lv_obj_set_style_text_font(temp2_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_pos(temp2_label, 165, 22);
    
    // Control buttons - 3 buttons in a row (compact)
    // Heater 1 button
    heater1_btn = lv_btn_create(scr);
    lv_obj_set_size(heater1_btn, 100, 50);
    lv_obj_set_pos(heater1_btn, 5, 55);
    lv_obj_add_event_cb(heater1_btn, heater1_event_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *h1_label = lv_label_create(heater1_btn);
    lv_label_set_text(h1_label, "H1:OFF");
    lv_obj_center(h1_label);
    
    // Heater 2 button
    heater2_btn = lv_btn_create(scr);
    lv_obj_set_size(heater2_btn, 100, 50);
    lv_obj_set_pos(heater2_btn, 110, 55);
    lv_obj_add_event_cb(heater2_btn, heater2_event_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *h2_label = lv_label_create(heater2_btn);
    lv_label_set_text(h2_label, "H2:OFF");
    lv_obj_center(h2_label);
    
    // Fan button
    fan_btn = lv_btn_create(scr);
    lv_obj_set_size(fan_btn, 100, 50);
    lv_obj_set_pos(fan_btn, 215, 55);
    lv_obj_add_event_cb(fan_btn, fan_event_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *fan_label = lv_label_create(fan_btn);
    lv_label_set_text(fan_label, "FAN:OFF");
    lv_obj_center(fan_label);
    
    // Status bar at bottom
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Ready");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_pos(status_label, 5, 155);
    
    // Version info
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "v1.0");
    lv_obj_set_style_text_color(info, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(info, LV_ALIGN_BOTTOM_RIGHT, -5, -2);
    
    ESP_LOGI(TAG, "UI created successfully");
    
    // Start tasks
    ESP_LOGI(TAG, "Starting tasks...");
    xTaskCreate(lv_tick_task, "lv_tick", 2048, NULL, 5, NULL);
    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);
    xTaskCreate(thermocouple_task, "thermocouple", 4096, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "========================================");
}
