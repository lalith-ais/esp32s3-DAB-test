#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

#include <esp_lcd_ili9341.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "hardware.h"

static const char *TAG = "lcd";

// Corrected ILI9341 initialization sequence based on TFT_eSPI
static const ili9341_lcd_init_cmd_t ili9341_init_cmds[] = {
    // Command, { data }, data_size, delay_ms
    {0xEF, (uint8_t []){0x03, 0x80, 0x02}, 3, 0},
    {0xCF, (uint8_t []){0x00, 0xC1, 0x30}, 3, 0},
    {0xED, (uint8_t []){0x64, 0x03, 0x12, 0x81}, 4, 0},
    {0xE8, (uint8_t []){0x85, 0x00, 0x78}, 3, 0},
    {0xCB, (uint8_t []){0x39, 0x2C, 0x00, 0x34, 0x02}, 5, 0},
    {0xF7, (uint8_t []){0x20}, 1, 0},
    {0xEA, (uint8_t []){0x00, 0x00}, 2, 0},
    {0xC0, (uint8_t []){0x23}, 1, 0},       // Power control 1
    {0xC1, (uint8_t []){0x10}, 1, 0},       // Power control 2
    {0xC5, (uint8_t []){0x3E, 0x28}, 2, 0}, // VCM control 1
    {0xC7, (uint8_t []){0x80}, 1, 0},       // VCM control 2
    {0xB1, (uint8_t []){0x00, 0x13}, 2, 0}, // Frame rate 100Hz
    {0xB6, (uint8_t []){0x08, 0x82, 0x27}, 3, 0}, // Display function
    {0xF2, (uint8_t []){0x00}, 1, 0},       // 3Gamma disable
    {0x26, (uint8_t []){0x01}, 1, 0},       // Gamma set
    {0xE0, (uint8_t []){0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15, 0},
};


esp_err_t lcd_display_rotate(lv_display_t *lvgl_disp, lv_display_rotation_t dir)
{
    if (lvgl_disp) {
        lv_display_set_rotation(lvgl_disp, dir);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t app_lcd_init(esp_lcd_panel_io_handle_t *lcd_io, esp_lcd_panel_handle_t *lcd_panel)
{
    const spi_bus_config_t buscfg = {
        .mosi_io_num      = LCD_SPI_MOSI,
        .miso_io_num      = LCD_SPI_MISO,
        .sclk_io_num      = LCD_SPI_CLK,
        .quadhd_io_num    = GPIO_NUM_NC,
        .quadwp_io_num    = GPIO_NUM_NC,
        .max_transfer_sz  = LCD_DRAWBUF_SIZE * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num        = LCD_CS,
        .dc_gpio_num        = LCD_DC,
        .spi_mode           = 0,
        .pclk_hz            = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth  = 10,
        .lcd_cmd_bits       = LCD_CMD_BITS,
        .lcd_param_bits     = LCD_PARAM_BITS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, lcd_io), TAG, "LCD new panel io failed");

    // Setup custom init commands (TFT_eSPI-based corrected sequence)
    ili9341_vendor_config_t vendor_config = {
        .init_cmds = ili9341_init_cmds,
        .init_cmds_size = sizeof(ili9341_init_cmds) / sizeof(ili9341_lcd_init_cmd_t),
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num  = LCD_RESET,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel  = LCD_BITS_PIXEL,
        .vendor_config   = &vendor_config,  // Pass our corrected init sequence
    };

    esp_err_t r = esp_lcd_new_panel_ili9341(*lcd_io, &panel_config, lcd_panel);

    esp_lcd_panel_reset(*lcd_panel);
    esp_lcd_panel_init(*lcd_panel);
    esp_lcd_panel_mirror(*lcd_panel, LCD_MIRROR_X, LCD_MIRROR_Y);
    esp_lcd_panel_invert_color(*lcd_panel, true);
    esp_lcd_panel_disp_on_off(*lcd_panel, true);

    return r;
}


lv_display_t *app_lvgl_init(esp_lcd_panel_io_handle_t lcd_io, esp_lcd_panel_handle_t lcd_panel)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority    = 4,
        .task_stack       = 8192,
        .task_affinity    = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms  = 5,
    };

    esp_err_t e = lvgl_port_init(&lvgl_cfg);
    if (e != ESP_OK) {
        ESP_LOGI(TAG, "lvgl_port_init() failed: %s", esp_err_to_name(e));
        return NULL;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size  = LCD_DRAWBUF_SIZE,
        .double_buffer = LCD_DOUBLE_BUFFER,
        .hres         = LCD_H_RES,
        .vres         = LCD_V_RES,
        .monochrome   = false,
        .rotation = {
            .swap_xy  = false,
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
        },
        .flags = {
            .buff_dma   = true,
            .buff_spiram = false,
            .swap_bytes = true,
        },
    };

    return lvgl_port_add_disp(&disp_cfg);
}
