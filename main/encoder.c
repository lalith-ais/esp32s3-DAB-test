#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_attr.h>
#include <esp_check.h>

#include "encoder.h"

static const char *TAG = "encoder";

// PCNT unit handle
static pcnt_unit_handle_t s_pcnt_unit = NULL;

// Button state
static volatile int64_t s_last_interrupt_us = 0;
static volatile bool    s_sw_pending = false;

// ---------------------------------------------------------------------------
// Button GPIO ISR
// ---------------------------------------------------------------------------
// encoder.c — replace the ISR
static void IRAM_ATTR gpio_sw_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if ((now - s_last_interrupt_us) < (ENCODER_SW_DEBOUNCE_MS * 1000)) {
        return;   // too soon — ignore
    }
    s_last_interrupt_us = now;

    // Only register a press on the falling edge (pin low = pressed)
    if (gpio_get_level(ENCODER_PIN_SW) == 0) {
        s_sw_pending = true;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

esp_err_t encoder_init(void)
{
    ESP_LOGI(TAG, "Initialising rotary encoder A=%d B=%d SW=%d",
             ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_SW);

    // --- PCNT unit ---
    // flags.accum_count = true lets the hardware accumulate across the
    // ±32767 counter range automatically — no manual overflow callback needed
    pcnt_unit_config_t unit_cfg = {
        .low_limit         = -32767,
        .high_limit        =  32766,
        .flags.accum_count = true,
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, &s_pcnt_unit), TAG, "pcnt_new_unit failed");

    // Glitch filter — ignores pulses shorter than 1 µs (removes contact bounce)
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 1000,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_cfg), TAG, "glitch filter failed");

    // Channel A — counts on A edges, gated by B level (x2 resolution)
    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num  = ENCODER_PIN_A,
        .level_gpio_num = ENCODER_PIN_B,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_a_cfg, &chan_a), TAG, "chan A failed");
    pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // Channel B — counts on B edges, gated by A level (x4 resolution total)
    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num  = ENCODER_PIN_B,
        .level_gpio_num = ENCODER_PIN_A,
    };
    pcnt_channel_handle_t chan_b = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_b_cfg, &chan_b), TAG, "chan B failed");
    pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // No watch-point callbacks needed — hardware accumulation handles overflow
    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt_unit),      TAG, "pcnt enable failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "pcnt clear failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_pcnt_unit),       TAG, "pcnt start failed");

    // --- Push button ---
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << ENCODER_PIN_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sw_cfg), TAG, "sw gpio_config failed");

    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(isr_ret, TAG, "isr service failed");
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(ENCODER_PIN_SW, gpio_sw_isr, NULL), TAG, "isr handler failed");

    ESP_LOGI(TAG, "Encoder initialised OK");
    return ESP_OK;
}

int encoder_get_position(void)
{
    int hw = 0;
    pcnt_unit_get_count(s_pcnt_unit, &hw);
    // x4 quadrature — divide by 4 to get clean detent steps
    return hw / 4;
}

void encoder_set_position(int pos)
{
    pcnt_unit_clear_count(s_pcnt_unit);
    // Note: with hardware accumulation we can only reset to 0.
    // For the radio use case (volume 0-100, station index) always
    // reset to 0 and manage the logical range in the caller.
}

bool encoder_sw_pressed(void)
{
    if (s_sw_pending) {
        s_sw_pending = false;
        return true;
    }
    return false;
}
