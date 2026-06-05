#pragma once

/** Display-only smoothing; g_dash fields stay raw for UART / alarms. */

float ui_filter_mph(float raw);
float ui_filter_oil_temp(float raw);
float ui_filter_oil_press(float raw);
float ui_filter_fuel_press(float raw);
float ui_filter_fuel_level(float raw);

/** Hysteresis-rounded integer labels for filtered pressure / fuel %. */
int ui_filter_oil_press_label(float disp);
int ui_filter_fuel_press_label(float disp);
int ui_filter_fuel_level_label(float disp);

void ui_filter_reset(void);
