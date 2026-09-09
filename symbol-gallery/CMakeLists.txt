cmake_minimum_required(VERSION 3.12.4)

# 本目录是 lvgl-widgets 合集的子工程，不能独立构建：
# lvgl 库及其头文件由 SDL 模拟器工程提供，这里只产出 lib-ui 静态库。
if(NOT TARGET lvgl)
    message(FATAL_ERROR
        "\n"
        "这个目录不能单独构建（没有 lvgl target，编译会报 lvgl.h not found）。\n"
        "请改为在 SDL 模拟器工程 lv_port_pc_vscode-master 下构建，\n"
        "并用 LVGL_PRO_PROJECT_DIR 指向本目录：\n"
        "\n"
        "  cd <路径>/lv_port_pc_vscode-master\n"
        "  cmake -B build -DLVGL_PRO_PROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}\n"
        "  cmake --build build -j\n"
        "  ./bin/main\n")
endif()

add_library(lib-ui STATIC
    ui.c
)

target_include_directories(lib-ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
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
