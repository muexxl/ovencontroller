#include "io_expander.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "IO_EXPANDER";
static uint8_t current_state = 0xFF;
static SemaphoreHandle_t io_mutex = NULL;
static i2c_master_dev_handle_t tca9554_dev_handle = NULL;

// TCA9554 Register addresses
#define TCA9554_REG_INPUT    0x00
#define TCA9554_REG_OUTPUT   0x01
#define TCA9554_REG_POLARITY 0x02
#define TCA9554_REG_CONFIG   0x03

// Write to a TCA9554 register
static esp_err_t tca9554_write_register(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(tca9554_dev_handle, data, 2, pdMS_TO_TICKS(100));
}

// Read from a TCA9554 register
static esp_err_t tca9554_read_register(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(tca9554_dev_handle, &reg, 1, value, 1, pdMS_TO_TICKS(100));
}

esp_err_t io_expander_init(i2c_master_bus_handle_t i2c_bus)
{
    esp_err_t ret;
    uint8_t data;
    
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing TCA9554 I/O expander at 0x%02X", IO_EXPANDER_ADDR);
    
    // Create mutex for thread-safe access
    io_mutex = xSemaphoreCreateMutex();
    if (io_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }
    
    // Configure TCA9554 device on existing bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IO_EXPANDER_ADDR,
        .scl_speed_hz = 400000,
    };
    
    ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &tca9554_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add TCA9554 device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Read current configuration
    ret = tca9554_read_register(TCA9554_REG_CONFIG, &data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read config register: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Current config: 0x%02X", data);
    
    // Configure all pins as outputs
    ret = tca9554_write_register(TCA9554_REG_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure pins as outputs: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Configured all pins as outputs");
    
    // Set all outputs high initially
    ret = tca9554_write_register(TCA9554_REG_OUTPUT, 0xFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial output state: %s", esp_err_to_name(ret));
        return ret;
    }
    current_state = 0xFF;
    
    ESP_LOGI(TAG, "TCA9554 initialized successfully");
    return ESP_OK;
}

esp_err_t io_expander_write_all(uint8_t value)
{
    if (io_mutex != NULL) {
        xSemaphoreTake(io_mutex, portMAX_DELAY);
    }
    
    esp_err_t ret = tca9554_write_register(TCA9554_REG_OUTPUT, value);
    
    if (ret == ESP_OK) {
        current_state = value;
        ESP_LOGD(TAG, "Wrote 0x%02X to output register", value);
    } else {
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(ret));
    }
    
    if (io_mutex != NULL) {
        xSemaphoreGive(io_mutex);
    }
    
    return ret;
}

esp_err_t io_expander_read_all(uint8_t *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (io_mutex != NULL) {
        xSemaphoreTake(io_mutex, portMAX_DELAY);
    }
    
    esp_err_t ret = tca9554_read_register(TCA9554_REG_INPUT, value);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
    }
    
    if (io_mutex != NULL) {
        xSemaphoreGive(io_mutex);
    }
    
    return ret;
}

esp_err_t io_expander_set_pin(exio_pin_t pin, bool level)
{
    if (pin > 7) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t new_state = current_state;
    
    if (level) {
        new_state |= (1 << pin);
    } else {
        new_state &= ~(1 << pin);
    }
    
    esp_err_t ret = io_expander_write_all(new_state);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set EXIO%d = %d (state=0x%02X)", pin, level, new_state);
    } else {
        ESP_LOGE(TAG, "Failed to set EXIO%d", pin);
    }
    
    return ret;
}

esp_err_t io_expander_get_pin(exio_pin_t pin, bool *level)
{
    if (pin > 7 || level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t state;
    esp_err_t ret = io_expander_read_all(&state);
    
    if (ret == ESP_OK) {
        *level = (state & (1 << pin)) ? true : false;
    }
    
    return ret;
}