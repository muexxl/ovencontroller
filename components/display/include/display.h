#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_err.h"
#include "driver/spi_master.h"
#include "lvgl.h"

// Display configuration
#define LCD_HOST    SPI2_HOST
#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 4
#define PIN_NUM_CLK 5
#define PIN_NUM_CS 7

#define PIN_NUM_DC 6
#define PIN_NUM_RST 14
#define PIN_NUM_BCKL 15

#define LCD_BK_LIGHT_ON_LEVEL   0
#define LCD_WIDTH  320
#define LCD_HEIGHT 170
#define LCD_COL_OFFSET 0
#define LCD_ROW_OFFSET 35

// Function declarations
esp_err_t display_init(void);
void display_lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void display_lvgl_set_backlight(uint8_t brightness);

#endif // DISPLAY_H
