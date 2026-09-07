cmake_minimum_required(VERSION 3.12.4)

add_library(lib-ui STATIC
    ui.c
    hex_menu/hex_grid.c
    hex_menu/hex_physics.c
    hex_menu/lv_hex_menu.c
)

target_include_directories(lib-ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hex_menu
)

target_compile_definitions(lib-ui PRIVATE LV_CONF_INCLUDE_SIMPLE)

target_link_libraries(lib-ui PUBLIC lvgl)

if(NOT MSVC)
    target_link_libraries(lib-ui PUBLIC m)
endif()

# 强制本工程源文件在最严格的警告级别下编译（警告即错误）。
# 使用 PRIVATE，避免传播到链接 lib-ui 的模拟器 main target。
if(NOT MSVC)
    target_compile_options(lib-ui PRIVATE -Wall -Wextra -Werror)
endif()
