#include "test_screen.h"
#include "lvgl.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"

static const char *TAG = "test_screen";

// Canvas buffer — RGB565, full screen 240×320
// Stored in IRAM-safe static so it survives between calls
#define CANVAS_W  320
#define CANVAS_H  240
#define BUF_PIXELS (CANVAS_W * CANVAS_H)

static lv_color_t *s_canvas_buf = NULL;
static lv_obj_t  *s_canvas = NULL;

// ─── Internal: fill buffer with random RGB565 pixels ─────────────────────────

static void fill_random(void)
{
    // esp_random() gives 32 bits — pack two RGB565 pixels per call
    uint32_t *buf32 = (uint32_t *)s_canvas_buf;
    for (int i = 0; i < BUF_PIXELS / 2; i++) {
        buf32[i] = esp_random();
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void test_screen_noise(void)
{
    if (lvgl_port_lock(0)) {
        if (s_canvas == NULL) {
            // Allocate canvas buffer from PSRAM — too large for DRAM
            s_canvas_buf = heap_caps_malloc(
                BUF_PIXELS * sizeof(lv_color_t),
                MALLOC_CAP_SPIRAM);

            if (s_canvas_buf == NULL) {
                ESP_LOGE(TAG, "Failed to allocate canvas buffer in PSRAM");
                lvgl_port_unlock();
                return;
            }

            lv_obj_t *scr = lv_scr_act();
            s_canvas = lv_canvas_create(scr);
            lv_canvas_set_buffer(s_canvas, s_canvas_buf,
                                 CANVAS_W, CANVAS_H,
                                 LV_COLOR_FORMAT_RGB565);
            lv_obj_set_pos(s_canvas, 0, 0);
            ESP_LOGI(TAG, "Canvas created %dx%d in PSRAM", CANVAS_W, CANVAS_H);
        }

        fill_random();
        lv_obj_invalidate(s_canvas);
        lvgl_port_unlock();
    }
}

void test_screen_noise_destroy(void)
{
    if (lvgl_port_lock(0)) {
        if (s_canvas != NULL) {
            lv_obj_del(s_canvas);
            s_canvas = NULL;
        }
        if (s_canvas_buf != NULL) {
            heap_caps_free(s_canvas_buf);
            s_canvas_buf = NULL;
        }
        lvgl_port_unlock();
    }
}
