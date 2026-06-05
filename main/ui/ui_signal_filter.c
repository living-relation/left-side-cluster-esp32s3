/**
 * Display-only smoothing for left-cluster gauges (raw g_dash unchanged).
 *
 * MPH: instant attack, snap on ≥2 MPH decel, 35% blend on small downward jitter.
 * Oil / fuel pressure: slow rise (14%), fast fall (38%), 10 PSI snap; label hysteresis.
 * Fuel level: slow rise (20%), slower fall (12%), 8% snap; label hysteresis.
 * Oil temp: symmetric EMA (15% rise/fall), 5 °F snap — same inertia as ECT.
 *
 * Bench mode bypasses these at the call site (snappy demo, paint-sync stepping).
 * Pressure arc alarm colors use raw PSI in ui_mini_arcs (filter lags on decel).
 */
#include "ui_signal_filter.h"
#include <math.h>

#define UNINIT (-999.0f)

#define MPH_DECEL_SNAP_MPH   2.0f
#define MPH_RELEASE_BLEND    0.35f

#define OIL_TEMP_BLEND       0.15f
#define OIL_TEMP_SNAP_F      5.0f

#define PRESS_RISE_BLEND     0.14f
#define PRESS_FALL_BLEND     0.38f
#define PRESS_SNAP_PSI       10.0f
#define PRESS_LABEL_STEP_PSI 2.0f

#define FUEL_ATTACK_BLEND    0.20f
#define FUEL_RELEASE_BLEND   0.12f
#define FUEL_DECEL_SNAP_PCT  8.0f
#define FUEL_LABEL_STEP_PCT  1.2f

static float s_mph           = UNINIT;
static float s_oil_temp      = UNINIT;
static float s_oil_press     = UNINIT;
static float s_fuel_press    = UNINIT;
static float s_fuel_level    = UNINIT;
static int   s_oil_press_lbl = -1;
static int   s_fuel_press_lbl = -1;
static int   s_fuel_level_lbl = -1;

static float ema_asym(float raw, float *state, float rise_blend, float fall_blend, float snap)
{
    if (*state < -900.0f) {
        *state = raw;
        return raw;
    }
    float delta = raw - *state;
    if (fabsf(delta) >= snap) {
        *state = raw;
    } else if (delta > 0.0f) {
        *state += delta * rise_blend;
    } else {
        *state += delta * fall_blend;
    }
    return *state;
}

void ui_filter_reset(void)
{
    s_mph            = UNINIT;
    s_oil_temp       = UNINIT;
    s_oil_press      = UNINIT;
    s_fuel_press     = UNINIT;
    s_fuel_level     = UNINIT;
    s_oil_press_lbl  = -1;
    s_fuel_press_lbl = -1;
    s_fuel_level_lbl = -1;
}

float ui_filter_mph(float raw)
{
    if (s_mph < -900.0f) {
        s_mph = raw;
        return raw;
    }
    if (raw > s_mph) {
        s_mph = raw;
    } else if ((s_mph - raw) >= MPH_DECEL_SNAP_MPH) {
        s_mph = raw;
    } else {
        s_mph += (raw - s_mph) * MPH_RELEASE_BLEND;
    }
    return s_mph;
}

float ui_filter_oil_temp(float raw)
{
    return ema_asym(raw, &s_oil_temp, OIL_TEMP_BLEND, OIL_TEMP_BLEND, OIL_TEMP_SNAP_F);
}

float ui_filter_oil_press(float raw)
{
    return ema_asym(raw, &s_oil_press, PRESS_RISE_BLEND, PRESS_FALL_BLEND, PRESS_SNAP_PSI);
}

float ui_filter_fuel_press(float raw)
{
    return ema_asym(raw, &s_fuel_press, PRESS_RISE_BLEND, PRESS_FALL_BLEND, PRESS_SNAP_PSI);
}

float ui_filter_fuel_level(float raw)
{
    return ema_asym(raw, &s_fuel_level, FUEL_ATTACK_BLEND, FUEL_RELEASE_BLEND, FUEL_DECEL_SNAP_PCT);
}

static int hysteresis_label(float disp, int *shown, float step)
{
    if (*shown < 0) {
        *shown = (int)(disp + 0.5f);
        return *shown;
    }
    int target = (int)(disp + 0.5f);
    if (target > *shown && (disp - (float)*shown) >= step) {
        *shown = target;
    } else if (target < *shown && ((float)*shown - disp) >= step) {
        *shown = target;
    }
    return *shown;
}

int ui_filter_oil_press_label(float disp)
{
    return hysteresis_label(disp, &s_oil_press_lbl, PRESS_LABEL_STEP_PSI);
}

int ui_filter_fuel_press_label(float disp)
{
    return hysteresis_label(disp, &s_fuel_press_lbl, PRESS_LABEL_STEP_PSI);
}

int ui_filter_fuel_level_label(float disp)
{
    return hysteresis_label(disp, &s_fuel_level_lbl, FUEL_LABEL_STEP_PCT);
}
