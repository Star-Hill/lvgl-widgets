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
