// test_screen.c - Based on your SVG layout
#include "test_screen.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "test_screen";

// Professional color palette
#define COLOR_BG_MAIN       0x101218
#define COLOR_BG_CARD       0x1A1D28
#define COLOR_TEXT_PRIMARY  0xFFFFFF
#define COLOR_TEXT_SECONDARY 0xC8D0E0
#define COLOR_ACCENT_CYAN   0x00BCD4
#define COLOR_STATUS_GREEN  0x4CAF50
#define COLOR_STATUS_AMBER  0xFFC107
#define COLOR_STATUS_RED    0xF44336
#define COLOR_SIGNAL_STRONG 0x4CAF50
#define COLOR_SIGNAL_WEAK   0xFFC107
#define COLOR_SIGNAL_POOR   0xF44336
#define COLOR_VU_LEFT       0x00BCD4  // Cyan for left channel
#define COLOR_VU_RIGHT      0x4CAF50  // Green for right channel

lv_obj_t *test_screen = NULL;

// UI element handles
static lv_obj_t *img_logo = NULL;
static lv_obj_t *label_date = NULL;
static lv_obj_t *label_time = NULL;
static lv_obj_t *label_ensemble = NULL;
static lv_obj_t *label_program_name = NULL;
static lv_obj_t *label_program_text = NULL;
static lv_obj_t *led_status = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *label_channel = NULL;
static lv_obj_t *label_bitrate = NULL;
static lv_obj_t *label_mode = NULL;
static lv_obj_t *label_stereo = NULL;
static lv_obj_t *label_signal = NULL;
static lv_obj_t *label_encoder = NULL;
static lv_obj_t *vu_left_bar = NULL;
static lv_obj_t *vu_right_bar = NULL;

// For scrolling text (marquee effect)
static lv_anim_t scroll_anim;
static bool text_scrolling = false;

// Function to update signal strength
void update_test_signal_strength(uint8_t rssi) {
    if(!label_signal) return;
    
    lv_color_t color;
    if(rssi >= 70) color = lv_color_hex(COLOR_SIGNAL_STRONG);
    else if(rssi >= 30) color = lv_color_hex(COLOR_SIGNAL_WEAK);
    else color = lv_color_hex(COLOR_SIGNAL_POOR);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "SIG: %d%%", rssi);
    lv_label_set_text(label_signal, buf);
    lv_obj_set_style_text_color(label_signal, color, LV_STATE_DEFAULT);
}

// Function to update play status
void update_test_play_status(bool is_playing) {
    if(!led_status) return;
    
    if(is_playing) {
        lv_led_set_color(led_status, lv_color_hex(COLOR_STATUS_GREEN));
        lv_led_on(led_status);
        lv_label_set_text(label_status, "PLAY");
        lv_obj_set_style_text_color(label_status, lv_color_hex(COLOR_STATUS_GREEN), 0);
    } else {
        lv_led_set_color(led_status, lv_color_hex(COLOR_STATUS_AMBER));
        lv_led_on(led_status);
        lv_label_set_text(label_status, "STOP");
        lv_obj_set_style_text_color(label_status, lv_color_hex(COLOR_STATUS_AMBER), 0);
    }
}

// Function to update encoder position
void update_test_encoder(int32_t position) {
    if(!label_encoder) return;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "ENC: %" PRIi32, position);
    lv_label_set_text(label_encoder, buf);
}

// Function to update VU meter (0-255 for each channel)
void update_vu_meter(uint8_t left_level, uint8_t right_level) {
    if(vu_left_bar) {
        lv_bar_set_value(vu_left_bar, left_level, LV_ANIM_OFF);
    }
    if(vu_right_bar) {
        lv_bar_set_value(vu_right_bar, right_level, LV_ANIM_OFF);
    }
}

// Function to set scrolling text for program text (when too long)
static void set_scrolling_text(lv_obj_t *label, const char *text, int max_width) {
    lv_label_set_text(label, text);
    lv_obj_update_layout(label);
    
    int text_width = lv_obj_get_width(label);
    
    if(text_width > max_width && !text_scrolling) {
        // TODO: Implement marquee scrolling for long text
        // For now, just truncate with ellipsis
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    } else {
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
}

// Create the professional test screen based on SVG layout
void create_test_screen(void) {
    ESP_LOGI(TAG, "Creating professional test screen from SVG layout");
    
    lvgl_port_lock(0);
    
    // Create screen
    test_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(test_screen, lv_color_hex(COLOR_BG_MAIN), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(test_screen, 0, 0);
    lv_obj_set_style_pad_all(test_screen, 0, 0);
    
    lv_obj_t *scr = test_screen;
    
    // ========== ROW 1 (y=0) ==========
    
    // Logo (0,0) 80×50
    img_logo = lv_img_create(scr);
    // lv_img_set_src(img_logo, &logo);  // Uncomment when you have logo
    lv_obj_set_pos(img_logo, 0, 0);
    lv_obj_set_size(img_logo, 80, 50);
    lv_obj_set_style_bg_color(img_logo, lv_color_hex(COLOR_BG_CARD), 0);
    
    // Date (80,0) 160×30
    label_date = lv_label_create(scr);
    lv_label_set_text(label_date, "2024-01-15");
    lv_obj_set_style_text_color(label_date, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_date, 80, 5);
    lv_obj_set_size(label_date, 160, 30);
    
    // Time (240,0) 80×50
    label_time = lv_label_create(scr);
    lv_label_set_text(label_time, "12:34");
    lv_obj_set_style_text_color(label_time, lv_color_hex(COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(label_time, 245, 10);
    lv_obj_set_size(label_time, 80, 50);
    
    // ========== ROW 2 (y=30) - Ensemble centered ==========
    label_ensemble = lv_label_create(scr);
    lv_label_set_text(label_ensemble, "BBC National DAB");
    lv_obj_set_style_text_color(label_ensemble, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_ensemble, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_ensemble, 80, 30);
    lv_obj_set_size(label_ensemble, 160, 20);
    lv_obj_set_style_text_align(label_ensemble, LV_TEXT_ALIGN_CENTER, 0);
    
    // ========== ROW 3 (y=50) - Program Name ==========
    label_program_name = lv_label_create(scr);
    lv_label_set_text(label_program_name, "BBC RADIO 4");
    lv_obj_set_style_text_color(label_program_name, lv_color_hex(COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label_program_name, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(label_program_name, 0, 50);
    lv_obj_set_size(label_program_name, 320, 40);
    lv_obj_set_style_text_align(label_program_name, LV_TEXT_ALIGN_CENTER, 0);
    
    // ========== ROW 4 (y=90) - Program Text (scrolling) ==========
    label_program_text = lv_label_create(scr);
    lv_label_set_text(label_program_text, "Now Playing: Morning Classics - Beethoven Symphony No. 5");
    lv_obj_set_style_text_color(label_program_text, lv_color_hex(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(label_program_text, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(label_program_text, 0, 90);
    lv_obj_set_size(label_program_text, 320, 80);
    lv_obj_set_style_text_align(label_program_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label_program_text, LV_LABEL_LONG_WRAP);
    
    // ========== ROW 5 (y=170) - Status Row ==========
    // Format: [LED][STATUS] [CH][BR][MODE][STEREO] [SIG] [ENC]
    
    // Status LED
    led_status = lv_led_create(scr);
    lv_obj_set_size(led_status, 12, 12);
    lv_obj_set_pos(led_status, 5, 174);
    
    // Status text
    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "PLAY");
    lv_obj_set_style_text_color(label_status, lv_color_hex(COLOR_STATUS_GREEN), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_status, 22, 174);
    
    // Channel
    label_channel = lv_label_create(scr);
    lv_label_set_text(label_channel, "CH:12B");
    lv_obj_set_style_text_color(label_channel, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_channel, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_channel, 65, 174);
    
    // Bitrate
    label_bitrate = lv_label_create(scr);
    lv_label_set_text(label_bitrate, "128k");
    lv_obj_set_style_text_color(label_bitrate, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_bitrate, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_bitrate, 118, 174);
    
    // Mode (DAB/FM)
    label_mode = lv_label_create(scr);
    lv_label_set_text(label_mode, "DAB");
    lv_obj_set_style_text_color(label_mode, lv_color_hex(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(label_mode, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_mode, 165, 174);
    
    // Stereo indicator
    label_stereo = lv_label_create(scr);
    lv_label_set_text(label_stereo, "STEREO");
    lv_obj_set_style_text_color(label_stereo, lv_color_hex(COLOR_STATUS_GREEN), 0);
    lv_obj_set_style_text_font(label_stereo, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_stereo, 200, 174);
    
    // Signal strength
    label_signal = lv_label_create(scr);
    lv_label_set_text(label_signal, "SIG:75%");
    lv_obj_set_style_text_color(label_signal, lv_color_hex(COLOR_SIGNAL_STRONG), 0);
    lv_obj_set_style_text_font(label_signal, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_signal, 255, 174);
    
    // Encoder position (right-aligned)
    label_encoder = lv_label_create(scr);
    lv_label_set_text(label_encoder, "ENC:0");
    lv_obj_set_style_text_color(label_encoder, lv_color_hex(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(label_encoder, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_encoder, 280, 174);
    
    // ========== ROW 6 (y=200) - VU Meter ==========
    // Left VU bar
    vu_left_bar = lv_bar_create(scr);
    lv_obj_set_size(vu_left_bar, 150, 30);
    lv_obj_set_pos(vu_left_bar, 5, 205);
    lv_bar_set_range(vu_left_bar, 0, 255);
    lv_bar_set_value(vu_left_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(vu_left_bar, lv_color_hex(COLOR_BG_CARD), 0);
    lv_obj_set_style_anim_duration(vu_left_bar, 50, 0);  // Fast response
    
    // Right VU bar
    vu_right_bar = lv_bar_create(scr);
    lv_obj_set_size(vu_right_bar, 150, 30);
    lv_obj_set_pos(vu_right_bar, 165, 205);
    lv_bar_set_range(vu_right_bar, 0, 255);
    lv_bar_set_value(vu_right_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(vu_right_bar, lv_color_hex(COLOR_BG_CARD), 0);
    lv_obj_set_style_anim_duration(vu_right_bar, 50, 0);
    
    // Add labels for L/R
    lv_obj_t *label_l = lv_label_create(scr);
    lv_label_set_text(label_l, "L");
    lv_obj_set_style_text_color(label_l, lv_color_hex(COLOR_VU_LEFT), 0);
    lv_obj_set_style_text_font(label_l, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_l, 8, 210);
    
    lv_obj_t *label_r = lv_label_create(scr);
    lv_label_set_text(label_r, "R");
    lv_obj_set_style_text_color(label_r, lv_color_hex(COLOR_VU_RIGHT), 0);
    lv_obj_set_style_text_font(label_r, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(label_r, 168, 210);
    
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Test screen created from SVG layout");
}