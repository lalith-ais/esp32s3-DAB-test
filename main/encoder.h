#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

// ── Pin assignments ────────────────────────────────────────────────────────────
#define ENCODER_PIN_A   GPIO_NUM_4   // CLK
#define ENCODER_PIN_B   GPIO_NUM_5   // DT
#define ENCODER_PIN_SW  GPIO_NUM_6   // Switch

// ── Button debounce ───────────────────────────────────────────────────────────
#define ENCODER_SW_DEBOUNCE_MS  300

// ── API ───────────────────────────────────────────────────────────────────────

// Initialise PCNT hardware and button GPIO.
esp_err_t encoder_init(void);

// Current position in detent steps.
// Positive = CW, negative = CCW from the last reset.
int  encoder_get_position(void);

// Reset position counter to zero.
// Use this when entering a new context (e.g. switching from volume to station select).
void encoder_reset_position(void);

// Returns true once per button press (edge-triggered, debounced).
bool encoder_sw_pressed(void);
