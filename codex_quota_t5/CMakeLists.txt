# Codex Quota Monitor - T5AI-Board
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
set(APP_NAME codex_quota)

set(APP_SRCS
    ${APP_PATH}/src/tuya_main.c
    ${APP_PATH}/src/codex_http.c
    ${APP_PATH}/src/codex_ui.c
    ${APP_PATH}/src/codex_mqtt.c
    ${APP_PATH}/src/codex_serial.c
    ${APP_PATH}/src/fonts/lv_font_cn_16.c
    # 触摸屏驱动（SDK 的 CONFIG_TP 默认未启用，直接编译进 app）
    ${APP_PATH}/src/drv_tp.c
    ${APP_PATH}/src/tp_driver.c
    ${APP_PATH}/src/bk_queue.c
    ${APP_PATH}/src/sim_i2c_driver_v2.c
)

add_library(${EXAMPLE_LIB} STATIC ${APP_SRCS})

target_include_directories(${EXAMPLE_LIB} PUBLIC
    ${APP_PATH}/include
    ${APP_PATH}/src
    # Beken SDK driver & SoC include paths (for compiling TP driver sources)
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/build/bk7258/tuya_app/bk7258_ap/config
)

# 自动添加 Beken SDK driver 模块所有子目录（TP 驱动依赖链很深）
set(BK_DRIVER_BASE ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/driver)
file(GLOB BK_DRIVER_SUBDIRS "${BK_DRIVER_BASE}/*")
foreach(subdir ${BK_DRIVER_SUBDIRS})
    if(IS_DIRECTORY ${subdir})
        target_include_directories(${EXAMPLE_LIB} PUBLIC ${subdir})
    endif()
endforeach()

target_include_directories(${EXAMPLE_LIB} PUBLIC
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/driver/include
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/driver/include/bk_private
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/driver/include/bk_private/legacy
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/soc/bk7258_ap
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/soc/bk7258_ap/soc
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/soc/bk7258_ap/hal
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/soc/common/hal/include
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/soc/common/soc/include
    ${CMAKE_SOURCE_DIR}/platform/T5AI/t5_os/ap/middleware/driver/bk7258_ap
)

# MQTT config comes from Kconfig only — do NOT hardcode here.
# LVGL fonts + display
target_compile_definitions(${EXAMPLE_LIB} PUBLIC
    LV_FONT_MONTSERRAT_14=1
    LV_FONT_MONTSERRAT_16=1
    LV_FONT_MONTSERRAT_20=1
    LV_FONT_MONTSERRAT_24=1
    LV_FONT_MONTSERRAT_32=1
    LV_FONT_MONTSERRAT_34=1
    LV_DRAW_BUF_PARTS=10
    DISPLAY_NAME="display"
)

# TP 驱动源文件来自 Beken SDK，其 LOGD 宏会触发 -Wunused-value，
# 需要为这些文件单独关闭该 warning。
set_source_files_properties(
    ${APP_PATH}/src/drv_tp.c
    ${APP_PATH}/src/tp_driver.c
    ${APP_PATH}/src/bk_queue.c
    ${APP_PATH}/src/sim_i2c_driver_v2.c
    PROPERTIES COMPILE_FLAGS "-Wno-unused-value -Wno-old-style-declaration -Wno-unused-function"
)
