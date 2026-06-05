idf_component_register(
    SRCS
        "main.c"
        "bsp.c"
        "uart_rx.c"
        "alarm_task.c"
        "dash_data.c"
        "ui/ui.c"
        "ui/ui_mph_arc.c"
        "ui/ui_mini_arcs.c"
        "ui/ui_boot.c"
        "ui/fonts/aerospace_88.c"
        "ui/fonts/aerospace_28.c"
        "ui/fonts/aerospace_32.c"
        "ui/fonts/aerospace_36.c"
        "ui/fonts/racehead_14.c"
        "ui/fonts/racehead_18.c"
        "ui/fonts/racehead_10.c"
        "ui/assets/toyota_logo.c"

    INCLUDE_DIRS
        "."
        "ui"

    PRIV_REQUIRES
        driver
        esp_lcd
        esp_driver_gpio
        esp_driver_uart
        esp_driver_i2c
        esp_psram
        nvs_flash
)

# SquareLine-generated font files use #ifdef LV_LVGL_H_INCLUDE_SIMPLE to select
# the LVGL include path. With the component manager, lvgl.h is at the component
# root, so the "simple" form is correct.
target_compile_definitions(__idf_main PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
