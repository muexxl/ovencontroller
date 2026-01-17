#ifndef TOUCH_H
#define TOUCH_H

#include "esp_err.h"
#include "lvgl.h"
#include "driver/i2c_master.h"

// Touch controller configuration
#define I2C_MASTER_SCL_IO      8
#define I2C_MASTER_SDA_IO      18
#define I2C_MASTER_NUM         I2C_NUM_0
#define I2C_MASTER_FREQ_HZ     400000

// Function declarations
esp_err_t touch_init(void);
void touch_lvgl_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
i2c_master_bus_handle_t touch_get_i2c_bus(void);  // Add this to share bus

#endif // TOUCH_H