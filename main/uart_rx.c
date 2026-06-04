/**
 * uart_rx.c — UART receive task, left cluster.
 *
 * Receives 32-byte LEFT frames (FRAME_TYPE 0x01) from the center cluster
 * at 921 600 8N1, 50 Hz cadence. Decodes each valid frame into g_dash and
 * updates last_update_ms. Invalid frames (bad SOF/EOF/CRC) are counted and
 * discarded.
 *
 * Pins (Kconfig.projbuild — both are J9 connector pins):
 *   RX = GPIO44  ← wired to the center cluster's UART TX
 *   TX = GPIO43  → wired to the center cluster's UART RX (reserved)
 *
 * No sensor decoding occurs here. All data arrives pre-processed from center.
 */

#include "dash_data.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_err.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "uart_rx";

#define RX_UART_PORT  UART_NUM_1
#define RX_BUF_SIZE   256

/* Shared mux (also used by alarm_task.c and ui paint tick) */
portMUX_TYPE g_dash_mux = portMUX_INITIALIZER_UNLOCKED;

static void uart_rx_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = UART_BRIDGE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(RX_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(RX_UART_PORT,
                                 CONFIG_TC_UART_TX_GPIO,   /* TX → center (reserved) */
                                 CONFIG_TC_UART_RX_GPIO,   /* RX ← center */
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(RX_UART_PORT, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "UART1 ready — baud=%u RX=GPIO%d",
             UART_BRIDGE_BAUD, CONFIG_TC_UART_RX_GPIO);
}

void uart_rx_task(void *arg)
{
    uart_rx_init();

    uint8_t  buf[UART_BRIDGE_FRAME_LEN];
    uint8_t  raw[UART_BRIDGE_FRAME_LEN * 2];  /* lookahead buffer */
    int      raw_len = 0;
    uint32_t err_count = 0;

    for (;;) {
        /* Read available bytes into raw buffer */
        int n = uart_read_bytes(RX_UART_PORT,
                                raw + raw_len,
                                sizeof(raw) - raw_len,
                                pdMS_TO_TICKS(30));
        if (n > 0) raw_len += n;

        /* Scan for SOF byte, discard leading garbage */
        while (raw_len > 0 && raw[0] != UART_BRIDGE_SOF) {
            memmove(raw, raw + 1, --raw_len);
            err_count++;
        }

        if (raw_len < (int)UART_BRIDGE_FRAME_LEN) continue;

        /* We have a full candidate frame starting at raw[0] */
        memcpy(buf, raw, UART_BRIDGE_FRAME_LEN);
        memmove(raw, raw + UART_BRIDGE_FRAME_LEN,
                raw_len - UART_BRIDGE_FRAME_LEN);
        raw_len -= UART_BRIDGE_FRAME_LEN;

        dash_data_t snap;
        uint16_t seq;
        if (!dash_decode_left(buf, &snap, &seq)) {
            err_count++;
            if (err_count % 100 == 0)
                ESP_LOGW(TAG, "%" PRIu32 " frame errors so far", err_count);
            continue;
        }

        /* Valid frame — commit to global dash */
        portENTER_CRITICAL(&g_dash_mux);
        g_dash.mph         = snap.mph;
        g_dash.oil_temp    = snap.oil_temp;
        g_dash.oil_press   = snap.oil_press;
        g_dash.fuel_press  = snap.fuel_press;
        g_dash.fuel_level  = snap.fuel_level;
        g_dash.rpm         = snap.rpm;
        g_dash.gear        = snap.gear;
        g_dash.odo_mode    = snap.odo_mode;
        g_dash.odo         = snap.odo;
        g_dash.trip_a      = snap.trip_a;
        g_dash.trip_b      = snap.trip_b;
        g_dash.flags       = snap.flags;
        g_dash.last_update_ms =
            (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        portEXIT_CRITICAL(&g_dash_mux);
    }
}
