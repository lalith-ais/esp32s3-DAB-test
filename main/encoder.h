#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

// Encoder GPIO pins - adjust if your wiring differs
#define ENCODER_PIN_A       (gpio_num_t) GPIO_NUM_4
#define ENCODER_PIN_B       (gpio_num_t) GPIO_NUM_5
#define ENCODER_PIN_SW      (gpio_num_t) GPIO_NUM_6

// Debounce time for the push button (ms)
#define ENCODER_SW_DEBOUNCE_MS  100

esp_err_t encoder_init(void);

// Returns the current position (0 .. max_pos-1), wrapping around
int  encoder_get_position(void);


// Returns true once per button press (edge triggered, debounced)
bool encoder_sw_pressed(void);
