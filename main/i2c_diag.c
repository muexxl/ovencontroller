#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "I2C_DIAG";

#define I2C_MASTER_SCL_IO    8
#define I2C_MASTER_SDA_IO    18
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   400000

// Try PCF8574 protocol (direct read/write, no registers)
esp_err_t test_pcf8574(uint8_t addr)
{
    ESP_LOGI(TAG, "Testing PCF8574 protocol...");
    
    // Try direct write
    uint8_t data = 0xFF;
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, addr, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Direct write: %s", esp_err_to_name(ret));
    
    // Try direct read
    ret = i2c_master_read_from_device(I2C_MASTER_NUM, addr, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Direct read: %s, data=0x%02X", esp_err_to_name(ret), data);
    
    return ret;
}

// Try TCA9554 protocol (register-based)
esp_err_t test_tca9554(uint8_t addr)
{
    ESP_LOGI(TAG, "Testing TCA9554 protocol...");
    esp_err_t ret;
    uint8_t data;
    
    // Read Configuration register (0x03)
    ret = i2c_master_write_read_device(I2C_MASTER_NUM, addr, (uint8_t[]){0x03}, 1, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Read Config (0x03): %s, data=0x%02X", esp_err_to_name(ret), data);
    
    // Read Input register (0x00)
    ret = i2c_master_write_read_device(I2C_MASTER_NUM, addr, (uint8_t[]){0x00}, 1, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Read Input (0x00): %s, data=0x%02X", esp_err_to_name(ret), data);
    
    // Read Output register (0x01)
    ret = i2c_master_write_read_device(I2C_MASTER_NUM, addr, (uint8_t[]){0x01}, 1, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Read Output (0x01): %s, data=0x%02X", esp_err_to_name(ret), data);
    
    // Try to write to Output register
    uint8_t write_data[] = {0x01, 0xFF};  // Register 0x01, data 0xFF
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, addr, write_data, 2, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Write Output (0x01=0xFF): %s", esp_err_to_name(ret));
    
    // Configure all as outputs (write 0x00 to config register 0x03)
    uint8_t config_data[] = {0x03, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, addr, config_data, 2, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Write Config (0x03=0x00): %s", esp_err_to_name(ret));
    
    return ret;
}

// Try MCP23008 protocol (similar to TCA9554 but different registers)
esp_err_t test_mcp23008(uint8_t addr)
{
    ESP_LOGI(TAG, "Testing MCP23008 protocol...");
    esp_err_t ret;
    uint8_t data;
    
    // Read IODIR register (0x00)
    ret = i2c_master_write_read_device(I2C_MASTER_NUM, addr, (uint8_t[]){0x00}, 1, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Read IODIR (0x00): %s, data=0x%02X", esp_err_to_name(ret), data);
    
    // Read GPIO register (0x09)
    ret = i2c_master_write_read_device(I2C_MASTER_NUM, addr, (uint8_t[]){0x09}, 1, &data, 1, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Read GPIO (0x09): %s, data=0x%02X", esp_err_to_name(ret), data);
    
    // Set all pins as outputs (write 0x00 to IODIR 0x00)
    uint8_t config_data[] = {0x00, 0x00};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, addr, config_data, 2, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Write IODIR (0x00=0x00): %s", esp_err_to_name(ret));
    
    // Write to GPIO register (0x09)
    uint8_t gpio_data[] = {0x09, 0xFF};
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, addr, gpio_data, 2, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "  Write GPIO (0x09=0xFF): %s", esp_err_to_name(ret));
    
    return ret;
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "I/O Expander Protocol Detector");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    ESP_LOGI(TAG, "I2C init: %s", esp_err_to_name(ret));
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Testing device at 0x20");
    ESP_LOGI(TAG, "========================================");
    
    // Test different protocols
    test_pcf8574(0x20);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "");
    test_tca9554(0x20);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "");
    test_mcp23008(0x20);
    
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Testing complete!");
    ESP_LOGI(TAG, "Look for ESP_OK results above");
    ESP_LOGI(TAG, "========================================");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}