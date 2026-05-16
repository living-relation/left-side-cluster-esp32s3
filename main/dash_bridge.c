/**
 * dash_bridge.c — TrackCluster UART encode/decode + CRC-8
 *
 * Implements the functions declared in center-dash_data.h.
 * Center cluster includes this file to ENCODE frames for TX.
 * Side clusters include their copy to DECODE frames from RX.
 *
 * 32-byte frame layout:
 *   [0]     SOF  = 0xA5
 *   [1]     FRAME_TYPE (0x01=LEFT, 0x02=RIGHT)
 *   [2..3]  sequence (uint16 LE)
 *   [4..29] payload (26 bytes, layout depends on FRAME_TYPE)
 *   [30]    CRC-8 over bytes [1..29], poly 0x07, init 0xFF
 *   [31]    EOF  = 0x5A
 */

#include "left-dash_data.h"
#include <string.h>

/* ── Global dash data instance ─────────────────────────────────────────── */
volatile dash_data_t dash = {0};

/* ── CRC-8 (poly 0x07, init 0xFF) ─────────────────────────────────────── */
uint8_t dash_crc8(const uint8_t *data, size_t n) {
    uint8_t c = 0xFF;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++)
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
}

/* ── Little-endian helpers ─────────────────────────────────────────────── */
static inline void put_u16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static inline void put_i16(uint8_t *p, int16_t v)  { put_u16(p, (uint16_t)v); }
static inline void put_u32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

static inline uint16_t get_u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static inline int16_t  get_i16(const uint8_t *p) { return (int16_t)get_u16(p); }
static inline uint32_t get_u32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/* ── LEFT payload encoder (FRAME_TYPE 0x01, sent on UART1) ─────────────── *
 *
 * Bytes  Field               Encoding
 * 0-1    mph × 10            uint16 LE
 * 2-3    oil_temp × 10       int16  LE
 * 4-5    oil_press × 10      uint16 LE
 * 6-7    fuel_press × 10     uint16 LE
 * 8-9    fuel_level × 10     uint16 LE
 * 10-11  rpm                 uint16 LE
 * 12     gear                int8
 * 13     odo_mode            uint8
 * 14-17  odo × 10            uint32 LE
 * 18-21  trip_active × 10    uint32 LE
 * 22-23  flags               uint16 LE
 * 24-25  reserved            0
 */
void dash_encode_left(uint8_t out[UART_BRIDGE_FRAME_LEN], const dash_data_t *d, uint16_t seq) {
    memset(out, 0, UART_BRIDGE_FRAME_LEN);

    out[0] = UART_BRIDGE_SOF;
    out[1] = UART_BRIDGE_TYPE_LEFT;
    put_u16(&out[2], seq);

    /* Payload at offset 4 */
    uint8_t *p = &out[4];
    put_u16(&p[0],  (uint16_t)(d->mph * 10.0f));
    put_i16(&p[2],  (int16_t)(d->oil_temp * 10.0f));
    put_u16(&p[4],  (uint16_t)(d->oil_press * 10.0f));
    put_u16(&p[6],  (uint16_t)(d->fuel_press * 10.0f));
    put_u16(&p[8],  (uint16_t)(d->fuel_level * 10.0f));
    put_u16(&p[10], (uint16_t)d->rpm);
    p[12] = (uint8_t)d->gear;
    p[13] = d->odo_mode;

    float active_trip = (d->odo_mode == DASH_TRIP_B) ? d->trip_b : d->trip_a;
    put_u32(&p[14], (uint32_t)(d->odo * 10.0f));
    put_u32(&p[18], (uint32_t)(active_trip * 10.0f));
    put_u16(&p[22], d->flags);
    /* p[24..25] = reserved = 0 (already zeroed) */

    out[30] = dash_crc8(&out[1], 29);
    out[31] = UART_BRIDGE_EOF;
}

/* ── RIGHT payload encoder (FRAME_TYPE 0x02, sent on UART2) ────────────── *
 *
 * Bytes  Field               Encoding
 * 0-1    lambda × 1000       uint16 LE
 * 2-3    (boost+30) × 100    uint16 LE (biased so vacuum encodes positive)
 * 4-5    coolant_temp × 10   int16  LE
 * 6-7    ign_adv × 10        int16  LE
 * 8-9    iat × 10            int16  LE
 * 10-11  rpm                 uint16 LE
 * 12-13  flags               uint16 LE
 * 14-25  reserved            0
 */
void dash_encode_right(uint8_t out[UART_BRIDGE_FRAME_LEN], const dash_data_t *d, uint16_t seq) {
    memset(out, 0, UART_BRIDGE_FRAME_LEN);

    out[0] = UART_BRIDGE_SOF;
    out[1] = UART_BRIDGE_TYPE_RIGHT;
    put_u16(&out[2], seq);

    uint8_t *p = &out[4];
    put_u16(&p[0],  (uint16_t)(d->lambda * 1000.0f));
    put_u16(&p[2],  (uint16_t)((d->boost + 30.0f) * 100.0f));
    put_i16(&p[4],  (int16_t)(d->coolant_temp * 10.0f));
    put_i16(&p[6],  (int16_t)(d->ign_adv * 10.0f));
    put_i16(&p[8],  (int16_t)(d->iat * 10.0f));
    put_u16(&p[10], (uint16_t)d->rpm);
    put_u16(&p[12], d->flags);
    /* p[14..25] = reserved = 0 (already zeroed) */

    out[30] = dash_crc8(&out[1], 29);
    out[31] = UART_BRIDGE_EOF;
}

/* ── LEFT payload decoder (for left-side cluster) ──────────────────────── */
bool dash_decode_left(const uint8_t in[UART_BRIDGE_FRAME_LEN], dash_data_t *d, uint16_t *seq_out) {
    if (in[0] != UART_BRIDGE_SOF || in[31] != UART_BRIDGE_EOF)
        return false;
    if (in[1] != UART_BRIDGE_TYPE_LEFT)
        return false;

    uint8_t crc = dash_crc8(&in[1], 29);
    if (crc != in[30])
        return false;

    *seq_out = get_u16(&in[2]);

    const uint8_t *p = &in[4];
    d->mph        = get_u16(&p[0]) * 0.1f;
    d->oil_temp   = get_i16(&p[2]) * 0.1f;
    d->oil_press  = get_u16(&p[4]) * 0.1f;
    d->fuel_press = get_u16(&p[6]) * 0.1f;
    d->fuel_level = get_u16(&p[8]) * 0.1f;
    d->rpm        = (float)get_u16(&p[10]);
    d->gear       = (int8_t)p[12];
    d->odo_mode   = p[13];
    d->odo        = get_u32(&p[14]) * 0.1f;
    /* trip_active goes into the active trip slot based on odo_mode */
    float trip_val = get_u32(&p[18]) * 0.1f;
    if (d->odo_mode == DASH_TRIP_B)
        d->trip_b = trip_val;
    else
        d->trip_a = trip_val;
    d->flags = get_u16(&p[22]);

    return true;
}

/* ── RIGHT payload decoder (for right-side cluster) ────────────────────── */
bool dash_decode_right(const uint8_t in[UART_BRIDGE_FRAME_LEN], dash_data_t *d, uint16_t *seq_out) {
    if (in[0] != UART_BRIDGE_SOF || in[31] != UART_BRIDGE_EOF)
        return false;
    if (in[1] != UART_BRIDGE_TYPE_RIGHT)
        return false;

    uint8_t crc = dash_crc8(&in[1], 29);
    if (crc != in[30])
        return false;

    *seq_out = get_u16(&in[2]);

    const uint8_t *p = &in[4];
    d->lambda      = get_u16(&p[0]) * 0.001f;
    d->boost       = (get_u16(&p[2]) * 0.01f) - 30.0f;
    d->coolant_temp = get_i16(&p[4]) * 0.1f;
    d->ign_adv     = get_i16(&p[6]) * 0.1f;
    d->iat         = get_i16(&p[8]) * 0.1f;
    d->rpm         = (float)get_u16(&p[10]);
    d->flags       = get_u16(&p[12]);

    return true;
}
