/**
 * main.c — TrackCluster LEFT cluster (ESP32-S3, 480×480)
 *
 * UART consumer only. Receives LEFT frames (FRAME_TYPE 0x01) from
 * the center cluster at 921600 8N1 on GPIO 18.
 *
 * Displays: MPH (arc + digital), Oil Temp, Oil Pressure,
 *           Fuel Pressure, Fuel Level (mini-arcs).
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "TCA9554PWR.h"
#include "ST7701S.h"
#include "GT911.h"
#include "LVGL_Driver.h"
#include "boot/boot_screen.h"
#include "ui/screens/ui_Screen1.h"
#include "ui/ui.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "left-dash_data.h"
#include "left-colors.h"

/* ── UART config ───────────────────────────────────────────────────────── */
#define UART_PORT       UART_NUM_1
#define UART_RX_PIN     18          /* GPIO 18 per design spec */
#define UART_TX_PIN     17          /* GPIO 17 reserved (future) */

static const char *TAG = "LEFT";

/* Reference the global dash instance from dash_bridge.c */
extern volatile dash_data_t dash;

/* ── UART init ─────────────────────────────────────────────────────────── */
static void uart_init(void) {
    uart_config_t cfg = {
        .baud_rate  = UART_BRIDGE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
                                  UART_TX_PIN,
                                  UART_RX_PIN,
                                  UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 4096, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "UART initialized (921600 8N1, RX on GPIO %d)", UART_RX_PIN);
}

/* ── UART RX task ──────────────────────────────────────────────────────── *
 * Byte-level state machine: sync on SOF 0xA5, accumulate 32 bytes,
 * validate via dash_decode_left(), stamp last_update_ms.
 */
static void uart_rx_task(void *arg) {
    uint8_t buf[UART_BRIDGE_FRAME_LEN];
    int idx = 0;
    uint16_t seq;

    while (1) {
        uint8_t byte;
        if (uart_read_bytes(UART_PORT, &byte, 1, portMAX_DELAY) != 1)
            continue;

        /* Sync on SOF */
        if (idx == 0 && byte != UART_BRIDGE_SOF)
            continue;

        buf[idx++] = byte;

        if (idx == UART_BRIDGE_FRAME_LEN) {
            idx = 0;

            /* Decode directly into a temp, then copy under critical section */
            dash_data_t tmp;
            if (dash_decode_left(buf, &tmp, &seq)) {
                tmp.last_update_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

                portDISABLE_INTERRUPTS();
                memcpy((void *)&dash, &tmp, sizeof(dash_data_t));
                portENABLE_INTERRUPTS();
            } else {
                ESP_LOGW(TAG, "CRC/frame fail");
            }
        }
    }
}

/* ── Staleness state ───────────────────────────────────────────────────── */
typedef enum {
    STALE_NORMAL,       /* 0–500 ms: full render */
    STALE_FADING,       /* 500–2000 ms: 30% alpha */
    STALE_NO_ECU,       /* >2000 ms: NO ECU pill */
} stale_state_t;

static stale_state_t stale_state = STALE_NO_ECU;  /* start as no-data */

static stale_state_t check_staleness(void) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t age = now - dash.last_update_ms;

    if (dash.last_update_ms == 0)     return STALE_NO_ECU;
    if (age <= DASH_STALE_MS)         return STALE_NORMAL;
    if (age <= DASH_STALE_OFF_MS)     return STALE_FADING;
    return STALE_NO_ECU;
}

/* ── Paint timer (33 ms = 30 Hz) ───────────────────────────────────────── */
static void paint_timer_cb(lv_timer_t *t) {
    stale_state = check_staleness();

    if (stale_state == STALE_NO_ECU) {
        /* TODO Phase 4: show "WAITING FOR ECU…" / "NO ECU" pill */
        return;
    }

    /* Snapshot dash under critical section */
    dash_data_t snap;
    portDISABLE_INTERRUPTS();
    memcpy(&snap, (const void *)&dash, sizeof(dash_data_t));
    portENABLE_INTERRUPTS();

    /* TODO Phase 4: update all LVGL widgets from snap */
    (void)snap;
}

/* ── Alarm task (20 ms) ────────────────────────────────────────────────── */
static void alarm_task(void *arg) {
    while (1) {
        /* TODO Phase 4: check thresholds, set alarm flags */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ── App entry ─────────────────────────────────────────────────────────── */
void app_main(void) {
    I2C_Init();
    EXIO_Init();
    LCD_Init();
    Touch_Init();
    LVGL_Init();

    ui_init();
    uart_init();

    /* UART RX task — high priority */
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL,
                configMAX_PRIORITIES - 2, NULL);

    /* Alarm task — high priority, 20 ms cadence */
    xTaskCreate(alarm_task, "alarm", 2048, NULL,
                configMAX_PRIORITIES - 3, NULL);

    /* Paint timer — 30 Hz via LVGL timer */
    lv_timer_create(paint_timer_cb, 33, NULL);

    /* Boot splash: blank → backlight on */
    Set_Backlight(0);
    vTaskDelay(pdMS_TO_TICKS(750));
    Set_Backlight(100);
}