// test_screen.h
#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

// Declare test_screen as extern (defined in .c file)
extern lv_obj_t *test_screen;

void create_test_screen(void);
void update_test_encoder(int32_t position);
void update_test_signal_strength(uint8_t rssi);
void update_test_play_status(bool is_playing);