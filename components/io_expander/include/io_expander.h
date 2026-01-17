#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdint.h>

// I2C configuration
#define IO_EXPANDER_ADDR    0x20

// EXIO pin definitions
typedef enum {
    EXIO0 = 0,
    EXIO1 = 1,
    EXIO2 = 2,
    EXIO3 = 3,
    EXIO4 = 4,
    EXIO5 = 5,
    EXIO6 = 6,
    EXIO7 = 7,
} exio_pin_t;

// Pin assignments
#define MAX31855_1_CS_PIN   EXIO0
#define MAX31855_2_CS_PIN   EXIO1
#define HEATER_1_PIN        EXIO2
#define HEATER_2_PIN        EXIO3
#define FAN_PIN             EXIO4

// Function declarations
esp_err_t io_expander_init(i2c_master_bus_handle_t i2c_bus);  // Now takes bus handle
esp_err_t io_expander_set_pin(exio_pin_t pin, bool level);
esp_err_t io_expander_get_pin(exio_pin_t pin, bool *level);
esp_err_t io_expander_write_all(uint8_t value);
esp_err_t io_expander_read_all(uint8_t *value);

#endif // IO_EXPANDER_H