/**
 * @file ovenctrl_refactored.c
 * @brief Main application file for oven controller
 * 
 * This refactored version uses modular components:
 * - wifi_manager: Handles WiFi connection and credentials
 * - ui_manager: Manages UI screens and LVGL
 * - Hardware components: display, touch, io_expander, max31855, pid_controller
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

// Hardware components
#include "display.h"
#include "touch.h"
#include "io_expander.h"
#include "max31855.h"
#include "pid_controller.h"

// High-level components
#include "wifi_manager.h"
#include "ui_manager.h"

static const char *TAG = "MAIN";

// Component handles
static max31855_handle_t tc1_handle;
static max31855_handle_t tc2_handle;
static i2c_master_bus_handle_t i2c_bus = NULL;

/**
 * @brief WiFi event callback
 * Updates UI when WiFi state changes
 */
static void wifi_event_callback(wifi_state_t state, void *user_data)
{
    ESP_LOGI(TAG, "WiFi state changed: %d", state);
    
    switch (state) {
        case WIFI_STATE_CONNECTED: {
            char ip_str[16];
            if (wifi_manager_get_ip(ip_str, sizeof(ip_str)) == ESP_OK) {
                wifi_credentials_t creds;
                if (wifi_manager_load_credentials(&creds) == ESP_OK) {
                    ui_manager_update_wifi_state(true, creds.ssid, ip_str);
                    ESP_LOGI(TAG, "Connected to %s, IP: %s", creds.ssid, ip_str);
                }
            }
            break;
        }
        
        case WIFI_STATE_DISCONNECTED:
        case WIFI_STATE_ERROR:
            ui_manager_update_wifi_state(false, "Not connected", "0.0.0.0");
            ESP_LOGI(TAG, "WiFi disconnected");
            break;
            
        case WIFI_STATE_CONNECTING:
            ESP_LOGI(TAG, "WiFi connecting...");
            break;
            
        default:
            break;
    }
}

/**
 * @brief Temperature reading task
 * Periodically reads thermocouples and updates UI
 */
static void thermocouple_task(void *arg)
{
    max31855_data_t tc1_data, tc2_data;
    float temp1 = 0.0f;
    float temp2 = 0.0f;
    
    ESP_LOGI(TAG, "Thermocouple reading task started");
    
    while (1) {
        // Read thermocouple 1
        if (max31855_read_temp(&tc1_handle, &tc1_data) == ESP_OK) {
            if (tc1_data.valid) {
                temp1 = tc1_data.thermocouple_temp;
            } else {
                ESP_LOGW(TAG, "TC1 fault detected");
                temp1 = -999.0f; // Error indicator
            }
        } else {
            ESP_LOGE(TAG, "Failed to read TC1");
            temp1 = -999.0f;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Read thermocouple 2
        if (max31855_read_temp(&tc2_handle, &tc2_data) == ESP_OK) {
            if (tc2_data.valid) {
                temp2 = tc2_data.thermocouple_temp;
            } else {
                ESP_LOGW(TAG, "TC2 fault detected");
                temp2 = -999.0f; // Error indicator
            }
        } else {
            ESP_LOGE(TAG, "Failed to read TC2");
            temp2 = -999.0f;
        }
        
        // Update UI with new temperatures
        ui_manager_update_temps(temp1, temp2);
        
        vTaskDelay(pdMS_TO_TICKS(450));
    }
}

/**
 * @brief LVGL tick timer task
 * Required for LVGL timing
 */
static void lv_tick_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}

/**
 * @brief LVGL handler task
 * Processes LVGL events and rendering
 */
static void lvgl_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
}

/**
 * @brief Initialize NVS (Non-Volatile Storage)
 * Required for WiFi and other persistent storage
 */
static esp_err_t init_nvs(void)
{
    ESP_LOGI(TAG, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief Initialize hardware components
 * Sets up display, touch, I2C, IO expander, and thermocouples
 */
static esp_err_t init_hardware(void)
{
    esp_err_t ret;
    
    // Initialize display
    ESP_LOGI(TAG, "Initializing display...");
    ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize touch controller (also initializes I2C bus)
    ESP_LOGI(TAG, "Initializing touch controller...");
    ret = touch_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Get I2C bus handle from touch driver
    i2c_bus = touch_get_i2c_bus();
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle");
        return ESP_FAIL;
    }
    
    // Initialize IO expander
    ESP_LOGI(TAG, "Initializing IO expander...");
    ret = io_expander_init(i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IO expander init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize MAX31855 thermocouples
    ESP_LOGI(TAG, "Initializing MAX31855 thermocouples...");
    ret = max31855_init(i2c_bus, &tc1_handle, &tc2_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAX31855 init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize PID controller
    ESP_LOGI(TAG, "Initializing PID controller...");
    ret = pid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PID controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Try to load PID calibration
    if (pid_is_calibrated()) {
        ESP_LOGI(TAG, "PID calibration data loaded");
    } else {
        ESP_LOGW(TAG, "No PID calibration data found - calibration required");
    }
    
    return ESP_OK;
}

/**
 * @brief Initialize LVGL library
 * Sets up LVGL with display and touch drivers
 */
static esp_err_t init_lvgl(void)
{
    ESP_LOGI(TAG, "Initializing LVGL...");
    
    // Initialize LVGL
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
    
    return ESP_OK;
}

/**
 * @brief Initialize WiFi manager
 * Sets up WiFi and attempts auto-connect
 */
static esp_err_t init_wifi(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register callback for WiFi events
    wifi_manager_register_callback(wifi_event_callback, NULL);
    
    // Try to auto-connect if credentials are saved
    ESP_LOGI(TAG, "Attempting WiFi auto-connect...");
    ret = wifi_manager_auto_connect();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Auto-connect initiated");
    } else if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved WiFi credentials - use WiFi screen to connect");
    } else {
        ESP_LOGW(TAG, "Auto-connect failed: %s", esp_err_to_name(ret));
    }
    
    return ESP_OK;
}

/**
 * @brief Create and start FreeRTOS tasks
 */
static void start_tasks(void)
{
    ESP_LOGI(TAG, "Starting tasks...");
    
    // LVGL tick task (high priority)
    xTaskCreate(lv_tick_task, "lv_tick", 2048, NULL, 5, NULL);
    
    // LVGL handler task (high priority)
    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);
    
    // Thermocouple reading task (medium priority)
    xTaskCreate(thermocouple_task, "thermocouple", 4096, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "All tasks started");
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Oven Controller Starting...         ");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());
    
    // Initialize hardware components
    ret = init_hardware();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware initialization failed!");
        return;
    }
    
    // Initialize LVGL
    ESP_ERROR_CHECK(init_lvgl());
    
    // Initialize UI manager (creates main screen)
    ESP_LOGI(TAG, "Initializing UI manager...");
    ESP_ERROR_CHECK(ui_manager_init());
    
    // Initialize WiFi (non-blocking, auto-connects in background)
    ret = init_wifi();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi initialization failed - continuing without WiFi");
    }
    
    // Start FreeRTOS tasks
    start_tasks();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   System Ready!                       ");
    ESP_LOGI(TAG, "========================================");
    
    // Print system information
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Display: %dx%d", LCD_WIDTH, LCD_HEIGHT);
    ESP_LOGI(TAG, "WiFi: %s", wifi_manager_is_connected() ? "Connected" : "Not connected");
    ESP_LOGI(TAG, "PID: %s", pid_is_calibrated() ? "Calibrated" : "Not calibrated");
}