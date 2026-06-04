/**
 * alarm_task.c — 20 ms threshold scanner, left cluster.
 *
 * Mirrors center's alarm_task but evaluates only the channels
 * rendered on the left display (MPH, oil temp/press, fuel press/level).
 * Writes DASH_FLAG_* bits atomically into g_dash.flags.
 *
 * Operates on the same g_dash populated by uart_rx_task.
 */

#include "dash_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

extern portMUX_TYPE g_dash_mux;

void alarm_task(void *arg)
{
    TickType_t       wake   = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(20);

    for (;;) {
        dash_data_t s;
        portENTER_CRITICAL(&g_dash_mux);
        s = *(const dash_data_t *)&g_dash;
        portEXIT_CRITICAL(&g_dash_mux);

        uint16_t flags = s.flags;   /* preserve flags set by center */

        /* Oil pressure (only flag if RPM data is fresh and engine is running) */
        if (s.rpm > DASH_OIL_PRESS_MIN_RPM && s.oil_press < DASH_OIL_PRESS_MIN)
            flags |= DASH_FLAG_OIL_PRESS_LOW;
        else
            flags &= ~DASH_FLAG_OIL_PRESS_LOW;

        /* Fuel low */
        if (s.fuel_level < DASH_FUEL_LOW)
            flags |= DASH_FLAG_FUEL_LOW;
        else
            flags &= ~DASH_FLAG_FUEL_LOW;

        /* Oil temp hot */
        if (s.oil_temp >= DASH_OIL_TEMP_REDLINE)
            flags |= DASH_FLAG_OIL_TEMP_HOT;
        else
            flags &= ~DASH_FLAG_OIL_TEMP_HOT;

        portENTER_CRITICAL(&g_dash_mux);
        g_dash.flags = flags;
        portEXIT_CRITICAL(&g_dash_mux);

        vTaskDelayUntil(&wake, period);
    }
}
