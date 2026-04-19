#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lcd_panel_io.h>
#include <esp_check.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#include "hardware.h"
#include "touch.h"

static const char *TAG = "touch";

esp_err_t touch_init(esp_lcd_touch_handle_t *tp)
{
    ESP_LOGI(TAG, "Initialising FT6336 touch over I2C");

    // Init I2C bus
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t i2c_bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus), TAG, "I2C bus init failed");

    // Create panel IO over I2C
    esp_lcd_panel_io_handle_t tp_io;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_cfg.dev_addr = TOUCH_I2C_ADDR;

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io),
        TAG, "Touch panel IO init failed");

    // Configure the touch driver
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_RST,
        .int_gpio_num = TOUCH_IRQ,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = true,
            .mirror_x = TOUCH_MIRROR_X,
            .mirror_y = TOUCH_MIRROR_Y,
        },
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, tp),
        TAG, "FT5x06/FT6336 touch driver init failed");

    ESP_LOGI(TAG, "FT6336 touch initialised OK");
    return ESP_OK;
}
