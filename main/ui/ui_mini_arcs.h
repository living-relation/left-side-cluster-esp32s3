#pragma once
#include "lvgl.h"
#include "dash_data.h"
#include "sdkconfig.h"
void ui_mini_arcs_create(lv_obj_t *parent);
void ui_mini_arcs_update(const dash_data_t *d);
void ui_mini_arcs_raise_text(void);
#if CONFIG_TC_BENCH_MODE
/** Call once per paint frame — limits heavy canvas redraw work. */
void ui_mini_arcs_bench_frame_begin(void);
#endif
