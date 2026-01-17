#ifndef MAX31855_H
#define MAX31855_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"  // Add this
#include "io_expander.h"

// Software SPI Pin configuration - using available header pins
#define MAX31855_SOFT_MISO_PIN  3
#define MAX31855_SOFT_SCK_PIN   23

// Fault codes
typedef enum {
    MAX31855_FAULT_NONE = 0,
    MAX31855_FAULT_OPEN = 1,
    MAX31855_FAULT_SHORT_GND = 2,
    MAX31855_FAULT_SHORT_VCC = 4,
} max31855_fault_t;

// Temperature data structure
typedef struct {
    float thermocouple_temp;
    float internal_temp;
    max31855_fault_t fault;
    bool valid;
} max31855_data_t;

// Handle for each MAX31855
typedef struct {
    exio_pin_t cs_pin;
    uint8_t chip_id;
} max31855_handle_t;

// Function declarations
esp_err_t max31855_init(i2c_master_bus_handle_t i2c_bus, 
                        max31855_handle_t *handle1, 
                        max31855_handle_t *handle2);  // Changed signature
esp_err_t max31855_read_temp(max31855_handle_t *handle, max31855_data_t *data);
const char* max31855_fault_string(max31855_fault_t fault);

#endif // MAX31855_H