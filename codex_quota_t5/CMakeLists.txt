# Codex Quota Monitor - T5AI-Board
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
set(APP_NAME codex_quota)

set(APP_SRCS
    ${APP_PATH}/src/tuya_main.c
    ${APP_PATH}/src/codex_http.c
    ${APP_PATH}/src/codex_ui.c
    ${APP_PATH}/src/codex_mqtt.c
)

add_library(${EXAMPLE_LIB} STATIC ${APP_SRCS})

target_include_directories(${EXAMPLE_LIB} PUBLIC
    ${APP_PATH}/include
    ${APP_PATH}/src
)

# MQTT broker config + LVGL fonts
target_compile_definitions(${EXAMPLE_LIB} PUBLIC
    MQTT_HOST=\"10.13.220.28\"
    MQTT_PORT=1883
    LV_FONT_MONTSERRAT_14=1
    LV_FONT_MONTSERRAT_16=1
    LV_FONT_MONTSERRAT_20=1
    LV_FONT_MONTSERRAT_24=1
    LV_FONT_MONTSERRAT_32=1
    LV_FONT_MONTSERRAT_34=1
    LV_DRAW_BUF_PARTS=10
    DISPLAY_NAME="display"
)
