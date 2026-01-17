#include "max31855.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "MAX31855";

#define SPI_DELAY_US 2

// Software SPI: Read one bit
static inline uint8_t spi_read_bit(void)
{
    uint8_t bit;
    
    gpio_set_level(MAX31855_SOFT_SCK_PIN, 1);
    ets_delay_us(SPI_DELAY_US);
    
    bit = gpio_get_level(MAX31855_SOFT_MISO_PIN);
    
    gpio_set_level(MAX31855_SOFT_SCK_PIN, 0);
    ets_delay_us(SPI_DELAY_US);
    
    return bit;
}

// Software SPI: Read 32 bits
static uint32_t spi_read_32bits(void)
{
    uint32_t data = 0;
    
    for (int i = 31; i >= 0; i--) {
        if (spi_read_bit()) {
            data |= (1UL << i);
        }
    }
    
    return data;
}

// CS control via I/O expander
static void max31855_cs_low(exio_pin_t cs_pin)
{
    io_expander_set_pin(cs_pin, false);
    ets_delay_us(10);
}

static void max31855_cs_high(exio_pin_t cs_pin)
{
    ets_delay_us(10);
    io_expander_set_pin(cs_pin, true);
}

// Parse 32-bit data from MAX31855
static void parse_max31855_data(uint32_t raw_data, max31855_data_t *data)
{
    if (raw_data & 0x00010000) {
        data->valid = false;
        data->fault = 0;
        
        if (raw_data & 0x00000001) data->fault |= MAX31855_FAULT_OPEN;
        if (raw_data & 0x00000002) data->fault |= MAX31855_FAULT_SHORT_GND;
        if (raw_data & 0x00000004) data->fault |= MAX31855_FAULT_SHORT_VCC;
        
        data->thermocouple_temp = 0.0;
        data->internal_temp = 0.0;
        return;
    }
    
    data->valid = true;
    data->fault = MAX31855_FAULT_NONE;
    
    int16_t tc_raw = (raw_data >> 18) & 0x3FFF;
    if (tc_raw & 0x2000) {
        tc_raw |= 0xC000;
    }
    data->thermocouple_temp = tc_raw * 0.25;
    
    int16_t int_raw = (raw_data >> 4) & 0x0FFF;
    if (int_raw & 0x0800) {
        int_raw |= 0xF000;
    }
    data->internal_temp = int_raw * 0.0625;
}

esp_err_t max31855_init(i2c_master_bus_handle_t i2c_bus,
                        max31855_handle_t *handle1, 
                        max31855_handle_t *handle2)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing MAX31855 with software SPI");
    
    // Initialize I/O expander using provided I2C bus
    ret = io_expander_init(i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I/O expander");
        return ret;
    }
    
    // Configure software SPI GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MAX31855_SOFT_SCK_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << MAX31855_SOFT_MISO_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    gpio_set_level(MAX31855_SOFT_SCK_PIN, 0);
    
    io_expander_set_pin(MAX31855_1_CS_PIN, true);
    io_expander_set_pin(MAX31855_2_CS_PIN, true);
    
    handle1->cs_pin = MAX31855_1_CS_PIN;
    handle1->chip_id = 1;
    
    handle2->cs_pin = MAX31855_2_CS_PIN;
    handle2->chip_id = 2;
    
    ESP_LOGI(TAG, "MAX31855_1 initialized on EXIO%d (Software SPI: MISO=GPIO%d, SCK=GPIO%d)", 
             MAX31855_1_CS_PIN, MAX31855_SOFT_MISO_PIN, MAX31855_SOFT_SCK_PIN);
    ESP_LOGI(TAG, "MAX31855_2 initialized on EXIO%d", MAX31855_2_CS_PIN);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    return ESP_OK;
}

esp_err_t max31855_read_temp(max31855_handle_t *handle, max31855_data_t *data)
{
    max31855_cs_low(handle->cs_pin);
    
    uint32_t raw_data = spi_read_32bits();
    
    max31855_cs_high(handle->cs_pin);
    
    parse_max31855_data(raw_data, data);
    
    if (!data->valid) {
        ESP_LOGW(TAG, "Chip %d fault: %s (raw=0x%08lX)", 
                 handle->chip_id, max31855_fault_string(data->fault), raw_data);
    } else {
        ESP_LOGD(TAG, "Chip %d: TC=%.2f°C, Internal=%.2f°C (raw=0x%08lX)", 
                 handle->chip_id, data->thermocouple_temp, 
                 data->internal_temp, raw_data);
    }
    
    return ESP_OK;
}

const char* max31855_fault_string(max31855_fault_t fault)
{
    switch (fault) {
        case MAX31855_FAULT_NONE:
            return "No fault";
        case MAX31855_FAULT_OPEN:
            return "Thermocouple open circuit";
        case MAX31855_FAULT_SHORT_GND:
            return "Short to ground";
        case MAX31855_FAULT_SHORT_VCC:
            return "Short to VCC";
        default:
            return "Multiple faults";
    }
}