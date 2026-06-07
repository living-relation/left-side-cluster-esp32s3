# Patch esp_lvgl_port RGB vsync callback into IRAM when CONFIG_LCD_RGB_ISR_IRAM_SAFE=y.
set(_lvgl_disp "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c")
if(NOT EXISTS "${_lvgl_disp}")
    return()
endif()
file(READ "${_lvgl_disp}" _lvgl_disp_content)
if(_lvgl_disp_content MATCHES "IRAM_ATTR lvgl_port_flush_rgb_vsync_ready_callback")
    return()
endif()
string(REPLACE
    "static bool lvgl_port_flush_rgb_vsync_ready_callback"
    "static bool IRAM_ATTR lvgl_port_flush_rgb_vsync_ready_callback"
    _lvgl_disp_content "${_lvgl_disp_content}")
file(WRITE "${_lvgl_disp}" "${_lvgl_disp_content}")
message(STATUS "esp_lvgl_port_iram_patch: applied IRAM_ATTR to RGB vsync callback")
