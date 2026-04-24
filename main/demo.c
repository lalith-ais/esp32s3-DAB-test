// demo.c - Clean version without unused timer
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "lcd.h"
#include "encoder.h"
#include "logo.h"
#include "test_screen.h"
#include "hardware.h"
#include "driver/gpio.h"

static const char *TAG = "demo";

static lv_obj_t *splash_screen = NULL;

// ─── Splash timer callback ────────────────────────────────────────────────────
static void splash_timer_cb(lv_timer_t *timer) {
    lv_timer_del(timer);
    lv_scr_load_anim(test_screen, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, true);
}

// ─── Splash screen ────────────────────────────────────────────────────────────
static void create_splash_screen(void) {
    splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_t *img = lv_img_create(splash_screen);
    lv_img_set_src(img, &logo);
    lv_obj_center(img);

    lv_obj_t *lbl = lv_label_create(splash_screen);
    lv_label_set_text(lbl, "ESP32 S3 FM / DAB / DAB+  Radio");
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -16);
}

void app_main(void) {
    // LCD Reset sequence
    gpio_set_direction(LCD_RESET, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LCD_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Initialize LCD and LVGL
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;
    lv_display_t *lvgl_display = NULL;

    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));
    lvgl_display = app_lvgl_init(lcd_io, lcd_panel);
    if (lvgl_display == NULL) {
        ESP_LOGI(TAG, "fatal error in app_lvgl_init");
        esp_restart();
    }

    ESP_ERROR_CHECK(encoder_init());
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_270));

    // Create test screen
    create_test_screen();

    lvgl_port_lock(0);
    create_splash_screen();
    lv_scr_load(splash_screen);
    lv_timer_create(splash_timer_cb, 2500, NULL);
    lvgl_port_unlock();

    // Initialize test data
    update_test_signal_strength(75);
    update_test_play_status(true);
    update_test_encoder(0);

    // Main loop
    int32_t last_encoder_pos = 0;
    
    for (;;) {
        int32_t enc_pos = encoder_get_position();
        bool sw_pressed = encoder_sw_pressed();

        if (lvgl_port_lock(0)) {
            // Update encoder if changed
            if (enc_pos != last_encoder_pos) {
                last_encoder_pos = enc_pos;
                update_test_encoder(enc_pos);
            }
            
            // Handle button press
            if (sw_pressed) {
                static bool playing = true;
                playing = !playing;
                update_test_play_status(playing);
                
                // Also cycle signal strength for demo
                static uint8_t signal = 75;
                signal = (signal + 15) % 100;
                update_test_signal_strength(signal);
            }
            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}