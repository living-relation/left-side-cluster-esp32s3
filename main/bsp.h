/**
 * bsp.h — board support for Waveshare ESP32-S3-Touch-LCD-2.8C (left cluster).
 *
 * Display: 480×480 round, ST7701S, RGB-565 16-bit parallel interface.
 * Touch:   GT911 capacitive, I²C (SCL=GPIO7, SDA=GPIO15, INT=GPIO16).
 * Panel init: SPI-like 3-wire over GPIO1 (SDA) / GPIO2 (SCK);
 *             CS and RST via TCA9554 I²C expander (addr 0x20).
 *
 * Backlight: GPIO6 (LCD_BL) via ESP32 LEDC PWM in bsp.c (required for a lit panel).
 */
#ifndef BSP_H
#define BSP_H

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES  480
#define BSP_LCD_V_RES  480

/** Drive BL GPIO low before panel/LVGL init (avoids power-on white flash). */
void bsp_backlight_hold_off(void);

/** Enable backlight after the first black LVGL frame (call from ui_init). */
void bsp_backlight_on(void);

/** Set backlight brightness, percent (clamped to [15,100]; 100 = brightest). */
void bsp_backlight_set_percent(uint8_t pct);

esp_err_t  bsp_init(void);
lv_disp_t *bsp_display_start(void);
bool        bsp_lvgl_lock(uint32_t timeout_ms);
void        bsp_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
#endif
