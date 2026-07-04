/**
 * bsp.c — Waveshare ESP32-S3-Touch-LCD-2.8C board support (left cluster).
 *
 * Schematic-verified pin assignments:
 *
 * ST7701S RGB parallel bus (16-bit 565, driven by esp_lcd_rgb_panel):
 *   R1=46  R2=3   R3=8   R4=18  R5=17
 *   G0=14  G1=13  G2=12  G3=11  G4=10  G5=9
 *   B1=5   B2=45  B3=48  B4=47  B5=21
 *   PCLK=41  DE=40  VSYNC=39  HSYNC=38
 *
 * Panel SPI init (3-wire):
 *   SDA=GPIO1  SCK=GPIO2
 *   CS  → TCA9554 P2  (I²C expander, addr 0x20)
 *   RST → TCA9554 P0
 *
 * I²C shared bus (SCL=GPIO7, SDA=GPIO15):
 *   TCA9554 @ 0x20 — LCD_RST(P0/EXIO1), TP_RST(P1/EXIO2),
 *                    LCD_CS(P2/EXIO3),  SD_CS(P4/EXIO5),
 *                    BUZZER(P7/EXIO8 — active HIGH, must be LOW at boot)
 *   GT911   @ 0x5D or 0x14 — capacitive touch
 *
 * Touch: GT911  INT=GPIO16  RST→TCA9554 P1
 *
 * Backlight: GPIO6 = LCD_BL — driven by ESP32 LEDC PWM (NOT external).
 */

#include "bsp.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_io_expander.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_rgb.h"
#include <string.h>
#if __has_include("esp_lcd_st7701.h")
#include "esp_lcd_st7701.h"
#endif

static const char *TAG = "bsp";

#define BSP_RGB_BOUNCE_LINES  10

/* ── GPIO constants (schematic-verified) ─────────────────────────────── */
#define BSP_LCD_SDA_GPIO     1
#define BSP_LCD_SCK_GPIO     2
#define BSP_PCLK_GPIO       41
#define BSP_DE_GPIO         40
#define BSP_VSYNC_GPIO      39
#define BSP_HSYNC_GPIO      38
#define BSP_R1_GPIO         46
#define BSP_R2_GPIO          3
#define BSP_R3_GPIO          8
#define BSP_R4_GPIO         18
#define BSP_R5_GPIO         17
#define BSP_G0_GPIO         14
#define BSP_G1_GPIO         13
#define BSP_G2_GPIO         12
#define BSP_G3_GPIO         11
#define BSP_G4_GPIO         10
#define BSP_G5_GPIO          9
#define BSP_B1_GPIO          5
#define BSP_B2_GPIO         45
#define BSP_B3_GPIO         48
#define BSP_B4_GPIO         47
#define BSP_B5_GPIO         21

#define TCA9554_ADDR            ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000
#define TCA9554_P_LCD_RST       IO_EXPANDER_PIN_NUM_0
#define TCA9554_P_TP_RST        IO_EXPANDER_PIN_NUM_1
#define TCA9554_P_LCD_CS        IO_EXPANDER_PIN_NUM_2
#define TCA9554_P_BUZZER        IO_EXPANDER_PIN_NUM_7

#define BSP_BL_GPIO         6
#define BSP_BL_LEDC_CH      LEDC_CHANNEL_0
#define BSP_BL_LEDC_TIMER   LEDC_TIMER_0
#define BSP_BL_LEDC_RES     LEDC_TIMER_10_BIT
#define BSP_BL_LEDC_FREQ_HZ 5000
#define BSP_BL_DUTY_MAX     ((1u << 10) - 1u)

static i2c_master_bus_handle_t  s_i2c_bus    = NULL;
static esp_io_expander_handle_t s_io_exp     = NULL;
static esp_lcd_panel_handle_t   s_panel      = NULL;
static esp_lcd_panel_io_handle_t s_panel_io  = NULL;

static esp_err_t i2c_and_expander_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = I2C_NUM_0,
        .scl_io_num        = CONFIG_TC_I2C_SCL_GPIO,
        .sda_io_num        = CONFIG_TC_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "i2c_bus");

    ESP_RETURN_ON_ERROR(
        esp_io_expander_new_i2c_tca9554(s_i2c_bus, TCA9554_ADDR, &s_io_exp),
        TAG, "tca9554");

    const uint32_t out_mask =
        TCA9554_P_LCD_RST | TCA9554_P_TP_RST | TCA9554_P_LCD_CS | TCA9554_P_BUZZER;
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_dir(s_io_exp, out_mask, IO_EXPANDER_OUTPUT),
        TAG, "tca_dir");

    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_level(s_io_exp,
            TCA9554_P_LCD_RST | TCA9554_P_TP_RST | TCA9554_P_LCD_CS, 1),
        TAG, "tca_hi");
    ESP_RETURN_ON_ERROR(
        esp_io_expander_set_level(s_io_exp, TCA9554_P_BUZZER, 0),
        TAG, "tca_buz");
    return ESP_OK;
}

static bool s_backlight_pwm_ready = false;

void bsp_backlight_hold_off(void)
{
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << BSP_BL_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl);
    gpio_set_level(BSP_BL_GPIO, 0);
}

static esp_err_t backlight_pwm_init(void)
{
    if (s_backlight_pwm_ready) {
        return ESP_OK;
    }

    ledc_timer_config_t tcfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BSP_BL_LEDC_RES,
        .timer_num       = BSP_BL_LEDC_TIMER,
        .freq_hz         = BSP_BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&tcfg), TAG, "bl_timer");

    ledc_channel_config_t ccfg = {
        .gpio_num   = BSP_BL_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BSP_BL_LEDC_CH,
        .timer_sel  = BSP_BL_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ccfg), TAG, "bl_chan");
    s_backlight_pwm_ready = true;
    return ESP_OK;
}

void bsp_backlight_set_percent(uint8_t pct)
{
    if (backlight_pwm_init() != ESP_OK) {
        return;
    }
    if (pct < 15)  pct = 15;    /* >=15% floor backstop — never fully black */
    if (pct > 100) pct = 100;
    /* LEDC channel is non-inverted (no output_invert): higher duty = brighter,
     * so 100% -> BSP_BL_DUTY_MAX = brightest. */
    uint32_t duty = (BSP_BL_DUTY_MAX * pct) / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BSP_BL_LEDC_CH, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BSP_BL_LEDC_CH);
    ESP_LOGI(TAG, "Backlight %u%% (LEDC duty=%lu)", pct, (unsigned long)duty);
}

static void panel_framebuffer_black(void)
{
    void *fb0 = NULL;
    void *fb1 = NULL;
    esp_err_t err = esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, &fb0, &fb1);
    size_t fb_bytes = (size_t)BSP_LCD_H_RES * BSP_LCD_V_RES * 2;
    if (err == ESP_OK && fb0 != NULL) {
        memset(fb0, 0, fb_bytes);
        if (fb1 != NULL) {
            memset(fb1, 0, fb_bytes);
        }
        return;
    }
    fb0 = NULL;
    err = esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &fb0);
    if (err != ESP_OK || fb0 == NULL) {
        ESP_LOGW(TAG, "RGB FB clear skipped (%s)", esp_err_to_name(err));
        return;
    }
    memset(fb0, 0, fb_bytes);
}

void bsp_backlight_on(void)   /* startup default until first frame arrives */
{
    bsp_backlight_set_percent(100);
}

#define ST7701_DATA(...) ((const uint8_t[]){__VA_ARGS__}), sizeof((const uint8_t[]){__VA_ARGS__})

static const st7701_lcd_init_cmd_t st7701_waveshare_init_cmds[] = {
    {0xFF, ST7701_DATA(0x77, 0x01, 0x00, 0x00, 0x13), 0},
    {0xEF, ST7701_DATA(0x08), 0},
    {0xFF, ST7701_DATA(0x77, 0x01, 0x00, 0x00, 0x10), 0},
    {0xC0, ST7701_DATA(0x3B, 0x00), 0},
    {0xC1, ST7701_DATA(0x10, 0x0C), 0},
    {0xC2, ST7701_DATA(0x07, 0x0A), 0},
    {0xC7, ST7701_DATA(0x00), 0},
    {0xCC, ST7701_DATA(0x10), 0},
    {0xCD, ST7701_DATA(0x08), 0},
    {0xB0, ST7701_DATA(0x05,0x12,0x98,0x0E,0x0F,0x07,0x07,0x09,
                        0x09,0x23,0x05,0x52,0x0F,0x67,0x2C,0x11), 0},
    {0xB1, ST7701_DATA(0x0B,0x11,0x97,0x0C,0x12,0x06,0x06,0x08,
                        0x08,0x22,0x03,0x51,0x11,0x66,0x2B,0x0F), 0},
    {0xFF, ST7701_DATA(0x77, 0x01, 0x00, 0x00, 0x11), 0},
    {0xB0, ST7701_DATA(0x5D), 0},
    {0xB1, ST7701_DATA(0x3E), 0},
    {0xB2, ST7701_DATA(0x81), 0},
    {0xB3, ST7701_DATA(0x80), 0},
    {0xB5, ST7701_DATA(0x4E), 0},
    {0xB7, ST7701_DATA(0x85), 0},
    {0xB8, ST7701_DATA(0x20), 0},
    {0xC1, ST7701_DATA(0x78), 0},
    {0xC2, ST7701_DATA(0x78), 0},
    {0xD0, ST7701_DATA(0x88), 0},
    {0xE0, ST7701_DATA(0x00, 0x00, 0x02), 0},
    {0xE1, ST7701_DATA(0x06,0x30,0x08,0x30,0x05,0x30,0x07,0x30,
                        0x00,0x33,0x33), 0},
    {0xE2, ST7701_DATA(0x11,0x11,0x33,0x33,0xF4,0x00,0x00,0x00,
                        0xF4,0x00,0x00,0x00), 0},
    {0xE3, ST7701_DATA(0x00, 0x00, 0x11, 0x11), 0},
    {0xE4, ST7701_DATA(0x44, 0x44), 0},
    {0xE5, ST7701_DATA(0x0D,0xF5,0x30,0xF0,0x0F,0xF7,0x30,0xF0,
                        0x09,0xF1,0x30,0xF0,0x0B,0xF3,0x30,0xF0), 0},
    {0xE6, ST7701_DATA(0x00, 0x00, 0x11, 0x11), 0},
    {0xE7, ST7701_DATA(0x44, 0x44), 0},
    {0xE8, ST7701_DATA(0x0C,0xF4,0x30,0xF0,0x0E,0xF6,0x30,0xF0,
                        0x08,0xF0,0x30,0xF0,0x0A,0xF2,0x30,0xF0), 0},
    {0xE9, ST7701_DATA(0x36, 0x01), 0},
    {0xEB, ST7701_DATA(0x00,0x01,0xE4,0xE4,0x44,0x88,0x40), 0},
    {0xED, ST7701_DATA(0xFF,0x10,0xAF,0x76,0x54,0x2B,0xCF,0xFF,
                        0xFF,0xFC,0xB2,0x45,0x67,0xFA,0x01,0xFF), 0},
    {0xEF, ST7701_DATA(0x08,0x08,0x08,0x45,0x3F,0x54), 0},
    {0xFF, ST7701_DATA(0x77, 0x01, 0x00, 0x00, 0x00), 0},
    {0x11, (const uint8_t[]){0x00}, 0, 120},
    {0x3A, ST7701_DATA(0x66), 0},
    {0x36, ST7701_DATA(0x00), 0},
    {0x35, ST7701_DATA(0x00), 0},
    {0x29, (const uint8_t[]){0x00}, 0, 20},
};

static esp_err_t panel_init(void)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_io_exp, TCA9554_P_LCD_RST, 0), TAG, "rst_lo");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(s_io_exp, TCA9554_P_LCD_RST, 1), TAG, "rst_hi");
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_line_config_t line_cfg = {
        .cs_io_type      = IO_TYPE_EXPANDER,
        .cs_expander_pin = TCA9554_P_LCD_CS,
        .scl_io_type     = IO_TYPE_GPIO,
        .scl_gpio_num    = BSP_LCD_SCK_GPIO,
        .sda_io_type     = IO_TYPE_GPIO,
        .sda_gpio_num    = BSP_LCD_SDA_GPIO,
        .io_expander     = s_io_exp,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg =
        ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, 0);
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_3wire_spi(&io_cfg, &s_panel_io), TAG, "panel_io");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    const int bounce_px = BSP_LCD_H_RES * BSP_RGB_BOUNCE_LINES;
    esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src     = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width  = 16,
        .bits_per_pixel = 16,
        .num_fbs     = 1,
        .bounce_buffer_size_px = bounce_px,
        .disp_gpio_num  = -1,
        .pclk_gpio_num  = BSP_PCLK_GPIO,
        .vsync_gpio_num = BSP_VSYNC_GPIO,
        .hsync_gpio_num = BSP_HSYNC_GPIO,
        .de_gpio_num    = BSP_DE_GPIO,
        .data_gpio_nums = {
            BSP_B1_GPIO, BSP_B2_GPIO, BSP_B3_GPIO, BSP_B4_GPIO, BSP_B5_GPIO,
            BSP_G0_GPIO, BSP_G1_GPIO, BSP_G2_GPIO, BSP_G3_GPIO, BSP_G4_GPIO, BSP_G5_GPIO,
            BSP_R1_GPIO, BSP_R2_GPIO, BSP_R3_GPIO, BSP_R4_GPIO, BSP_R5_GPIO,
        },
        .timings = {
            .pclk_hz = 18 * 1000 * 1000,
            .h_res   = BSP_LCD_H_RES,
            .v_res   = BSP_LCD_V_RES,
            .hsync_back_porch  = 10,
            .hsync_front_porch = 50,
            .hsync_pulse_width = 8,
            .vsync_back_porch  = 18,
            .vsync_front_porch = 8,
            .vsync_pulse_width = 2,
            .flags.pclk_active_neg = false,
        },
        .flags.fb_in_psram = true,
    };

#if __has_include("esp_lcd_st7701.h")
    st7701_vendor_config_t vendor_cfg = {
        .rgb_config     = &rgb_cfg,
        .init_cmds      = st7701_waveshare_init_cmds,
        .init_cmds_size = sizeof(st7701_waveshare_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
        .flags.mirror_by_cmd    = false,
        .flags.auto_del_panel_io = true,
    };
    panel_cfg.vendor_config = &vendor_cfg;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_st7701(s_panel_io, &panel_cfg, &s_panel), TAG, "st7701");
#else
    ESP_LOGW(TAG, "esp_lcd_st7701 component not found — creating RGB panel directly");
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_rgb_panel(&rgb_cfg, &s_panel), TAG, "rgb_panel");
#endif

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel),  TAG, "init");
    panel_framebuffer_black();
    return ESP_OK;
}

esp_err_t bsp_init(void)
{
    ESP_LOGI(TAG, "TrackCluster Left BSP — ESP32-S3-Touch-LCD-2.8C");
    bsp_backlight_hold_off();
    ESP_RETURN_ON_ERROR(i2c_and_expander_init(), TAG, "i2c");
    ESP_RETURN_ON_ERROR(panel_init(),            TAG, "panel");
    ESP_RETURN_ON_ERROR(backlight_pwm_init(),    TAG, "backlight_pwm");
    return ESP_OK;
}

static lv_disp_t *s_disp = NULL;

lv_disp_t *bsp_display_start(void)
{
    if (s_disp) return s_disp;

    const lvgl_port_cfg_t port_cfg = {
        .task_priority    = 6,
        .task_stack       = 7168,
        .task_affinity    = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms  = 5,
    };
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = NULL,
        .panel_handle  = s_panel,
        .buffer_size   = BSP_LCD_H_RES * 80,
        .double_buffer = true,
        .hres          = BSP_LCD_H_RES,
        .vres          = BSP_LCD_V_RES,
        .monochrome    = false,
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .rotation      = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags         = {
            .buff_dma = false,
            .buff_spiram = true,
            .swap_bytes = false,
            .direct_mode = false,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = { .bb_mode = 1, .avoid_tearing = 0 },
    };
    s_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    if (bsp_lvgl_lock(portMAX_DELAY)) {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_obj_invalidate(scr);
        lv_refr_now(NULL);
        lv_refr_now(NULL);
        bsp_lvgl_unlock();
    }
    panel_framebuffer_black();

    return s_disp;
}

bool bsp_lvgl_lock(uint32_t timeout_ms) { return lvgl_port_lock(timeout_ms); }
void bsp_lvgl_unlock(void)              {        lvgl_port_unlock();          }
