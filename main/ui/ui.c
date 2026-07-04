/**
 * ui.c — top-level LVGL UI coordinator, left cluster.
 *
 * Displays on 480×480 round:
 *   - MPH outer arc (solid 270°, always orange, lv_arc)
 *   - MPH digital readout (Aerospace 88 px, centered)
 *   - 4 mini segmented arcs (custom lv_canvas):
 *       top-left  (136,144) — Oil Temp   100–300 °F
 *       top-right (344,144) — Oil Press   10–140 PSI (arc fill)
 *       bot-right (344,336) — Fuel Press   0–160 PSI
 *       bot-left  (136,336) — Fuel Level   0–100 %
 *   (Standalone/bench: gauges always render g_dash — at 0 with nothing connected.)
 *
 * Visual reference: LVGL Simulator.html (left cluster) — kept in sync with this
 * code in both directions.
 */

#include "ui.h"
#include "bsp.h"
#include "ui_mph_arc.h"
#include "ui_mini_arcs.h"
#include "ui_boot.h"
#include "dash_data.h"
#include "left-colors.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
#include "esp_timer.h"

extern portMUX_TYPE g_dash_mux;

static const char *TAG_UI = "ui";
static bool s_live = false;

#if CONFIG_TC_BENCH_MODE
/* Paint-sync: one bench_step_once() per frame; step N integers (2–5) for speed. */
#define UI_PAINT_TICK_MS   2U
#define BENCH_STEP_SIZE    3   /* integers per frame (use 2–5) */
#else
#define UI_PAINT_TICK_MS  5U
#endif

#if CONFIG_TC_BENCH_MODE
static void bench_step_chan(int *val, int *dir, int min, int max, int step)
{
    int next = *val + (*dir * step);
    if (next >= max) {
        *val = max;
        *dir = -1;
    } else if (next <= min) {
        *val = min;
        *dir = 1;
    } else {
        *val = next;
    }
}

static void bench_step_once(void)
{
    static int s_mph = 0;
    static int s_ot = 100;
    static int s_op = 40;
    static int s_fp = 20;
    static int s_fl = 0;
    static int s_dir_mph = 1;
    static int s_dir_ot = 1;
    static int s_dir_op = 1;
    static int s_dir_fp = 1;
    static int s_dir_fl = 1;

    bench_step_chan(&s_mph, &s_dir_mph, 0, 200, BENCH_STEP_SIZE);
    bench_step_chan(&s_ot, &s_dir_ot, 100, 300, BENCH_STEP_SIZE);
    bench_step_chan(&s_op, &s_dir_op, 0, 140, BENCH_STEP_SIZE);
    bench_step_chan(&s_fp, &s_dir_fp, 0, 160, BENCH_STEP_SIZE);
    bench_step_chan(&s_fl, &s_dir_fl, 0, 100, BENCH_STEP_SIZE);

    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    portENTER_CRITICAL(&g_dash_mux);
    g_dash.mph            = (float)s_mph;
    g_dash.oil_temp       = (float)s_ot;
    g_dash.oil_press      = (float)s_op;
    g_dash.fuel_press     = (float)s_fp;
    g_dash.fuel_level     = (float)s_fl;
    g_dash.last_update_ms = ms;
    portEXIT_CRITICAL(&g_dash_mux);
}
#endif

void ui_paint_tick(lv_timer_t *t)
{
    if (!s_live) return;

#if CONFIG_TC_BENCH_MODE
    bench_step_once();
    ui_mini_arcs_bench_frame_begin();
#endif

    dash_data_t snap;
    portENTER_CRITICAL(&g_dash_mux);
    snap = *(const dash_data_t *)&g_dash;
    portEXIT_CRITICAL(&g_dash_mux);

    /* Sentinel (0xFF is impossible for a 0-100 value) so the first live tick
     * always applies whatever brightness is in g_dash, independent of the
     * startup default seeded elsewhere. */
    static uint8_t s_last_brightness = 0xFF;
    if (snap.brightness != s_last_brightness) {
        s_last_brightness = snap.brightness;
        bsp_backlight_set_percent(snap.brightness);
    }

    ui_mph_arc_update(&snap);
    ui_mini_arcs_update(&snap);
}

#define BOOT_LIVE_SETTLE_MS  400

static void ui_live_start_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    s_live = true;
#if CONFIG_TC_BENCH_MODE
    ESP_LOGI(TAG_UI, "bench paint-sync step=%d paint_ms=%u",
             BENCH_STEP_SIZE, (unsigned)UI_PAINT_TICK_MS);
#endif
    lv_timer_create(ui_paint_tick, UI_PAINT_TICK_MS, NULL);
}

void ui_on_boot_complete(void)
{
    /* Let splash teardown finish before fast arc/canvas updates (reduces RGB tear). */
    lv_timer_t *t = lv_timer_create(ui_live_start_cb, BOOT_LIVE_SETTLE_MS, NULL);
    lv_timer_set_repeat_count(t, 1);
}

void ui_init(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    ui_mini_arcs_create(scr);
    ui_mph_arc_create(scr);
    ui_mini_arcs_raise_text();
    ui_mph_arc_raise_text();
    ui_boot_start(scr, ui_on_boot_complete);

    if (bsp_lvgl_lock(portMAX_DELAY)) {
        lv_refr_now(NULL);
        bsp_lvgl_unlock();
    }
    bsp_backlight_on();
}
