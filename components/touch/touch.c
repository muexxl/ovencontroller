#include "touch.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOUCH";

#define CST816_ADDR            0x15
#define CST816_REG_XPOS_H      0x03
#define CST816_REG_XPOS_L      0x04
#define CST816_REG_YPOS_H      0x05
#define CST816_REG_YPOS_L      0x06
#define CST816_REG_POINTS      0x02
#define CST816_REG_CHIP_ID     0xA7
#define CST816_REG_FW_VERSION  0xA9

static uint16_t last_x = 0;
static uint16_t last_y = 0;
static bool is_pressed = false;

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t cst816_dev_handle = NULL;

static esp_err_t cst816_read_reg(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(cst816_dev_handle, &reg_addr, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t touch_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing touch with new I2C driver");
    
    // Configure I2C master bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus creation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C bus created");
    
    // Configure CST816 device
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CST816_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    
    ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &cst816_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add CST816 device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Give the touch controller time to initialize
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Try to read chip ID to verify communication
    uint8_t chip_id;
    ret = cst816_read_reg(CST816_REG_CHIP_ID, &chip_id, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CST816 Chip ID: 0x%02X", chip_id);
        
        uint8_t fw_version;
        if (cst816_read_reg(CST816_REG_FW_VERSION, &fw_version, 1) == ESP_OK) {
            ESP_LOGI(TAG, "CST816 FW Version: 0x%02X", fw_version);
        }
    } else {
        ESP_LOGW(TAG, "Failed to read CST816 chip ID, but continuing...");
    }
    
    ESP_LOGI(TAG, "Touch initialized");
    return ESP_OK;
}

// Function to share I2C bus with other components
i2c_master_bus_handle_t touch_get_i2c_bus(void)
{
    return i2c_bus_handle;
}

// LVGL touch read callback
// LVGL touch read callback
void touch_lvgl_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint8_t touch_data[5];
    
    if (cst816_read_reg(CST816_REG_POINTS, touch_data, 5) == ESP_OK) {
        uint8_t points = touch_data[0];
        
        if (points > 0) {
            uint16_t touch_x = ((touch_data[1] & 0x0F) << 8) | touch_data[2];
            uint16_t touch_y = ((touch_data[3] & 0x0F) << 8) | touch_data[4];
            
            // Transform coordinates: portrait (170x320) -> landscape (320x170)
            // Physical display is 170 wide x 320 tall (portrait)
            // We use it as 320 wide x 170 tall (landscape)
            // Rotation: 90 degrees clockwise
            last_x = touch_y;           // Display X = Touch Y (0-319)
            last_y = 169 - touch_x;     // Display Y = 169 - Touch X
            
            // Clamp to display bounds
            if (last_x > 319) last_x = 319;
            if (last_y > 169) last_y = 169;
            
            is_pressed = true;
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = last_x;
            data->point.y = last_y;
        } else {
            is_pressed = false;
            data->state = LV_INDEV_STATE_RELEASED;
            data->point.x = last_x;
            data->point.y = last_y;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = last_x;
        data->point.y = last_y;
    }
}