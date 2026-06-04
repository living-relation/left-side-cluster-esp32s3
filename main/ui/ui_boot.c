/**
 * ui_boot.c — static Toyota boot splash, side clusters (left & right).
 *
 * Shows toyota_logo.png (composite mark + wordmark) for ~1.5 s then
 * fires on_done() to hand off to the live cluster screen.
 *
 * Asset: main/ui/assets/toyota_logo.c — auto-generated LVGL ARGB8888
 * image, compiled directly into the firmware.
 */
#include "ui_boot.h"
#include "lvgl.h"

/* Image declared in ui/assets/toyota_logo.c */
LV_IMAGE_DECLARE(toyota_logo);

static ui_boot_done_cb_t s_cb      = NULL;
static lv_obj_t         *s_overlay = NULL;

static void boot_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_image_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void boot_done_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    if (s_overlay) { lv_obj_del(s_overlay); s_overlay = NULL; }
    if (s_cb) s_cb();
}

void ui_boot_start(lv_obj_t *parent, ui_boot_done_cb_t on_done)
{
    s_cb = on_done;

    /* Full-screen black overlay */
    s_overlay = lv_obj_create(parent);
    lv_obj_set_size(s_overlay, 480, 480);
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_overlay);

    /* Toyota logo image — scaled to fit 300×250 within the 480×480 display */
    lv_obj_t *img = lv_image_create(s_overlay);
    lv_image_set_src(img, &toyota_logo);
    lv_image_set_scale(img, 175);   /* ~68% — fits 300×250 px */
    lv_obj_center(img);

    /* Simple fade-in */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_time(&a, 400);
    lv_anim_set_exec_cb(&a, boot_opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_timer_t *t = lv_timer_create(boot_done_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}
