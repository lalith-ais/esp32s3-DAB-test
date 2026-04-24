// test_screen.c
#include "test_screen.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include <inttypes.h>  // For PRIi32 macro
#include <stdio.h>
#include <string.h>

static const char *TAG = "test_screen";

// Professional color palette (24-bit RGB)
#define COLOR_BG_MAIN       0x101218  // Dark charcoal
#define COLOR_BG_CARD       0x1A1D28  // Slightly lighter
#define COLOR_TEXT_PRIMARY  0xFFFFFF  // White
#define COLOR_TEXT_SECONDARY 0xC8D0E0 // Light gray
#define COLOR_ACCENT_CYAN   0x00BCD4  // Cyan blue
#define COLOR_STATUS_GREEN  0x4CAF50  // Mint green
#define COLOR_STATUS_AMBER  0xFFC107  // Amber
#define COLOR_STATUS_RED    0xF44336  // Coral red
#define COLOR_SIGNAL_STRONG 0x4CAF50  // Green
#define COLOR_SIGNAL_WEAK   0xFFC107  // Amber
#define COLOR_SIGNAL_POOR   0xF44336  // Red

// Define test_screen here (not static - it's extern in header)
lv_obj_t *test_screen = NULL;

// UI element handles (static to this file only)
static lv_obj_t *label_program_name = NULL;
static lv_obj_t *label_ensemble = NULL;
static lv_obj_t *label_program_text = NULL;
static lv_obj_t *label_time = NULL;
static lv_obj_t *label_date = NULL;
static lv_obj_t *label_channel = NULL;
static lv_obj_t *label_bitrate = NULL;
static lv_obj_t *label_mode = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *label_signal = NULL;
static lv_obj_t *label_encoder = NULL;
static lv_obj_t *led_status = NULL;
static lv_obj_t *label_stereo = NULL;

// Update signal strength (0-100)
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

// Update play status
void update_test_play_status(bool is_playing) {
    if(!led_status) return;
    
    if(is_playing) {
        lv_obj_set_style_bg_color(led_status, lv_color_hex(COLOR_STATUS_GREEN), 0);
        lv_label_set_text(label_status, "PLAYING");
        lv_obj_set_style_text_color(label_status, lv_color_hex(COLOR_STATUS_GREEN), 0);
    } else {
        lv_obj_set_style_bg_color(led_status, lv_color_hex(COLOR_STATUS_AMBER), 0);
        lv_label_set_text(label_status, "STOPPED");
        lv_obj_set_style_text_color(label_status, lv_color_hex(COLOR_STATUS_AMBER), 0);
    }
}

// Update encoder position (fixed format specifier)
void update_test_encoder(int32_t position) {
    if(!label_encoder) return;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "ENC: %" PRIi32, position);  // Use PRIi32 macro
    lv_label_set_text(label_encoder, buf);
}

// Create the professional test screen (void return - no ESP_ERROR_CHECK needed)
void create_test_screen(void) {
    ESP_LOGI(TAG, "Creating professional test screen");
    
    lvgl_port_lock(0);
    
    // Create screen
    test_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(test_screen, lv_color_hex(COLOR_BG_MAIN), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(test_screen, 0, 0);
    
    lv_obj_t *scr = test_screen;
    
    // ========== TOP SECTION ==========
    
    // Time (top right)
    label_time = lv_label_create(scr);
    lv_label_set_text(label_time, "12:34:56");
    lv_obj_set_style_text_color(label_time, lv_color_hex(COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_24, 0);
    lv_obj_align(label_time, LV_ALIGN_TOP_RIGHT, -10, 10);
    
    // Date (top left)
    label_date = lv_label_create(scr);
    lv_label_set_text(label_date, "2024-01-15");
    lv_obj_set_style_text_color(label_date, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_align(label_date, LV_ALIGN_TOP_LEFT, 10, 12);
    
    // Signal meter (top right area)
    label_signal = lv_label_create(scr);
    lv_label_set_text(label_signal, "SIG: 75%");
    lv_obj_set_style_text_color(label_signal, lv_color_hex(COLOR_SIGNAL_STRONG), 0);
    lv_obj_set_style_text_font(label_signal, &lv_font_montserrat_14, 0);
    lv_obj_align(label_signal, LV_ALIGN_TOP_RIGHT, -10, 42);
    
    // ========== MIDDLE SECTION (Main content) ==========
    
    // Program Name (largest, most important)
    label_program_name = lv_label_create(scr);
    lv_label_set_text(label_program_name, "BBC RADIO 4");
    lv_obj_set_style_text_color(label_program_name, lv_color_hex(COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label_program_name, &lv_font_montserrat_32, 0);
    lv_obj_align(label_program_name, LV_ALIGN_CENTER, 0, -50);
    
    // Ensemble (secondary info)
    label_ensemble = lv_label_create(scr);
    lv_label_set_text(label_ensemble, "BBC National DAB");
    lv_obj_set_style_text_color(label_ensemble, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_ensemble, &lv_font_montserrat_14, 0);
    lv_obj_align(label_ensemble, LV_ALIGN_CENTER, 0, -15);
    
    // Program Text (dynamic, accent color)
    label_program_text = lv_label_create(scr);
    lv_label_set_text(label_program_text, "Now Playing: Morning Classics");
    lv_obj_set_style_text_color(label_program_text, lv_color_hex(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(label_program_text, &lv_font_montserrat_16, 0);
    lv_obj_align(label_program_text, LV_ALIGN_CENTER, 0, 15);
    
    // ========== BOTTOM SECTION (Status bar) ==========
    
    // Status LED (left side)
    led_status = lv_led_create(scr);
    lv_obj_set_size(led_status, 12, 12);
    lv_obj_set_style_bg_color(led_status, lv_color_hex(COLOR_STATUS_GREEN), 0);
    lv_obj_align(led_status, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    
    // Status text
    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "PLAYING");
    lv_obj_set_style_text_color(label_status, lv_color_hex(COLOR_STATUS_GREEN), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_align_to(label_status, led_status, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    
    // Channel
    label_channel = lv_label_create(scr);
    lv_label_set_text(label_channel, "CH: 12B");
    lv_obj_set_style_text_color(label_channel, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_channel, &lv_font_montserrat_12, 0);
    lv_obj_align(label_channel, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    lv_obj_align_to(label_channel, label_status, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    
    // Bitrate
    label_bitrate = lv_label_create(scr);
    lv_label_set_text(label_bitrate, "128 kbps");
    lv_obj_set_style_text_color(label_bitrate, lv_color_hex(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(label_bitrate, &lv_font_montserrat_12, 0);
    lv_obj_align_to(label_bitrate, label_channel, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    
    // Mode (DAB/FM)
    label_mode = lv_label_create(scr);
    lv_label_set_text(label_mode, "DAB");
    lv_obj_set_style_text_color(label_mode, lv_color_hex(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(label_mode, &lv_font_montserrat_12, 0);
    lv_obj_align_to(label_mode, label_bitrate, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    
    // Stereo indicator
    label_stereo = lv_label_create(scr);
    lv_label_set_text(label_stereo, "STEREO");
    lv_obj_set_style_text_color(label_stereo, lv_color_hex(COLOR_STATUS_GREEN), 0);
    lv_obj_set_style_text_font(label_stereo, &lv_font_montserrat_12, 0);
    lv_obj_align_to(label_stereo, label_mode, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    
    // Encoder position (bottom right)
    label_encoder = lv_label_create(scr);
    lv_label_set_text(label_encoder, "ENC: 0");
    lv_obj_set_style_text_color(label_encoder, lv_color_hex(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(label_encoder, &lv_font_montserrat_12, 0);
    lv_obj_align(label_encoder, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Test screen created successfully");
}

// Function to cycle through test data (optional - for demonstration)
void test_screen_cycle_data(void) {
    static int counter = 0;
    const char *programs[] = {"BBC RADIO 4", "CLASSIC FM", "TALK SPORT", "BBC RADIO 3"};
    const char *ensembles[] = {"BBC National DAB", "Digital One", "SDL National", "Local Multiplex"};
    const char *texts[] = {"Now Playing: Classical Music", "Live News: Parliament", "Sports Update", "Jazz Evening"};
    
    counter = (counter + 1) % 4;
    
    lvgl_port_lock(0);
    lv_label_set_text(label_program_name, programs[counter]);
    lv_label_set_text(label_ensemble, ensembles[counter]);
    lv_label_set_text(label_program_text, texts[counter]);
    
    // Random signal strength for demo
    update_test_signal_strength(50 + (counter * 15));
    
    // Toggle play status
    update_test_play_status(counter % 2 == 0);
    lvgl_port_unlock();
}