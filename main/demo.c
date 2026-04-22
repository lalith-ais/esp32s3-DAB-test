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
#include "encoder.h"
#include "logo.h"   // declares: LV_IMG_DECLARE(logo)

static const char *TAG = "demo";

// Constants
#define TOUCH_POLL_MS          50
#define SPLASH_DURATION_MS     2500
#define COUNTER_MAX            10000
#define DOT_SIZE               12
#define DOT_OFFSET             (DOT_SIZE / 2)
#define DISPLAY_WIDTH          320
#define DISPLAY_HEIGHT         240

static lv_obj_t *lbl_counter;
static lv_obj_t *lbl_touch;     // shows live X, Y
static lv_obj_t *lbl_touch_dot; // small dot that follows the finger
static lv_obj_t *lbl_encoder;   // shows rotary encoder position

static lv_obj_t *test_screen   = NULL;
static lv_obj_t *splash_screen = NULL;

static lv_timer_t *splash_timer = NULL;

// Global touch handle for error checking
static esp_lcd_touch_handle_t global_tp = NULL;

// ─── Splash timer callback ────────────────────────────────────────────────────

static void splash_timer_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    ESP_LOGI(TAG, "Splash screen finished, loading test screen");
    // Fade in over 400 ms; last arg = true → auto-delete splash after transition
    lv_scr_load_anim(test_screen, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, true);
}


// ─── Splash screen ────────────────────────────────────────────────────────────

static void create_splash_screen(void)
{
    splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_black(), LV_STATE_DEFAULT);

    // The logo is 320×240 – exactly fills the display in landscape
    lv_obj_t *img = lv_img_create(splash_screen);
    lv_img_set_src(img, &logo);
    lv_obj_center(img);

    // Overlay text on top of the logo
    lv_obj_t *lbl = lv_label_create(splash_screen);
    lv_label_set_text(lbl, "ESP32 S3 FM / DAB / DAB+  Radio");

    // Style the text
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_STATE_DEFAULT);

    // Position it – e.g. near the bottom of the screen
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -16);
}


// ─── Test screen (original app_lvgl_main, now builds into its own screen) ────

static esp_err_t create_test_screen(void)
{
    test_screen = lv_obj_create(NULL);   // <-- own screen, not lv_scr_act()

    lvgl_port_lock(0);

    lv_obj_t *scr = test_screen;  // convenience alias

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
    lv_obj_align(lbl_touch, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Small dot that moves to the touch position
    lbl_touch_dot = lv_obj_create(scr);
    lv_obj_set_size(lbl_touch_dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(lbl_touch_dot, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lbl_touch_dot, lv_color_make(0xff, 0xff, 0x00), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lbl_touch_dot, 0, LV_STATE_DEFAULT);
    lv_obj_add_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN); // hidden until first touch

    // Encoder position readout
    lbl_encoder = lv_label_create(scr);
    lv_label_set_text(lbl_encoder, "Enc: 0");
    lv_obj_set_style_text_color(lbl_encoder, lv_color_make(0x00, 0xff, 0xff), LV_STATE_DEFAULT);
    lv_obj_align(lbl_encoder, LV_ALIGN_BOTTOM_LEFT, 8, -8);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Test screen created successfully");
    return ESP_OK;
}

// ─── LVGL update task ─────────────────────────────────────────────────────────

static void lvgl_update_task(void *arg)
{
    char buf[32];
    uint16_t n = 0;
    bool last_touch_state = false;
    
    // For encoder click action tracking
    bool last_sw_state = false;
    
    ESP_LOGI(TAG, "LVGL update task started");

    while (1)
    {
        // --- read raw touch data (only if touch is available) ---
        bool touch_available = (global_tp != NULL);
        uint8_t point_cnt = 0;
        esp_err_t err = ESP_FAIL;
        
        if (touch_available)
        {
            esp_lcd_touch_point_data_t touch_points[1];  // Array to store touch points
            uint8_t max_points = 1;
            
            esp_lcd_touch_read_data(global_tp);  // Still need to read data first
            err = esp_lcd_touch_get_data(global_tp, touch_points, &point_cnt, max_points);
            
            // Update UI based on touch state
            if (lvgl_port_lock(0))
            {
                if (err == ESP_OK && point_cnt > 0)
                {
                    int x = touch_points[0].x;
                    int y = touch_points[0].y;
                    
                    // Validate coordinates
                    if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT)
                    {
                        if (!last_touch_state)
                        {
                            ESP_LOGD(TAG, "Touch started at (%d, %d)", x, y);
                            last_touch_state = true;
                        }
                        
                        sprintf(buf, "Touch: %3d, %3d", x, y);
                        lv_label_set_text(lbl_touch, buf);
                        lv_obj_align(lbl_touch, LV_ALIGN_BOTTOM_MID, 0, -8);
                        
                        // Move the dot to the finger position
                        lv_obj_clear_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_set_pos(lbl_touch_dot, x - DOT_OFFSET, y - DOT_OFFSET);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Invalid touch coordinates: (%d, %d)", x, y);
                        lv_label_set_text(lbl_touch, "Touch: ---");
                        lv_obj_add_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN);
                    }
                }
                else
                {
                    if (last_touch_state)
                    {
                        ESP_LOGD(TAG, "Touch ended");
                        last_touch_state = false;
                    }
                    lv_label_set_text(lbl_touch, "Touch: ---");
                    lv_obj_add_flag(lbl_touch_dot, LV_OBJ_FLAG_HIDDEN);
                }
                lvgl_port_unlock();
            }
        }

        // --- read encoder ---
        int enc_pos = encoder_get_position();
        bool sw_pressed = encoder_sw_pressed();

        // --- update LVGL labels under lock ---
        if (lvgl_port_lock(0))
        {
            // Counter with overflow protection
            sprintf(buf, "%04d", n);
            lv_label_set_text(lbl_counter, buf);
            n = (n + 1) % COUNTER_MAX;

            // Encoder position with click detection
            if (sw_pressed)
            {
                sprintf(buf, "Enc: %d [CLICK]", enc_pos);
                // Optional: Add action when encoder button is pressed
                if (!last_sw_state)
                {
                    ESP_LOGI(TAG, "Encoder button pressed at position %d", enc_pos);
                    // Reset counter on click (example action)
                    n = 0;
                    last_sw_state = true;
                }
            }
            else
            {
                sprintf(buf, "Enc: %d", enc_pos);
                last_sw_state = false;
            }
            lv_label_set_text(lbl_encoder, buf);

            lvgl_port_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_MS));
    }
}

// ─── app_main ─────────────────────────────────────────────────────────────────

void app_main(void)
{
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t    lcd_panel;
    lvgl_port_touch_cfg_t     touch_cfg;
    lv_display_t             *lvgl_display = NULL;
    
    // Initialize display brightness
    ESP_ERROR_CHECK(lcd_display_brightness_init());

    // Initialize LCD panel
    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));
    
    // Initialize LVGL
    lvgl_display = app_lvgl_init(lcd_io, lcd_panel);
    if (lvgl_display == NULL)
    {
        ESP_LOGE(TAG, "Fatal error in app_lvgl_init");
        esp_restart();
    }
    ESP_LOGI(TAG, "LVGL initialized successfully");

    // Initialize touch (with error recovery)
    esp_err_t touch_err = touch_init(&global_tp);
    if (touch_err != ESP_OK || global_tp == NULL)
    {
        ESP_LOGW(TAG, "Touch not available (err=%d), continuing without touch", touch_err);
        global_tp = NULL;
    }
    else
    {
        ESP_LOGI(TAG, "Touch initialized successfully");
        touch_cfg.disp    = lvgl_display;
        touch_cfg.handle  = global_tp;
        touch_cfg.scale.x = 0;
        touch_cfg.scale.y = 0;
        lvgl_port_add_touch(&touch_cfg);
    }

    // Initialize encoder
    ESP_ERROR_CHECK(encoder_init());
    ESP_LOGI(TAG, "Encoder initialized, initial position: %d", encoder_get_position());

    // Set display brightness and rotation
    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_270));

    // Build test screen first (hidden), then show splash on top
    ESP_ERROR_CHECK(create_test_screen());

    lvgl_port_lock(0);
    create_splash_screen();
    lv_scr_load(splash_screen);                    // show splash immediately
    splash_timer = lv_timer_create(splash_timer_cb, SPLASH_DURATION_MS, NULL);
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Splash screen displayed for %d ms", SPLASH_DURATION_MS);

    // Create LVGL update task
    xTaskCreate(lvgl_update_task, "lvgl_update", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "LVGL update task created");

    // Main loop just waits (all work is done in tasks)
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Should never reach here
    vTaskDelay(portMAX_DELAY);
}