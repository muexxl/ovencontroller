#include "display.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";
static spi_device_handle_t spi_handle;

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;
} lcd_init_cmd_t;

DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    {0x36, {(1 << 5) | (1 << 6)}, 1},  // Memory Data Access Control (BGR mode)
    {0x3A, {0x55}, 1},                  // 16-bit color
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x45}, 1},
    {0xBB, {0x2B}, 1},
    {0xC0, {0x2C}, 1},
    {0xC2, {0x01, 0xff}, 2},
    {0xC3, {0x11}, 1},
    {0xC4, {0x20}, 1},
    {0xC6, {0x0f}, 1},
    {0xD0, {0xA4, 0xA1}, 2},
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    {0x11, {0}, 0x80},  // Sleep out
    {0x29, {0}, 0x80},  // Display on
    {0, {0}, 0xff}
};

static void lcd_cmd(const uint8_t cmd, bool keep_cs_active)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0;
    if (keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;
    }
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &t));
}

static void lcd_data(const uint8_t *data, int len)
{
    if (len == 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void*)1;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_handle, &t));
}

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

esp_err_t display_init(void)
{
    esp_err_t ret;
    
    // Configure GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BCKL)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = true,
    };
    gpio_config(&io_conf);
    
    // SPI bus configuration
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 2 * 20,  // Buffer for 20 lines
    };
    
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  // 40 MHz
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
        .pre_cb = lcd_spi_pre_transfer_callback,
    };
    
    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    
    ret = spi_bus_add_device(LCD_HOST, &devcfg, &spi_handle);
    ESP_ERROR_CHECK(ret);
    
    // Reset display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Send init commands
    int cmd = 0;
    while (st_init_cmds[cmd].databytes != 0xff) {
        lcd_cmd(st_init_cmds[cmd].cmd, false);
        lcd_data(st_init_cmds[cmd].data, st_init_cmds[cmd].databytes & 0x1F);
        if (st_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        cmd++;
    }
    
    // Enable color inversion
    lcd_cmd(0x21, false);
    
    // Turn on backlight
    gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON_LEVEL);
    
    ESP_LOGI(TAG, "Display initialized");
    return ESP_OK;
}

// LVGL flush callback - sends framebuffer to display
void display_lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    
    // Set column address
    lcd_cmd(0x2A, false);
    uint8_t col_data[] = {
        (LCD_COL_OFFSET + area->x1) >> 8,
        (LCD_COL_OFFSET + area->x1) & 0xFF,
        (LCD_COL_OFFSET + area->x2) >> 8,
        (LCD_COL_OFFSET + area->x2) & 0xFF
    };
    lcd_data(col_data, 4);
    
    // Set row address
    lcd_cmd(0x2B, false);
    uint8_t row_data[] = {
        (LCD_ROW_OFFSET + area->y1) >> 8,
        (LCD_ROW_OFFSET + area->y1) & 0xFF,
        (LCD_ROW_OFFSET + area->y2) >> 8,
        (LCD_ROW_OFFSET + area->y2) & 0xFF
    };
    lcd_data(row_data, 4);
    
    // Write pixels
    lcd_cmd(0x2C, false);
    lcd_data((uint8_t*)color_map, size * 2);
    
    lv_disp_flush_ready(drv);
}

void display_lvgl_set_backlight(uint8_t brightness)
{
    // Simple on/off for now (could use PWM for dimming)
    gpio_set_level(PIN_NUM_BCKL, brightness > 0 ? LCD_BK_LIGHT_ON_LEVEL : !LCD_BK_LIGHT_ON_LEVEL);
}
