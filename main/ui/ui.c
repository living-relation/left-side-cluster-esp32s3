/**
 * ui.c — top-level LVGL UI coordinator, left cluster.
 *
 * Displays on 480×480 round:
 *   - MPH outer arc (solid 270°, always orange, lv_arc)
 *   - MPH digital readout (Aerospace 88 px, centered)
 *   - 4 mini segmented arcs (custom lv_canvas):
 *       top-left  (136,144) — Oil Temp   100–300 °F
 *       top-right (344,144) — Oil Press   40–140 PSI
 *       bot-right (344,336) — Fuel Press  20–160 PSI
 *       bot-left  (136,336) — Fuel Level   0–100 %
 *   (Standalone/bench: gauges always render g_dash — at 0 with nothing connected.)
 *
 * Visual reference: LVGL Simulator.html (left cluster) — kept in sync with this
 * code in both directions.
 */

#include "ui.h"
#include "ui_mph_arc.h"
#include "ui_mini_arcs.h"
#include "ui_boot.h"
#include "dash_data.h"
#include "left-colors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

extern portMUX_TYPE g_dash_mux;

static bool s_live = false;

#define UI_PAINT_TICK_MS  8   /* 125 Hz paint tick (matches center) */

void ui_paint_tick(lv_timer_t *t)
{
    if (!s_live) return;

    dash_data_t snap;
    portENTER_CRITICAL(&g_dash_mux);
    snap = *(const dash_data_t *)&g_dash;
    portEXIT_CRITICAL(&g_dash_mux);

    /* Always render the latest data (0 when nothing is connected — no overlay). */
    ui_mph_arc_update(&snap);
    ui_mini_arcs_update(&snap);
}

void ui_on_boot_complete(void)
{
    s_live = true;
    lv_timer_create(ui_paint_tick, UI_PAINT_TICK_MS, NULL);
}

void ui_init(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    ui_mph_arc_create(scr);
    ui_mini_arcs_create(scr);
    ui_boot_start(scr, ui_on_boot_complete);
}
