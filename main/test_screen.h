#pragma once

// Fill the active LVGL screen with randomised RGB565 pixels.
// Safe to call repeatedly — canvas is created on first call,
// reused on subsequent calls.
// Must be called after lvgl_port_init() and lv_scr_load().
void test_screen_noise(void);

// Remove the canvas from the screen (call before switching to real UI).
void test_screen_noise_destroy(void);
