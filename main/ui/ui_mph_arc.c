/**
 * ui_mph_arc.c — MPH outer arc + digital readout, left cluster.
 */
#include "ui_mph_arc.h"
#include "ui_signal_filter.h"
#include "left-colors.h"
#include "sdkconfig.h"

LV_FONT_DECLARE(aerospace_88);
LV_FONT_DECLARE(racehead_18);

static lv_obj_t *s_arc;
static lv_obj_t *s_dig;
static lv_obj_t *s_unit;
static int s_last_arc_v = -1;
static int s_last_dig = -1;

void ui_mph_arc_create(lv_obj_t *parent)
{
    s_arc = lv_arc_create(parent);
    lv_obj_set_size(s_arc, 460, 460);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    /* 0.1 MPH resolution on indicator (2000 steps over 270°). */
    lv_arc_set_range(s_arc, 0, 2000);
    lv_arc_set_value(s_arc, 0);
    lv_obj_set_style_arc_color(s_arc, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, COLOR_INACTIVE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 14, LV_PART_MAIN);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_arc_set_mode(s_arc, LV_ARC_MODE_NORMAL);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    s_dig = lv_label_create(parent);
    lv_label_set_text(s_dig, "0");
    lv_obj_set_style_text_color(s_dig, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_dig, &aerospace_88, 0);
    lv_obj_align(s_dig, LV_ALIGN_CENTER, 0, 0);

    s_unit = lv_label_create(parent);
    lv_label_set_text(s_unit, "MPH");
    lv_obj_set_style_text_color(s_unit, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(s_unit, &racehead_18, 0);
    lv_obj_align(s_unit, LV_ALIGN_CENTER, 0, 56);
}

void ui_mph_arc_raise_text(void)
{
    if (s_dig)  lv_obj_move_foreground(s_dig);
    if (s_unit) lv_obj_move_foreground(s_unit);
}

void ui_mph_arc_update(const dash_data_t *d)
{
    float raw = d->mph;
    if (raw < 0.0f) raw = 0.0f;
    if (raw > 200.0f) raw = 200.0f;

#if CONFIG_TC_BENCH_MODE
    float mph = raw;
#else
    float mph = ui_filter_mph(raw);
#endif

    int mph_i = (int)(mph + 0.5f);
    int arc_v = mph_i * 10;
    if (arc_v != s_last_arc_v) {
        s_last_arc_v = arc_v;
        lv_arc_set_value(s_arc, arc_v);
    }

    if (mph_i != s_last_dig) {
        s_last_dig = mph_i;
        lv_label_set_text_fmt(s_dig, "%d", mph_i);
        lv_obj_align(s_dig, LV_ALIGN_CENTER, 0, 0);
    }
}
