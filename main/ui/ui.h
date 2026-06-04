/**
 * ui.h — LVGL UI entry point, left cluster.
 */
#ifndef UI_H
#define UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(lv_disp_t *disp);
void ui_paint_tick(lv_timer_t *t);

#ifdef __cplusplus
}
#endif
#endif
