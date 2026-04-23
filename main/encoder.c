#include <string.h>

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

// Accumulated position (PCNT hardware counter is 16-bit and can overflow,
// so we accumulate into a full int via the watch-point callback)
static volatile int s_accumulated = 0;

// Button state
static volatile int64_t s_last_interrupt_us = 0;
static volatile bool    s_sw_pending = false;   // set in ISR, cleared by encoder_sw_pressed()


// ---------------------------------------------------------------------------
// PCNT overflow/underflow watch-point callback
// Called when the hardware counter hits +/- PCNT_HIGH_LIMIT
// ---------------------------------------------------------------------------
#define PCNT_HIGH_LIMIT  32766
#define PCNT_LOW_LIMIT  -32767

static bool IRAM_ATTR pcnt_on_reach(pcnt_unit_handle_t unit,
                                    const pcnt_watch_event_data_t *edata,
                                    void *user_ctx)
{
    // edata->watch_point_val is either +HIGH_LIMIT or -LOW_LIMIT
    s_accumulated += edata->watch_point_value;
    return false; // no task woken
}


// ---------------------------------------------------------------------------
// Button GPIO ISR
// ---------------------------------------------------------------------------
static void IRAM_ATTR gpio_sw_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if ((now - s_last_interrupt_us) > (ENCODER_SW_DEBOUNCE_MS * 1000)) {
        s_sw_pending = true;
        s_last_interrupt_us = now;
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
    pcnt_unit_config_t unit_cfg = {
        .low_limit  = PCNT_LOW_LIMIT,
        .high_limit = PCNT_HIGH_LIMIT,
        .flags.accum_count = true,   // hardware accumulates across glitches
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, &s_pcnt_unit), TAG, "pcnt_new_unit failed");

    // Glitch filter — ignores pulses shorter than 1 µs (removes contact bounce)
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 1000,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_cfg), TAG, "glitch filter failed");

    // Channel A — counts on A edges, gated by B level (gives x2 resolution)
    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num  = ENCODER_PIN_A,
        .level_gpio_num = ENCODER_PIN_B,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_a_cfg, &chan_a), TAG, "chan A failed");
    pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,   // rising  edge on A: B low  → CW
        PCNT_CHANNEL_EDGE_ACTION_INCREASE);  // falling edge on A: B high → CCW
    pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // B high → keep direction
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);  // B low  → invert direction

    // Channel B — counts on B edges, gated by A level (x4 resolution total)
    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num  = ENCODER_PIN_B,
        .level_gpio_num = ENCODER_PIN_A,
    };
    pcnt_channel_handle_t chan_b = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_b_cfg, &chan_b), TAG, "chan B failed");
    pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // rising  edge on B
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);  // falling edge on B
    pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // Watch points for overflow accumulation
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit,  PCNT_HIGH_LIMIT), TAG, "watch+ failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit,  PCNT_LOW_LIMIT),  TAG, "watch- failed");

    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_on_reach,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_register_event_callbacks(s_pcnt_unit, &cbs, NULL), TAG, "cb failed");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt_unit),  TAG, "pcnt enable failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "pcnt clear failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_pcnt_unit),   TAG, "pcnt start failed");

    // --- Push button ---
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << ENCODER_PIN_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,   // falling = button pressed
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sw_cfg), TAG, "sw gpio_config failed");
    // ISR service may already be installed by another driver (e.g. touch) - that's fine
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

    // x4 quadrature: divide by 4 to get detent steps, same as FOUR3 latch mode
    return (s_accumulated + hw) / 4;
}



bool encoder_sw_pressed(void)
{
    if (s_sw_pending) {
        s_sw_pending = false;
        return true;
    }
    return false;
}
