/**
 * left-dash_data.h — TrackCluster master dash data model
 *
 * Self-contained per-cluster copy. Identical across all three repos except for
 * the include guard.
 *
 * - Center cluster:  PRODUCER. Decodes ECU CAN, transmits per-side UART frames.
 * - Left cluster:    CONSUMER. Receives LEFT UART frames (FRAME_TYPE 0x01).
 * - Right cluster:   CONSUMER. Receives RIGHT UART frames (FRAME_TYPE 0x02).
 *
 * Every field below is rendered by at least one display. Any sensor the ECU
 * broadcasts that does NOT appear on a display has been removed from the
 * dashboard data model entirely (no battery voltage, no baro, no fuel temp,
 * no coolant pressure, no ethanol, no throttle, no lap timer, no GPS speed).
 *
 * See `docs/left-02-data-model-and-uart.md` for the full UART protocol.
 */

#ifndef LEFT_DASH_DATA_H
#define LEFT_DASH_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Status flag bits (only flags driven by displayed channels) ──────────── */
#define DASH_FLAG_OIL_PRESS_LOW  (1u << 0)
#define DASH_FLAG_FUEL_LOW       (1u << 1)
#define DASH_FLAG_SHIFT          (1u << 2)
#define DASH_FLAG_REV_LIMIT      (1u << 3)
#define DASH_FLAG_COOLANT_HOT    (1u << 4)
#define DASH_FLAG_OIL_TEMP_HOT   (1u << 5)
#define DASH_FLAG_LAMBDA_BAD     (1u << 6)
#define DASH_FLAG_OVERBOOST      (1u << 7)

/* ── Odometer mode enum ──────────────────────────────────────────────────── */
typedef enum {
    DASH_ODO     = 0,
    DASH_TRIP_A  = 1,
    DASH_TRIP_B  = 2,
} dash_odo_mode_t;

/* ── Master struct — ONLY fields rendered on a display ────────────────────── */
typedef struct {
    /* Engine */
    float    rpm;          // center: tach arc, shift LEDs, gear-box tint
    float    lambda;       // right:  digital readout
    float    boost;        // right:  bar gauge (derived from MAP)
    float    ign_adv;      // right:  bar gauge

    /* Temperatures (°F) */
    float    oil_temp;     // left:   mini arc
    float    coolant_temp; // right:  bar gauge
    float    iat;          // right:  bar gauge

    /* Pressures (PSI) */
    float    oil_press;    // left:   mini arc
    float    fuel_press;   // left:   mini arc

    /* Driveline */
    float    mph;          // left:   speedo arc + digital
    int8_t   gear;         // center: gear glyph

    /* Fuel */
    float    fuel_level;   // left:   mini arc

    /* Trip / odo — computed locally on center by integrating mph */
    float    odo;
    float    trip_a;
    float    trip_b;
    uint8_t  odo_mode;     // dash_odo_mode_t

    /* Status & freshness */
    uint16_t flags;        // see DASH_FLAG_* bits
    uint32_t last_update_ms;
} dash_data_t;

/* Global snapshot, protected by a critical section / mux. */
extern volatile dash_data_t dash;

/* ── UART bridge protocol ────────────────────────────────────────────────── *
 * 32-byte fixed frame, 921 600 8N1, 50 Hz cadence per side.
 * Center transmits both frame types; left/right each receive one.
 */
#define UART_BRIDGE_BAUD         921600u
#define UART_BRIDGE_FRAME_LEN    32u
#define UART_BRIDGE_SOF          0xA5u
#define UART_BRIDGE_EOF          0x5Au
#define UART_BRIDGE_TYPE_LEFT    0x01u
#define UART_BRIDGE_TYPE_RIGHT   0x02u
#define UART_BRIDGE_PERIOD_MS    20u

/* CRC-8 over the 29-byte body (offsets 1..29) — poly 0x07, init 0xFF. */
uint8_t dash_crc8(const uint8_t *data, size_t n);

/* Encode dash → wire. Each writes exactly UART_BRIDGE_FRAME_LEN bytes. */
void dash_encode_left (uint8_t out[UART_BRIDGE_FRAME_LEN], const dash_data_t *d, uint16_t seq);
void dash_encode_right(uint8_t out[UART_BRIDGE_FRAME_LEN], const dash_data_t *d, uint16_t seq);

/* Decode wire → dash. Returns true on valid SOF/EOF/CRC, false otherwise. */
bool dash_decode_left (const uint8_t in[UART_BRIDGE_FRAME_LEN], dash_data_t *d, uint16_t *seq_out);
bool dash_decode_right(const uint8_t in[UART_BRIDGE_FRAME_LEN], dash_data_t *d, uint16_t *seq_out);

/* ── Threshold constants (used by display code, also alarm task) ─────────── */

/* RPM */
#define DASH_RPM_SHIFT_SLOW          5500.0f
#define DASH_RPM_SHIFT_FAST          6500.0f
#define DASH_RPM_LEDS_ALL_RED_ON     7000.0f    /* hysteresis: latch all-red */
#define DASH_RPM_LEDS_ALL_RED_OFF    6500.0f    /* hysteresis: release all-red */
#define DASH_RPM_REDLINE             6750.0f    /* segment redline */
#define DASH_RPM_REV_LIMIT           7500.0f    /* hard cut */

/* Oil */
#define DASH_OIL_TEMP_REDLINE        215.0f
#define DASH_OIL_TEMP_MAX            250.0f
#define DASH_OIL_PRESS_MIN           25.0f      /* alarm if rpm > MIN_RPM */
#define DASH_OIL_PRESS_MIN_RPM       2000.0f
#define DASH_OIL_PRESS_REDLINE       125.0f

/* Fuel pressure — display range 20..160 PSI */
#define DASH_FUEL_PRESS_DISPLAY_MIN  20.0f
#define DASH_FUEL_PRESS_DISPLAY_MAX  160.0f
#define DASH_FUEL_PRESS_LOW          30.0f
#define DASH_FUEL_PRESS_REDLINE      125.0f

/* Coolant */
#define DASH_ECT_GREEN_MAX           200.0f
#define DASH_ECT_YELLOW_MAX          215.0f
#define DASH_ECT_ORANGE_MAX          230.0f
#define DASH_COOLANT_MAX             230.0f

/* IAT — display range 30..200 °F */
#define DASH_IAT_DISPLAY_MIN         30.0f
#define DASH_IAT_DISPLAY_MAX         200.0f
#define DASH_IAT_GREEN_MAX           100.0f
#define DASH_IAT_YELLOW_MAX          130.0f
#define DASH_IAT_ORANGE_MAX          150.0f

/* Boost */
#define DASH_BOOST_CYAN_MAX          14.0f
#define DASH_BOOST_ORANGE_MAX        20.0f
#define DASH_BOOST_OVERBOOST         22.0f

/* Lambda — widened for 93 octane AND E85.
 * Alarms fire on raw value (no throttle gating — throttle is not displayed
 * and therefore no longer in the data model). */
#define DASH_LAMBDA_RICH_ALARM       0.70f
#define DASH_LAMBDA_RICH_WARN        0.75f
#define DASH_LAMBDA_LEAN_WARN        1.02f
#define DASH_LAMBDA_LEAN_ALARM       1.05f

/* Ignition */
#define DASH_IGN_GREEN_MIN           12.0f
#define DASH_IGN_GREEN_MAX           32.0f

/* Fuel level */
#define DASH_FUEL_LOW                15.0f
#define DASH_FUEL_CAUTION            30.0f

/* UART staleness (side displays) */
#define DASH_STALE_MS                500u
#define DASH_STALE_OFF_MS            2000u

#ifdef __cplusplus
}
#endif

#endif /* LEFT_DASH_DATA_H */
