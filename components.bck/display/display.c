#include <stdio.h>
#include "display.h"

/* SPI Master example - ST7789V White Screen Test for 170x320 display */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////// Hardware Configuration ///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LCD_HOST SPI2_HOST

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 4
#define PIN_NUM_CLK 5
#define PIN_NUM_CS 7

#define PIN_NUM_DC 6
#define PIN_NUM_RST 14
#define PIN_NUM_BCKL 15

#define LCD_BK_LIGHT_ON_LEVEL 0

// Display dimensions - 170x320
#define LCD_WIDTH 320
#define LCD_HEIGHT 240

// Some 170x320 displays are windows into a 240x320 panel
// You may need column offset (typically 35 for centered 170px in 240px)
#define LCD_COL_OFFSET 0
#define LCD_ROW_OFFSET 0

typedef struct
{
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;
} lcd_init_cmd_t;

static spi_device_handle_t spi;
char *TAG = "display";

DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    /* Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 (BGR mode) */
    {0x36, {(1 << 5) | (1 << 6) | (1 << 3)}, 1},
    /* Interface Pixel Format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Porch Setting */
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    /* Gate Control, Vgh=13.65V, Vgl=-10.43V */
    {0xB7, {0x45}, 1},
    /* VCOM Setting, VCOM=1.175V */
    {0xBB, {0x2B}, 1},
    /* LCM Control, XOR: BGR, MX, MH */
    {0xC0, {0x2C}, 1},
    /* VDV and VRH Command Enable, enable=1 */
    {0xC2, {0x01, 0xff}, 2},
    /* VRH Set, Vap=4.4+... */
    {0xC3, {0x11}, 1},
    /* VDV Set, VDV=0 */
    {0xC4, {0x20}, 1},
    /* Frame Rate Control, 60Hz, inversion=0 */
    {0xC6, {0x0f}, 1},
    /* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
    {0xD0, {0xA4, 0xA1}, 2},
    /* Positive Voltage Gamma Control */
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    /* Negative Voltage Gamma Control */
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    /* Sleep Out */
    {0x11, {0}, 0x80},
    /* Display On */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}};

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void *)0;
    if (keep_cs_active)
    {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;
    }
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0)
    {
        return;
    }
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void *)1;
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

uint32_t lcd_get_id(spi_device_handle_t spi)
{
    spi_device_acquire_bus(spi, portMAX_DELAY);
    lcd_cmd(spi, 0x04, true);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * 3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void *)1;

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);

    spi_device_release_bus(spi);
    return *(uint32_t *)t.rx_data;
}

esp_err_t display_init()
{
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 2 + 8};

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
        .pre_cb = lcd_spi_pre_transfer_callback,
    };

    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(LCD_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    int cmd = 0;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BCKL));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);

    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    uint32_t lcd_id = lcd_get_id(spi);
    ESP_LOGI(TAG, "LCD ID: %08" PRIx32 "\n", lcd_id);
    ESP_LOGI(TAG, "LCD ST7789V initialization (320x240).\n");

    while (st_init_cmds[cmd].databytes != 0xff)
    {
        lcd_cmd(spi, st_init_cmds[cmd].cmd, false);
        lcd_data(spi, st_init_cmds[cmd].data, st_init_cmds[cmd].databytes & 0x1F);
        if (st_init_cmds[cmd].databytes & 0x80)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        cmd++;
    }

    gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Backlight enabled.\n");

    lcd_cmd(spi, 0x21, false); // INVON command
    ESP_LOGI(TAG, "Color inversion enabled.\n");
    vTaskDelay(10 / portTICK_PERIOD_MS);

    return ESP_OK;
}

void fill_screen_color(spi_device_handle_t spi, uint16_t color)
{
    ESP_LOGI(TAG, "Filling screen with color 0x%04X...\n", color);

    // Set column address with offset
    lcd_cmd(spi, 0x2A, false);
    uint8_t col_data[] = {
        LCD_COL_OFFSET >> 8,                    // Start column high
        LCD_COL_OFFSET & 0xFF,                  // Start column low
        (LCD_COL_OFFSET + LCD_WIDTH - 1) >> 8,  // End column high
        (LCD_COL_OFFSET + LCD_WIDTH - 1) & 0xFF // End column low
    };
    lcd_data(spi, col_data, 4);
    ESP_LOGI(TAG, "Column range: %d to %d\n", LCD_COL_OFFSET, LCD_COL_OFFSET + LCD_WIDTH - 1);

    // Set row address with offset
    lcd_cmd(spi, 0x2B, false);
    uint8_t row_data[] = {
        LCD_ROW_OFFSET >> 8,                     // Start row high
        LCD_ROW_OFFSET & 0xFF,                   // Start row low
        (LCD_ROW_OFFSET + LCD_HEIGHT - 1) >> 8,  // End row high
        (LCD_ROW_OFFSET + LCD_HEIGHT - 1) & 0xFF // End row low
    };
    lcd_data(spi, row_data, 4);
    ESP_LOGI(TAG, "Row range: %d to %d\n", LCD_ROW_OFFSET, LCD_ROW_OFFSET + LCD_HEIGHT - 1);

    // Memory write command
    lcd_cmd(spi, 0x2C, false);

    // Allocate buffer for one line
    uint16_t *line_buffer = spi_bus_dma_memory_alloc(LCD_HOST, LCD_WIDTH * sizeof(uint16_t), 0);
    assert(line_buffer != NULL);

    // Fill buffer with color
    for (int i = 0; i < LCD_WIDTH; i++)
    {
        line_buffer[i] = color;
    }

    // Send pixels for entire screen
    for (int y = 0; y < LCD_HEIGHT; y++)
    {
        lcd_data(spi, (uint8_t *)line_buffer, LCD_WIDTH * 2);
    }

    free(line_buffer);
    ESP_LOGI(TAG, "Screen filled! Total pixels: %d\n", LCD_WIDTH * LCD_HEIGHT);
}

esp_err_t display_demo(void)
{
    fill_screen_color(spi, 0xFFFF); // White
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    fill_screen_color(spi, 0xF800); // Red
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    fill_screen_color(spi, 0x001F); // Green (sending blue value for BGR)
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    fill_screen_color(spi, 0x07E0); // Blue (sending green value for BGR)

    return ESP_OK;
}