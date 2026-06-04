/**
 * main.c — left cluster application entry point.
 *
 * Boot: bsp_init() → bsp_display_start() → ui_init() (boot splash → live screen),
 * then either the UART RX task (normal) or a bench sweep (CONFIG_TC_BENCH_MODE).
 * No CAN. No sensor decoding. Data arrives over UART from center (normal mode).
 */

#include "bsp.h"
#include "dash_data.h"
#include "ui/ui.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "main";

extern void uart_rx_task(void *arg);
extern void alarm_task(void *arg);

#if CONFIG_TC_BENCH_MODE
extern portMUX_TYPE g_dash_mux;
/* Bench mode: no center/ECU connected. Slowly sweep this cluster's channels so
 * every widget + color can be verified standalone. Toggle OFF for real data. */
static void bench_task(void *arg)
{
    float p = 0.0f;
    for (;;) {
        p += 0.01f; if (p > 1.0f) p = 0.0f;
        portENTER_CRITICAL(&g_dash_mux);
        g_dash.mph            = p * 160.0f;
        g_dash.oil_temp       = 100.0f + p * 160.0f;
        g_dash.oil_press      = 40.0f + p * 90.0f;
        g_dash.fuel_press     = 20.0f + p * 100.0f;
        g_dash.fuel_level     = p * 100.0f;
        g_dash.last_update_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        portEXIT_CRITICAL(&g_dash_mux);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "TrackCluster Left — booting");

    ESP_ERROR_CHECK(bsp_init());
    lv_disp_t *disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "display init failed — halting");
        for (;;) vTaskDelay(portMAX_DELAY);
    }

    bsp_lvgl_lock(portMAX_DELAY);
    ui_init(disp);
    bsp_lvgl_unlock();

#if CONFIG_TC_BENCH_MODE
    ESP_LOGW(TAG, "BENCH MODE — demo sweep, UART RX disabled");
    xTaskCreatePinnedToCore(bench_task, "bench", 4096, NULL, 7, NULL, 0);
#else
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx", 4096, NULL, 7, NULL, 0);
#endif
    xTaskCreatePinnedToCore(alarm_task, "alarm", 4096, NULL, 6, NULL, 0);

    ESP_LOGI(TAG, "All tasks started");
}
