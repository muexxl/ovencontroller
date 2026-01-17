#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

// Configuration (adjust pins for your hardware)
#define I2C_MASTER_SCL_IO           8
#define I2C_MASTER_SDA_IO           18
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define TOUCH_INT_PIN               21
#define TOUCH_RST_PIN               22

#define CST816_ADDR                 0x15

// Registers
#define CST816_REG_GESTURE_ID       0x01
#define CST816_REG_FINGER_NUM       0x02
#define CST816_REG_XPOS_H           0x03
#define CST816_REG_XPOS_L           0x04
#define CST816_REG_YPOS_H           0x05
#define CST816_REG_YPOS_L           0x06
#define CST816_REG_CHIP_ID          0xA7
#define CST816_REG_FW_VERSION       0xA9

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
    uint8_t points;
} touch_data_t;

// I2C read from CST816
static esp_err_t cst816_read_reg(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, CST816_ADDR,
                                        &reg_addr, 1, data, len,
                                        pdMS_TO_TICKS(1000));
}

// I2C write to CST816
static esp_err_t cst816_write_reg(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, CST816_ADDR,
                                      write_buf, sizeof(write_buf),
                                      pdMS_TO_TICKS(1000));
}

// Initialize I2C
esp_err_t touch_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed");
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return ret;
    }

    ESP_LOGI(TAG, "I2C initialized successfully");
    return ESP_OK;
}

// Initialize CST816
esp_err_t cst816_init(void)
{
    esp_err_t ret;

    // Initialize I2C first
    ret = touch_i2c_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Configure reset pin if available
#ifdef TOUCH_RST_PIN
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TOUCH_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    // Reset the touch controller
    gpio_set_level(TOUCH_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TOUCH_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
#endif

    // Configure interrupt pin if available
#ifdef TOUCH_INT_PIN
    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << TOUCH_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&int_conf);
#endif

    // Read chip ID to verify communication
    uint8_t chip_id;
    ret = cst816_read_reg(CST816_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID");
        return ret;
    }

    ESP_LOGI(TAG, "CST816 Chip ID: 0x%02X", chip_id);

    // Read firmware version
    uint8_t fw_version;
    ret = cst816_read_reg(CST816_REG_FW_VERSION, &fw_version, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "CST816 FW Version: 0x%02X", fw_version);
    }

    return ESP_OK;
}

// Read touch data
esp_err_t cst816_read_touch(touch_data_t *touch)
{
    uint8_t data[7];
    esp_err_t ret;

    // Read all touch data registers at once
    ret = cst816_read_reg(CST816_REG_GESTURE_ID, data, 7);
    if (ret != ESP_OK) {
        return ret;
    }

    touch->gesture = data[0];
    touch->points = data[1];
    touch->x = ((data[2] & 0x0F) << 8) | data[3];
    touch->y = ((data[4] & 0x0F) << 8) | data[5];

    return ESP_OK;
}

// Get gesture name (for debugging)
const char* cst816_gesture_name(uint8_t gesture)
{
    switch(gesture) {
        case 0x00: return "None";
        case 0x01: return "Swipe Up";
        case 0x02: return "Swipe Down";
        case 0x03: return "Swipe Left";
        case 0x04: return "Swipe Right";
        case 0x05: return "Single Click";
        case 0x0B: return "Double Click";
        case 0x0C: return "Long Press";
        default: return "Unknown";
    }
}

// Task to continuously read touch data
void touch_task(void *pvParameters)
{
    touch_data_t touch;
    
    while (1) {
        if (cst816_read_touch(&touch) == ESP_OK) {
            if (touch.points > 0) {
                ESP_LOGI(TAG, "Touch: X=%d, Y=%d, Points=%d, Gesture=%s",
                         touch.x, touch.y, touch.points,
                         cst816_gesture_name(touch.gesture));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // Poll every 50ms
    }
}