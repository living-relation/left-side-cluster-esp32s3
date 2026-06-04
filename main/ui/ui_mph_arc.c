/**
 * ui_mph_arc.c — MPH outer arc + digital readout, left cluster.
 *
 * Native 480×480. cx=cy=240.
 * Outer arc: lv_arc, single color ORANGE, 270° sweep, 0–200 MPH range.
 * Digital:   Aerospace 88 px, centered, always orange.
 * Unit:      RaceHead "MPH", below digital.
 *
 * See left-03-design-spec.md for full geometry.
 */
#include "ui_mph_arc.h"
#include "left-colors.h"

LV_FONT_DECLARE(aerospace_88);
LV_FONT_DECLARE(racehead_18);

#define CX 240
#define CY 240

static lv_obj_t *s_arc;
static lv_obj_t *s_dig;
static lv_obj_t *s_unit;

void ui_mph_arc_create(lv_obj_t *parent)
{
    s_arc = lv_arc_create(parent);
    lv_obj_set_size(s_arc, 460, 460);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, 200);
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
    lv_obj_align(s_dig, LV_ALIGN_CENTER, 0, -20);

    /* "MPH" unit label — RaceHead 18 px, white, 15 px below the digital value */
    s_unit = lv_label_create(parent);
    lv_label_set_text(s_unit, "MPH");
    lv_obj_set_style_text_color(s_unit, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(s_unit, &racehead_18, 0);
    lv_obj_align(s_unit, LV_ALIGN_CENTER, 0, 33);
}

void ui_mph_arc_update(const dash_data_t *d)
{
    int mph = (int)d->mph;
    if (mph < 0) mph = 0;
    if (mph > 200) mph = 200;
    lv_arc_set_value(s_arc, mph);
    lv_label_set_text_fmt(s_dig, "%d", mph);
}
