#include <stdio.h>
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
#include "ui/ui.h"

int coolant_temp = 100;
int oil_temp = 142; 
int oil_pressure = 2; 
int fuel_pressure = 34; 
float fuel_level = 100.0f;       
bool coolant_temp_increasing = true; 
bool oil_temp_increasing = true; 
bool oil_pressure_increasing = true;  
bool fuel_pressure_increasing = true;
bool fuel_level_increasing = true;

static lv_color_t green_color;
static lv_color_t red_color;
static lv_color_t blue_color;
static lv_color_t orange_color;


static inline int map_int(int x,
                          int in_min, int in_max,
                          int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min)
           / (in_max - in_min)
           + out_min;
}

static inline int constrain_int(int x, int low, int high){
    if (x < low) return low;
    if (x > high) return high;
    return x;
}

static void init_label_styles(void){

    blue_color = lv_palette_main(LV_PALETTE_BLUE);
    green_color = lv_palette_main(LV_PALETTE_GREEN);
    red_color = lv_palette_main(LV_PALETTE_RED);
    orange_color = lv_palette_main(LV_PALETTE_DEEP_ORANGE);
}

static void update_label_if_needed(lv_obj_t *label, int new_value, lv_color_t new_color) {
    char buf[12];
    sprintf(buf, "%d", new_value);

    // Only update text if changed
    const char *old_text = lv_label_get_text(label);
    if (strcmp(old_text, buf) != 0) {
        lv_label_set_text(label, buf);
    }

    // Only update color if changed
    lv_color_t old_color = lv_obj_get_style_text_color(label, LV_PART_MAIN);
    if (old_color.full != new_color.full) {
        lv_obj_set_style_text_color(label, new_color, LV_PART_MAIN);
    }

}

void simulate_guage_movement(int gauge_value, int max_value, int min_value, int step, lv_obj_t * text, const char * type, bool isIncreasing){
    
    gauge_value += (isIncreasing ? step : -step);
    lv_color_t color = green_color;
    
    if (gauge_value >= max_value || gauge_value <= min_value){
        isIncreasing = !isIncreasing;
        gauge_value = constrain_int(gauge_value, 0, max_value);
    }
    if(strcmp(type, "fuel_pressure") == 0) {
        fuel_pressure = gauge_value;
        fuel_pressure_increasing = isIncreasing;
        color = (gauge_value > 70 || gauge_value < 30) ? red_color : green_color;
    } else if(strcmp(type, "coolant_temp") == 0) {
        coolant_temp = gauge_value;
        coolant_temp_increasing = isIncreasing;
        color = (gauge_value < 150) ? blue_color : ((gauge_value > 205) ? red_color : green_color);
    } else if(strcmp(type, "oil_temp") == 0) {
        oil_temp = gauge_value;
        oil_temp_increasing = isIncreasing;;
        color = (gauge_value < 150) ? blue_color : ((gauge_value > 220) ? red_color : green_color);
    } else if(strcmp(type, "oil_pressure") == 0) {
        oil_pressure = gauge_value;
        oil_pressure_increasing = isIncreasing;
        color = (gauge_value > 125 || gauge_value < 25) ? red_color : green_color;
    } 

    update_label_if_needed(text, gauge_value, color);
}

void simulate_fuel_arc(float *fuel_value, lv_obj_t *arc){
    const float step_per_second = 1.0f;

    static bool increasing = true;

    //Update for fuel gauge color, slows down FPS a little
    // lv_color_t new_color = green_color;
    // new_color = (*fuel_value < 20) ? red_color : ((*fuel_value < 35) ? orange_color : green_color) ;

    // // Only update color if changed
    // lv_color_t old_color = lv_obj_get_style_arc_color(arc, LV_PART_INDICATOR);
    // if (old_color.full != new_color.full) {
    //     lv_obj_set_style_arc_color(fuel_arc, new_color, LV_PART_INDICATOR);
    // }

    // update fuel
    if(increasing) *fuel_value += step_per_second;
    else          *fuel_value -= step_per_second;

    // reverse at bounds
    if(*fuel_value >= 100.0f) increasing = false;
    if(*fuel_value <= 0.0f)   increasing = true;

    lv_arc_set_value(arc, (int)*fuel_value);
}

void gauge_timer(lv_timer_t * t) {
    simulate_guage_movement(coolant_temp, 250, 100, 4, ui_Label5, "coolant_temp", coolant_temp_increasing);
    simulate_guage_movement(fuel_pressure, 100, 0, 2, ui_Label6, "fuel_pressure", fuel_pressure_increasing);
    simulate_guage_movement(oil_temp, 250, 100, 5, ui_Label7, "oil_temp", oil_temp_increasing);
    simulate_guage_movement(oil_pressure, 150, 0, 3, ui_Label8, "oil_pressure", oil_pressure_increasing);
}

void arc_timer(lv_timer_t * t){
    simulate_fuel_arc(&fuel_level, fuel_arc);
}


void app_main(void){   
    I2C_Init();
    EXIO_Init();
    LCD_Init();
    Touch_Init();
    LVGL_Init();

    lv_init();
    ui_init();
    init_label_styles();
    lv_timer_create(gauge_timer, 200, NULL);
    lv_timer_create(arc_timer, 100, NULL);
}