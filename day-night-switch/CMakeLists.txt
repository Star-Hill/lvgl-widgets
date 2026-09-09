cmake_minimum_required(VERSION 3.12.4)

add_library(lib-ui STATIC
    ui.c
    day_night_switch/lv_day_night_switch.c
)

target_include_directories(lib-ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/day_night_switch
)

target_compile_definitions(lib-ui PRIVATE LV_CONF_INCLUDE_SIMPLE)

target_link_libraries(lib-ui PUBLIC lvgl)

if(NOT MSVC)
    target_link_libraries(lib-ui PUBLIC m)
endif()

# 与 hex-menu 一致：本工程源文件在最严格警告级别下编译（警告即错误）
if(NOT MSVC)
    target_compile_options(lib-ui PRIVATE -Wall -Wextra -Werror)
endif()
