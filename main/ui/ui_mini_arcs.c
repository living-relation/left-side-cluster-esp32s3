/**
 * ui_mini_arcs.c — 4 segmented-arc canvases, left cluster.
 *
 * Each arc: 10 segments, 270° sweep, gap 3.5°, ro=64 ri=40.
 * Canvas 160×160 allocated from PSRAM, center at display coordinate (cx, cy).
 *
 *   top-left  (136,144) Oil Temp   100–300 °F
 *   top-right (344,144) Oil Press   40–140 PSI
 *   bot-right (344,336) Fuel Press  20–160 PSI
 *   bot-left  (136,336) Fuel Level   0–100 %
 *
 * Angles follow the same convention as ui_rpm_arc.c:
 *   a=0° → top (12 o'clock), increases clockwise.
 *   START_DEG=135 → bottom-left (7:30), sweeps CW to bottom-right (4:30).
 */
#include "ui_mini_arcs.h"
#include "left-colors.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

LV_FONT_DECLARE(aerospace_22);
LV_FONT_DECLARE(racehead_18);

#define N_SEGS      10
#define CANVAS_SZ   160
#define CX_L        80          /* canvas-local center */
#define CY_L        80
#define RO          64
#define RI          40
#define START_DEG   135.0f
#define TOTAL_DEG   270.0f
#define GAP_DEG     3.5f
#define WEDGE_STEPS 6

typedef struct {
    lv_obj_t   *canvas;
    void       *buf;
    lv_obj_t   *name_lbl;
    lv_obj_t   *val_lbl;
    float       range_min, range_max;
} mini_arc_t;

static mini_arc_t s_arcs[4];

#define PTS_PER_WEDGE ((WEDGE_STEPS + 1) * 2)
static lv_point_precise_t s_wedge_pts[N_SEGS][PTS_PER_WEDGE];
static int        s_wedge_n  [N_SEGS];

static float deg_to_rad(float d) { return d * (float)M_PI / 180.0f; }

static void precompute_wedges(void)
{
    float seg_span = (TOTAL_DEG - GAP_DEG * N_SEGS) / N_SEGS;
    for (int i = 0; i < N_SEGS; i++) {
        float a0 = START_DEG + i * (seg_span + GAP_DEG);
        float a1 = a0 + seg_span;
        lv_point_precise_t *pts = s_wedge_pts[i];
        int n = 0;
        /* Outer arc (a0 → a1) */
        for (int s = 0; s <= WEDGE_STEPS; s++) {
            float a = a0 + (a1 - a0) * (float)s / WEDGE_STEPS;
            float r = deg_to_rad(a - 90.0f);
            pts[n].x = (lv_coord_t)(CX_L + RO * cosf(r));
            pts[n].y = (lv_coord_t)(CY_L + RO * sinf(r));
            n++;
        }
        /* Inner arc (a1 → a0, reversed) */
        for (int s = WEDGE_STEPS; s >= 0; s--) {
            float a = a0 + (a1 - a0) * (float)s / WEDGE_STEPS;
            float r = deg_to_rad(a - 90.0f);
            pts[n].x = (lv_coord_t)(CX_L + RI * cosf(r));
            pts[n].y = (lv_coord_t)(CY_L + RI * sinf(r));
            n++;
        }
        s_wedge_n[i] = n;
    }
}

static void draw_arc_canvas(lv_obj_t *canvas, float value, float min, float max,
                             lv_color_t color, lv_color_t alarm_color, bool alarm)
{
    lv_canvas_fill_bg(canvas, COLOR_BG_PRIMARY, LV_OPA_COVER);

    float pct = (value - min) / (max - min);
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    int lit = (int)(pct * N_SEGS);
    if (lit > N_SEGS) lit = N_SEGS;

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;

    for (int i = 0; i < N_SEGS; i++) {
        dsc.bg_color = (i < lit) ? (alarm ? alarm_color : color) : COLOR_INACTIVE;
        /* Draw polygon as a triangle fan from the first vertex */
        const lv_point_precise_t *pts = s_wedge_pts[i];
        int n = s_wedge_n[i];
        for (int t = 1; t < n - 1; t++) {
            dsc.p[0] = pts[0];
            dsc.p[1] = pts[t];
            dsc.p[2] = pts[t + 1];
            lv_draw_triangle(&layer, &dsc);
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static lv_obj_t *make_canvas(lv_obj_t *parent, int cx, int cy, void **buf_out)
{
    void *buf = heap_caps_malloc(
        (size_t)(CANVAS_SZ) * (CANVAS_SZ) * 2, MALLOC_CAP_SPIRAM);
    *buf_out = buf;
    if (!buf) return NULL;
    lv_obj_t *c = lv_canvas_create(parent);
    lv_canvas_set_buffer(c, buf, CANVAS_SZ, CANVAS_SZ, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(c, cx - CANVAS_SZ / 2, cy - CANVAS_SZ / 2);
    lv_canvas_fill_bg(c, COLOR_BG_PRIMARY, LV_OPA_COVER);
    return c;
}

void ui_mini_arcs_create(lv_obj_t *parent)
{
    precompute_wedges();

    static const struct {
        int cx, cy;
        float rmin, rmax;
        const char *name;
    } DEFS[4] = {
        { 136, 144, 100.0f, 300.0f, "OIL F"   },
        { 344, 144,  40.0f, 140.0f, "OIL PSI" },
        { 344, 336,  20.0f, 160.0f, "FUEL PSI"},
        { 136, 336,   0.0f, 100.0f, "FUEL %"  },
    };

    for (int i = 0; i < 4; i++) {
        s_arcs[i].range_min = DEFS[i].rmin;
        s_arcs[i].range_max = DEFS[i].rmax;

        s_arcs[i].canvas = make_canvas(parent, DEFS[i].cx, DEFS[i].cy, &s_arcs[i].buf);

        /* Name label centered above canvas */
        s_arcs[i].name_lbl = lv_label_create(parent);
        lv_label_set_text(s_arcs[i].name_lbl, DEFS[i].name);
        lv_obj_set_style_text_color(s_arcs[i].name_lbl, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(s_arcs[i].name_lbl, &racehead_18, 0);
        lv_obj_set_width(s_arcs[i].name_lbl, CANVAS_SZ);
        lv_obj_set_style_text_align(s_arcs[i].name_lbl, LV_TEXT_ALIGN_CENTER, 0);
        /* Inside the arc hollow, just below the digital value (white, +5 px down) */
        lv_obj_set_pos(s_arcs[i].name_lbl, DEFS[i].cx - CANVAS_SZ / 2,
                       DEFS[i].cy + 11);

        /* Value label centered inside the hollow of the arc */
        s_arcs[i].val_lbl = lv_label_create(parent);
        lv_label_set_text(s_arcs[i].val_lbl, "-");
        lv_obj_set_style_text_color(s_arcs[i].val_lbl, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(s_arcs[i].val_lbl, &aerospace_22, 0);
        lv_obj_set_width(s_arcs[i].val_lbl, CANVAS_SZ);
        lv_obj_set_style_text_align(s_arcs[i].val_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s_arcs[i].val_lbl, DEFS[i].cx - CANVAS_SZ / 2,
                       DEFS[i].cy - 14);
    }
}

void ui_mini_arcs_update(const dash_data_t *d)
{
    bool ot_alarm = (d->flags & DASH_FLAG_OIL_TEMP_HOT)  != 0;
    bool op_alarm = (d->flags & DASH_FLAG_OIL_PRESS_LOW) != 0;
    bool fl_low   = (d->flags & DASH_FLAG_FUEL_LOW)      != 0;

    /* Oil temp */
    draw_arc_canvas(s_arcs[0].canvas, d->oil_temp, 100.0f, 300.0f,
                    COLOR_GREEN, COLOR_RED_HOT, ot_alarm);
    lv_label_set_text_fmt(s_arcs[0].val_lbl, "%d", (int)d->oil_temp);
    lv_obj_set_style_text_color(s_arcs[0].val_lbl,
                                ot_alarm ? COLOR_RED_HOT : COLOR_WHITE, 0);

    /* Oil press */
    draw_arc_canvas(s_arcs[1].canvas, d->oil_press, 40.0f, 140.0f,
                    COLOR_GREEN, COLOR_RED_HOT, op_alarm);
    lv_label_set_text_fmt(s_arcs[1].val_lbl, "%d", (int)d->oil_press);
    lv_obj_set_style_text_color(s_arcs[1].val_lbl,
                                op_alarm ? COLOR_RED_HOT : COLOR_WHITE, 0);

    /* Fuel press */
    draw_arc_canvas(s_arcs[2].canvas, d->fuel_press, 20.0f, 160.0f,
                    fuel_press_color(d->fuel_press), COLOR_RED_HOT, false);
    lv_label_set_text_fmt(s_arcs[2].val_lbl, "%d", (int)d->fuel_press);

    /* Fuel level (show as integer, no % — aerospace_28 has no glyph for %) */
    draw_arc_canvas(s_arcs[3].canvas, d->fuel_level, 0.0f, 100.0f,
                    fuel_color(d->fuel_level), COLOR_RED_HOT, fl_low);
    lv_label_set_text_fmt(s_arcs[3].val_lbl, "%d", (int)d->fuel_level);
    lv_obj_set_style_text_color(s_arcs[3].val_lbl,
                                fl_low ? COLOR_RED_HOT : COLOR_WHITE, 0);
}
