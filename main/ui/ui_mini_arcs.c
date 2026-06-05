/**
 * ui_mini_arcs.c — 4 segmented-arc canvases, left cluster.
 *
 * Each arc: 10 segments, 270° sweep, gap 3.5°, ro=64 ri=40.
 * Canvas 160×160 allocated from PSRAM, center at display coordinate (cx, cy).
 *
 *   top-left  (136,144) Oil Temp   100–300 °F
 *   top-right (344,144) Oil Press   10–140 PSI (arc fill; label shows actual PSI)
 *   bot-right (344,336) Fuel Press   0–160 PSI
 *   bot-left  (136,336) Fuel Level   0–100 %
 *
 * Angles follow the same convention as ui_rpm_arc.c:
 *   a=0° → top (12 o'clock), increases clockwise.
 *   START_DEG=135 → bottom-left (7:30), sweeps CW to bottom-right (4:30).
 */
#include "ui_mini_arcs.h"
#include "dash_data.h"
#include "left-colors.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG_MINI = "ui_mini";

LV_FONT_DECLARE(aerospace_28);
LV_FONT_DECLARE(racehead_18);

#define MINI_VAL_BOX_W       CANVAS_SZ
#define MINI_VAL_CENTER_Y    0   /* centered in arc hollow */
#define MINI_VAL_TO_NAME_GAP 25
#define MINI_NAME_NUDGE_Y    8

/* Per-arc X nudge (left = negative): oil -16, fuel -16. */
static const int8_t MINI_NAME_NUDGE_X[4] = { -16, -16, -16, -16 };
#define MINI_NAME_COL_W        72

static const char * const MINI_NAME_TEXT[4] = {
    "OIL\nF",
    "OIL\nPSI",
    "FUEL\nPSI",
    "FUEL\n%",
};

#define N_SEGS      10
#define CANVAS_SZ   160
#define CX_L        80          /* canvas-local center */
#define CY_L        80
#define RO          64
#define RI          40
/* Inner arc ID = 2*RI; ring is outline only, 1 px smaller, 3 px stroke. */
#define FUEL_LOW_RING_D      (2 * RI - 1)

#define OIL_PRESS_ARC_FLOOR_PSI  10.0f
#define OIL_PRESS_ARC_CEIL_PSI   140.0f
#define START_DEG   135.0f
#define TOTAL_DEG   270.0f
#define GAP_DEG     3.5f
#define WEDGE_STEPS 4

typedef struct {
    lv_obj_t   *canvas;
    void       *buf;
    lv_obj_t   *name_lbl;
    lv_obj_t   *val_lbl;
    int         cx;
    int         cy;
    float       range_min, range_max;
    lv_obj_t   *center_ring; /* hollow warning ring behind digits */
} mini_arc_t;

static mini_arc_t s_arcs[4];
static float s_last_pct[4];
static int s_last_val[4];

#define FUEL_LOW_ON_PCT      (DASH_FUEL_LOW - 1.0f)   /* red arc + value below ~20% */
#define FUEL_LOW_OFF_PCT     (DASH_FUEL_LOW + 2.0f)
#define FUEL_ORANGE_ON_PCT   (DASH_FUEL_CAUTION - 1.0f) /* orange arc + value below 50% */
#define FUEL_ORANGE_OFF_PCT  (DASH_FUEL_CAUTION + 2.0f)

static bool fuel_low_display(float pct)
{
    static bool latched = false;
    if (!latched && pct < FUEL_LOW_ON_PCT) {
        latched = true;
    } else if (latched && pct > FUEL_LOW_OFF_PCT) {
        latched = false;
    }
    return latched;
}

static bool fuel_orange_display(float pct)
{
    static bool latched = false;
    if (!latched && pct < FUEL_ORANGE_ON_PCT) {
        latched = true;
    } else if (latched && pct > FUEL_ORANGE_OFF_PCT) {
        latched = false;
    }
    return latched;
}

static void fuel_level_colors(float pct, bool *alarm, lv_color_t *arc_color,
                              lv_color_t *txt_color)
{
    bool low = fuel_low_display(pct);
    bool orange = !low && fuel_orange_display(pct);

    *alarm = low;
    *arc_color = low ? COLOR_RED_HOT : (orange ? COLOR_ORANGE : COLOR_CYAN);
    *txt_color = *arc_color;
}

static lv_color_t mini_arc_lit_color(lv_color_t seg_color, lv_color_t alarm_color, bool alarm)
{
    return alarm ? alarm_color : seg_color;
}

/** Arc fill 0–1: 10 PSI → ≥1 segment; 140 PSI → full arc. Label still shows true PSI. */
static float oil_press_fill_pct(float psi)
{
    if (psi < OIL_PRESS_ARC_FLOOR_PSI) {
        return 0.0f;
    }
    float span = OIL_PRESS_ARC_CEIL_PSI - OIL_PRESS_ARC_FLOOR_PSI;
    float pct = (psi - OIL_PRESS_ARC_FLOOR_PSI) / span;
    if (pct > 1.0f) {
        pct = 1.0f;
    }
    const float one_seg = 1.0f / (float)N_SEGS;
    if (pct > 0.0f && pct < one_seg) {
        pct = one_seg;
    }
    return pct;
}

static float mini_arc_fill_pct(int idx, float value, float min, float max)
{
    if (idx == 1) {
        return oil_press_fill_pct(value);
    }
    float pct = (value - min) / (max - min);
    if (pct < 0.0f) {
        pct = 0.0f;
    }
    if (pct > 1.0f) {
        pct = 1.0f;
    }
    return pct;
}

static lv_obj_t *make_center_ring(lv_obj_t *parent, int cx, int cy)
{
    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, FUEL_LOW_RING_D, FUEL_LOW_RING_D);
    lv_obj_set_pos(ring, cx - FUEL_LOW_RING_D / 2, cy - FUEL_LOW_RING_D / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 3, 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    return ring;
}

static bool colors_match(lv_color_t a, lv_color_t b)
{
    return lv_color_to_u16(a) == lv_color_to_u16(b);
}

static void mini_arc_ring_update(int idx, bool show, lv_color_t border_color)
{
    lv_obj_t *ring = s_arcs[idx].center_ring;
    if (!ring) {
        return;
    }
    lv_obj_set_style_border_color(ring, border_color, 0);
    if (show) {
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);
    }
}

#define FP_LOW_ON_PSI         (DASH_FUEL_PRESS_LOW - 2.0f)
#define FP_LOW_OFF_PSI        (DASH_FUEL_PRESS_LOW + 2.0f)
#define FP_HIGH_ON_PSI        (DASH_FUEL_PRESS_REDLINE + 2.0f)
#define FP_HIGH_OFF_PSI       (DASH_FUEL_PRESS_REDLINE - 3.0f)

/** Color from raw PSI (hysteresis). Arc fill may use filtered PSI separately. */
static lv_color_t fuel_press_display_color(float psi)
{
    static bool low_lat = false;
    static bool high_lat = false;
    if (!low_lat && psi < FP_LOW_ON_PSI) {
        low_lat = true;
    } else if (low_lat && psi > FP_LOW_OFF_PSI) {
        low_lat = false;
    }
    if (!high_lat && psi > FP_HIGH_ON_PSI) {
        high_lat = true;
    } else if (high_lat && psi < FP_HIGH_OFF_PSI) {
        high_lat = false;
    }
    if (low_lat || high_lat) {
        return COLOR_RED_HOT;
    }
    return COLOR_CYAN;
}

#if !CONFIG_TC_BENCH_MODE
/* Display-only slosh filter (g_dash.fuel_level stays raw for UART/alarms). */
#define FUEL_ATTACK_BLEND    0.20f   /* smooth rises — kills instant up-spikes from slosh */
#define FUEL_RELEASE_BLEND   0.12f   /* slower falls — damp brief dips in corners */
#define FUEL_DECEL_SNAP_PCT  8.0f    /* large drop still snaps (real emptying) */
#define FUEL_LABEL_STEP_PCT  1.2f    /* integer label hysteresis band */

static float s_disp_fuel = -1.0f;

static float fuel_level_display_filter(float raw)
{
    if (s_disp_fuel < 0.0f) {
        s_disp_fuel = raw;
        return raw;
    }
    float delta = raw - s_disp_fuel;
    if (fabsf(delta) >= FUEL_DECEL_SNAP_PCT) {
        s_disp_fuel = raw;
    } else if (delta > 0.0f) {
        s_disp_fuel += delta * FUEL_ATTACK_BLEND;
    } else {
        s_disp_fuel += delta * FUEL_RELEASE_BLEND;
    }
    return s_disp_fuel;
}

static int fuel_percent_label(float fl_disp)
{
    static int shown = -1;
    if (shown < 0) {
        shown = (int)(fl_disp + 0.5f);
        return shown;
    }
    int target = (int)(fl_disp + 0.5f);
    if (target > shown && (fl_disp - (float)shown) >= FUEL_LABEL_STEP_PCT) {
        shown = target;
    } else if (target < shown && ((float)shown - fl_disp) >= FUEL_LABEL_STEP_PCT) {
        shown = target;
    }
    return shown;
}

/* Display-only oil/fuel PSI (raw stays in g_dash for alarm_task / UART). */
#define PRESS_RISE_BLEND      0.14f  /* slow on rises — pump/cam ripple */
#define PRESS_FALL_BLEND      0.38f  /* faster on drops — real loss of pressure */
#define PRESS_SNAP_PSI        10.0f
#define PRESS_LABEL_STEP_PSI  2.0f
static float s_disp_oil_press = -1.0f;
static float s_disp_fuel_press = -1.0f;
static int s_oil_press_lbl = -1;
static int s_fuel_press_lbl = -1;

static float pressure_display_filter(float raw, float *state)
{
    if (*state < 0.0f) {
        *state = raw;
        return raw;
    }
    float delta = raw - *state;
    if (fabsf(delta) >= PRESS_SNAP_PSI) {
        *state = raw;
    } else if (delta < 0.0f) {
        *state += delta * PRESS_FALL_BLEND;
    } else {
        *state += delta * PRESS_RISE_BLEND;
    }
    return *state;
}

static int pressure_psi_label(float disp, int *shown)
{
    if (*shown < 0) {
        *shown = (int)(disp + 0.5f);
        return *shown;
    }
    int target = (int)(disp + 0.5f);
    if (target > *shown && (disp - (float)*shown) >= PRESS_LABEL_STEP_PSI) {
        *shown = target;
    } else if (target < *shown && ((float)*shown - disp) >= PRESS_LABEL_STEP_PSI) {
        *shown = target;
    }
    return *shown;
}

#endif

#if CONFIG_TC_BENCH_MODE
static int s_last_lit[4];
static int s_canvas_budget;
static bool s_pending[4];
#endif

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

static void draw_wedge_quads(lv_layer_t *layer, const lv_point_precise_t *pts,
                             lv_color_t color, lv_draw_triangle_dsc_t *dsc)
{
    dsc->bg_color = color;
    for (int k = 0; k < WEDGE_STEPS; k++) {
        lv_point_precise_t o0 = pts[k];
        lv_point_precise_t o1 = pts[k + 1];
        lv_point_precise_t i1 = pts[(2 * WEDGE_STEPS + 1) - (k + 1)];
        lv_point_precise_t i0 = pts[(2 * WEDGE_STEPS + 1) - k];
        dsc->p[0] = o0; dsc->p[1] = o1; dsc->p[2] = i1;
        lv_draw_triangle(layer, dsc);
        dsc->p[0] = o0; dsc->p[1] = i1; dsc->p[2] = i0;
        lv_draw_triangle(layer, dsc);
    }
}

/** Wedge for segment seg_i filled fill_frac in [0,1] (1 = full segment). */
static void build_partial_wedge_pts(int seg_i, float fill_frac, lv_point_precise_t *pts, int *n_out)
{
    float seg_span = (TOTAL_DEG - GAP_DEG * N_SEGS) / N_SEGS;
    float a0 = START_DEG + (float)seg_i * (seg_span + GAP_DEG);
    float a1 = a0 + seg_span * fill_frac;
    int n = 0;
    for (int s = 0; s <= WEDGE_STEPS; s++) {
        float a = a0 + (a1 - a0) * (float)s / (float)WEDGE_STEPS;
        float r = deg_to_rad(a - 90.0f);
        pts[n].x = (lv_coord_t)(CX_L + RO * cosf(r));
        pts[n].y = (lv_coord_t)(CY_L + RO * sinf(r));
        n++;
    }
    for (int s = WEDGE_STEPS; s >= 0; s--) {
        float a = a0 + (a1 - a0) * (float)s / (float)WEDGE_STEPS;
        float r = deg_to_rad(a - 90.0f);
        pts[n].x = (lv_coord_t)(CX_L + RI * cosf(r));
        pts[n].y = (lv_coord_t)(CY_L + RI * sinf(r));
        n++;
    }
    *n_out = n;
}

static void draw_arc_canvas(lv_obj_t *canvas, float pct,
                             lv_color_t color, lv_color_t alarm_color, bool alarm)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    float segs_f = pct * (float)N_SEGS;
    int lit_full = (int)segs_f;
    float frac = segs_f - (float)lit_full;
    if (lit_full >= N_SEGS) {
        lit_full = N_SEGS;
        frac = 0.0f;
    }

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;

    lv_point_precise_t partial_pts[PTS_PER_WEDGE];
    int partial_n = 0;
    lv_color_t active = alarm ? alarm_color : color;

    for (int i = 0; i < N_SEGS; i++) {
        if (i < lit_full) {
            draw_wedge_quads(&layer, s_wedge_pts[i], active, &dsc);
        } else if (i == lit_full && frac > 0.001f) {
            build_partial_wedge_pts(i, frac, partial_pts, &partial_n);
            (void)partial_n;
            draw_wedge_quads(&layer, partial_pts, active, &dsc);
        } else {
            draw_wedge_quads(&layer, s_wedge_pts[i], COLOR_INACTIVE, &dsc);
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

#if CONFIG_TC_BENCH_MODE
/** Full segments only (no partial wedge) — faster, steadier bench paint. */
static void draw_arc_canvas_seg_only(lv_obj_t *canvas, float pct,
                                     lv_color_t color, lv_color_t alarm_color, bool alarm)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    int lit = (int)(pct * (float)N_SEGS);
    if (lit > N_SEGS) {
        lit = N_SEGS;
    }

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    lv_color_t active = alarm ? alarm_color : color;

    for (int i = 0; i < N_SEGS; i++) {
        draw_wedge_quads(&layer, s_wedge_pts[i], (i < lit) ? active : COLOR_INACTIVE, &dsc);
    }
    lv_canvas_finish_layer(canvas, &layer);
}

void ui_mini_arcs_bench_frame_begin(void)
{
    s_canvas_budget = 1;
}
#endif

static lv_obj_t *make_canvas(lv_obj_t *parent, int cx, int cy, void **buf_out)
{
    void *buf = heap_caps_malloc(
        (size_t)(CANVAS_SZ) * (CANVAS_SZ) * 4, MALLOC_CAP_SPIRAM);
    *buf_out = buf;
    if (!buf) return NULL;
    lv_obj_t *c = lv_canvas_create(parent);
    lv_canvas_set_buffer(c, buf, CANVAS_SZ, CANVAS_SZ, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(c, cx - CANVAS_SZ / 2, cy - CANVAS_SZ / 2);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_canvas_fill_bg(c, lv_color_black(), LV_OPA_TRANSP);
    return c;
}

/** Name labels sit to the right of the centered value digits (values stay fixed). */
static void mini_arc_position_name(int idx)
{
    mini_arc_t *a = &s_arcs[idx];
    if (!a->name_lbl || !a->val_lbl) {
        return;
    }

    lv_obj_update_layout(a->val_lbl);
    lv_obj_update_layout(a->name_lbl);
    lv_coord_t tw = lv_obj_get_content_width(a->val_lbl);
    lv_coord_t nw = lv_obj_get_content_width(a->name_lbl);
    int x_left = a->cx - MINI_VAL_BOX_W / 2;
    int x_val_right = x_left + (MINI_VAL_BOX_W + tw) / 2;

    const lv_font_t *vf = lv_obj_get_style_text_font(a->val_lbl, LV_PART_MAIN);
    lv_coord_t vh = vf ? lv_font_get_line_height(vf) : 24;
    int val_top = a->cy - vh / 2 + MINI_VAL_CENTER_Y;
    int val_mid = val_top + vh / 2;
    int stack_h = lv_obj_get_height(a->name_lbl);

    /* Previous layout: name centered on arc (cx). New: right of digits + gap.
     * Pull back halfway toward the old position (50% of horizontal move). */
    int x_new = x_val_right + MINI_VAL_TO_NAME_GAP;
    int x_old = a->cx - nw / 2;
    int x_pos = x_old + (x_new - x_old) / 2;

    lv_obj_set_pos(a->val_lbl, x_left, val_top);
    lv_obj_set_pos(a->name_lbl,
                   x_pos + MINI_NAME_NUDGE_X[idx],
                   val_mid - stack_h / 2 + MINI_NAME_NUDGE_Y);
}

void ui_mini_arcs_create(lv_obj_t *parent)
{
    precompute_wedges();

    static const struct {
        int cx, cy;
        float rmin, rmax;
    } DEFS[4] = {
        { 136, 144, 100.0f, 300.0f },
        { 344, 144,   0.0f, 140.0f },
        { 344, 336,   0.0f, 160.0f },
        { 136, 336,   0.0f, 100.0f },
    };

    for (int i = 0; i < 4; i++) {
        s_arcs[i].cx = DEFS[i].cx;
        s_arcs[i].cy = DEFS[i].cy;
        s_arcs[i].range_min = DEFS[i].rmin;
        s_arcs[i].range_max = DEFS[i].rmax;

        s_arcs[i].canvas = make_canvas(parent, DEFS[i].cx, DEFS[i].cy, &s_arcs[i].buf);
        s_arcs[i].center_ring = make_center_ring(parent, DEFS[i].cx, DEFS[i].cy);

        /* Value label centered inside the hollow of the arc (position unchanged). */
        s_arcs[i].val_lbl = lv_label_create(parent);
        lv_label_set_text(s_arcs[i].val_lbl, "-");
        lv_obj_set_style_text_color(s_arcs[i].val_lbl, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(s_arcs[i].val_lbl, &aerospace_28, 0);
        lv_obj_set_style_text_opa(s_arcs[i].val_lbl, LV_OPA_COVER, 0);
        lv_obj_set_width(s_arcs[i].val_lbl, MINI_VAL_BOX_W);
        lv_obj_set_style_text_align(s_arcs[i].val_lbl, LV_TEXT_ALIGN_CENTER, 0);

        /* Name: two-line stack (word / unit), to the right of the digits. */
        s_arcs[i].name_lbl = lv_label_create(parent);
        lv_label_set_text(s_arcs[i].name_lbl, MINI_NAME_TEXT[i]);
        lv_obj_set_style_text_color(s_arcs[i].name_lbl, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(s_arcs[i].name_lbl, &racehead_18, 0);
        lv_obj_set_style_text_opa(s_arcs[i].name_lbl, LV_OPA_COVER, 0);
        lv_obj_set_width(s_arcs[i].name_lbl, MINI_NAME_COL_W);
        lv_obj_set_style_text_align(s_arcs[i].name_lbl, LV_TEXT_ALIGN_CENTER, 0);
        mini_arc_position_name(i);
        s_last_pct[i] = -1.0f;
        s_last_val[i] = -1;
#if CONFIG_TC_BENCH_MODE
        s_last_lit[i] = -1;
        s_pending[i] = false;
#endif
    }
}

void ui_mini_arcs_raise_text(void)
{
    for (int i = 0; i < 4; i++) {
        if (s_arcs[i].center_ring) {
            lv_obj_move_foreground(s_arcs[i].center_ring);
        }
        if (s_arcs[i].val_lbl) {
            lv_obj_move_foreground(s_arcs[i].val_lbl);
        }
        if (s_arcs[i].name_lbl) {
            lv_obj_move_foreground(s_arcs[i].name_lbl);
        }
    }
}

static void update_arc(int idx, float value, float min, float max,
                       lv_color_t color, lv_color_t alarm_color, bool alarm,
                       lv_obj_t *val_lbl, lv_color_t txt_color, int label_pct)
{
    float pct = mini_arc_fill_pct(idx, value, min, max);

    int ival = (label_pct >= 0) ? label_pct : (int)value;
#if CONFIG_TC_BENCH_MODE
    /* Bench: full canvas only when lit segment changes (digits still every +1). */
    int lit = (int)(pct * (float)N_SEGS);
    if (lit > N_SEGS) {
        lit = N_SEGS;
    }
    if (lit != s_last_lit[idx]) {
        if (s_canvas_budget > 0) {
            s_canvas_budget--;
            s_last_lit[idx] = lit;
            s_last_pct[idx] = pct;
            draw_arc_canvas_seg_only(s_arcs[idx].canvas, pct,
                                     color, alarm_color, alarm);
        } else {
            s_pending[idx] = true;
        }
    }
#else
    if (fabsf(pct - s_last_pct[idx]) >= 0.002f) {
        s_last_pct[idx] = pct;
        draw_arc_canvas(s_arcs[idx].canvas, pct, color, alarm_color, alarm);
    }
#endif
    if (ival != s_last_val[idx]) {
        s_last_val[idx] = ival;
        lv_label_set_text_fmt(val_lbl, "%d", ival);
        mini_arc_position_name(idx);
    }
    lv_obj_set_style_text_color(val_lbl, txt_color, 0);
}

#if CONFIG_TC_BENCH_MODE
static void bench_flush_pending(int idx, const dash_data_t *d,
                                lv_color_t color, lv_color_t alarm_color, bool alarm)
{
    if (!s_pending[idx] || s_canvas_budget <= 0) {
        return;
    }
    s_pending[idx] = false;
    s_canvas_budget--;
    float min = s_arcs[idx].range_min;
    float max = s_arcs[idx].range_max;
    float value = 0.0f;
    switch (idx) {
        case 0: value = d->oil_temp; break;
        case 1: value = d->oil_press; break;
        case 2: value = d->fuel_press; break;
        case 3: value = d->fuel_level; break;
    }
    float pct = mini_arc_fill_pct(idx, value, min, max);
    s_last_lit[idx] = (int)(pct * (float)N_SEGS);
    if (s_last_lit[idx] > N_SEGS) {
        s_last_lit[idx] = N_SEGS;
    }
    s_last_pct[idx] = pct;
    draw_arc_canvas_seg_only(s_arcs[idx].canvas, pct, color, alarm_color, alarm);
}
#endif

void ui_mini_arcs_update(const dash_data_t *d)
{
    bool ot_hot = (d->flags & DASH_FLAG_OIL_TEMP_HOT) != 0;
    bool op_alarm = (d->flags & DASH_FLAG_OIL_PRESS_LOW) != 0;
    float fl_raw = d->fuel_level;
    lv_color_t fp_col;
    lv_color_t fl_arc_col;
    bool fl_alarm;
#if CONFIG_TC_BENCH_MODE
    float op_disp = d->oil_press;
    float fp_disp = d->fuel_press;
    int op_lbl = -1;
    int fp_lbl = -1;
    float fl_disp = fl_raw;
    int fl_lbl = -1;
    fuel_level_colors(fl_raw, &fl_alarm, &fl_arc_col, &fl_arc_col);
#else
    float op_disp = pressure_display_filter(d->oil_press, &s_disp_oil_press);
    float fp_disp = pressure_display_filter(d->fuel_press, &s_disp_fuel_press);
    int op_lbl = pressure_psi_label(op_disp, &s_oil_press_lbl);
    int fp_lbl = pressure_psi_label(fp_disp, &s_fuel_press_lbl);
    float fl_disp = fuel_level_display_filter(fl_raw);
    int fl_lbl = fuel_percent_label(fl_disp);
    fuel_level_colors(fl_disp, &fl_alarm, &fl_arc_col, &fl_arc_col);
#endif
    /* Arc/ring color from raw PSI so red follows drops (filter lags on decel). */
    fp_col = fuel_press_display_color(d->fuel_press);

    lv_color_t ot_col = COLOR_GREEN;
    lv_color_t op_col = COLOR_GREEN;

#if CONFIG_TC_BENCH_MODE
    bench_flush_pending(0, d, ot_col, COLOR_RED_HOT, ot_hot);
    bench_flush_pending(1, d, op_col, COLOR_RED_HOT, op_alarm);
    bench_flush_pending(2, d, fp_col, COLOR_RED_HOT, false);
    bench_flush_pending(3, d, fl_arc_col, COLOR_RED_HOT, fl_alarm);
#endif

    lv_color_t ot_txt = mini_arc_lit_color(ot_col, COLOR_RED_HOT, ot_hot);
    lv_color_t op_txt = mini_arc_lit_color(op_col, COLOR_RED_HOT, op_alarm);
    lv_color_t fl_txt = mini_arc_lit_color(fl_arc_col, COLOR_RED_HOT, fl_alarm);

    update_arc(0, d->oil_temp, 100.0f, 300.0f, ot_col, COLOR_RED_HOT, ot_hot,
               s_arcs[0].val_lbl, ot_txt, -1);
    update_arc(1, op_disp, 0.0f, 140.0f, op_col, COLOR_RED_HOT, op_alarm,
               s_arcs[1].val_lbl, op_txt, op_lbl);
    update_arc(2, fp_disp, 0.0f, 160.0f, fp_col, COLOR_RED_HOT, false,
               s_arcs[2].val_lbl, fp_col, fp_lbl);
    update_arc(3, fl_disp, 0.0f, 100.0f, fl_arc_col, COLOR_RED_HOT, fl_alarm,
               s_arcs[3].val_lbl, fl_txt, fl_lbl);

    mini_arc_ring_update(0, ot_hot, ot_txt);
    mini_arc_ring_update(1, op_alarm, op_txt);
    mini_arc_ring_update(2, colors_match(fp_col, COLOR_RED_HOT), fp_col);
    mini_arc_ring_update(3, fl_alarm, fl_txt);
}
