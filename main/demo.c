#include <stdio.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>
#include <esp_lcd_touch.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "lcd.h"
#include "touch.h"

static const char *TAG = "demo";

static lv_obj_t *lbl_counter;
static lv_obj_t *lbl_touch;     // shows live X, Y
static lv_obj_t *lbl_touch_dot; // small dot that follows the finger


static esp_err_t app_lvgl_main(void)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_port_lock(0);

    // Background
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_STATE_DEFAULT);

    // Colour-check labels
    lv_obj_t *labelR = lv_label_create(scr);
    lv_label_set_text(labelR, "Red");
    lv_obj_set_style_text_color(labelR, lv_color_make(0xff, 0, 0), LV_STATE_DEFAULT);
    lv_obj_align(labelR, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *labelG = lv_label_create(scr);
    lv_label_set_text(labelG, "Green");
    lv_obj_set_style_text_color(labelG, lv_color_make(0, 0xff, 0), LV_STATE_DEFAULT);
    lv_obj_align(labelG, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *labelB = lv_label_create(scr);
    lv_label_set_text(labelB, "Blue");
    lv_obj_set_style_text_color(labelB, lv_color_make(0, 0, 0xff), LV_STATE_DEFAULT);
    lv_obj_align(labelB, LV_ALIGN_TOP_MID, 0, 48);

    // Counter button (still tappable)
    lv_obj_t *btn_counter = lv_button_create(scr);
    lv_obj_align(btn_counter, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(btn_counter, 120, 50);

    lbl_counter = lv_label_create(btn_counter);
    lv_label_set_text(lbl_counter, "0000");
    lv_obj_set_style_text_color(lbl_counter, lv_color_make(248, 11, 181), LV_STATE_DEFAULT);
    lv_obj_align(lbl_counter, LV_ALIGN_CENTER, 0, 0);

    // Touch coordinate readout at bottom
    lbl_touch = lv_label_create(scr);
    lv_label_set_text(lbl_touch, "Touch: ---");
    lv_obj_set_style_text_color(lbl_touch, lv_color_make(0xff, 0xff, 0x00), LV_STATE_DEFAULT);
    lv_obj_align(lbl_touch, LV_ALIGN_BOTTOM_LEFT, 0, -8);

    // Small dot that moves to the touch position
    lbl_touch_dot = lv_obj_create(scr);
    lv_obj_set_size(lbl_touch_dot, 12, 12);
    lv_obj_set_style_radius(lbl_touch_dot, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lbl_touch_dot, lv_color_make(0xff, 0xff, 0x00), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lbl_touch_dot, 0, LV_STATE_DEFAULT);
    lv_obj_add_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN); // hidden until first touch

    lvgl_port_unlock();

    return ESP_OK;
}


void app_main(void)
{
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t    lcd_panel;
    esp_lcd_touch_handle_t    tp;
    lvgl_port_touch_cfg_t     touch_cfg;
    lv_display_t             *lvgl_display = NULL;
    char buf[32];
    uint16_t n = 0;

    ESP_ERROR_CHECK(lcd_display_brightness_init());

    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));
    lvgl_display = app_lvgl_init(lcd_io, lcd_panel);
    if (lvgl_display == NULL)
    {
        ESP_LOGI(TAG, "fatal error in app_lvgl_init");
        esp_restart();
    }

    ESP_ERROR_CHECK(touch_init(&tp));
    touch_cfg.disp    = lvgl_display;
    touch_cfg.handle  = tp;
    touch_cfg.scale.x = 0;
    touch_cfg.scale.y = 0;
    lvgl_port_add_touch(&touch_cfg);

    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_270));
    ESP_ERROR_CHECK(app_lvgl_main());

    while (42)
    {
        // --- read raw touch data ---
        uint16_t touch_x[1], touch_y[1], touch_strength[1];
        uint8_t  touch_cnt = 0;

        esp_lcd_touch_read_data(tp);
        bool touched = esp_lcd_touch_get_coordinates(
            tp, touch_x, touch_y, touch_strength, &touch_cnt, 1);

        // --- update LVGL labels under lock ---
        if (lvgl_port_lock(0))
        {
            // counter ticks regardless
            sprintf(buf, "%04d", n++);
            lv_label_set_text(lbl_counter, buf);

            if (touched && touch_cnt > 0)
            {
                sprintf(buf, "Touch: %3d, %3d", touch_x[0], touch_y[0]);
                lv_label_set_text(lbl_touch, buf);
                lv_obj_align(lbl_touch, LV_ALIGN_BOTTOM_LEFT, 0, -8);

                // Move the dot to the finger position
                lv_obj_clear_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(lbl_touch_dot, touch_x[0] - 6, touch_y[0] - 6);
            }
            else
            {
                lv_label_set_text(lbl_touch, "Touch: ---");
                lv_obj_add_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN);
            }

            lvgl_port_unlock();
        }

        vTaskDelay(50 / portTICK_PERIOD_MS); // ~20 Hz poll
    }

    vTaskDelay(portMAX_DELAY);
}
