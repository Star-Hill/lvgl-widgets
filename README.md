# lvgl-widgets

LVGL 可复用自定义控件与交互效果合集。每个效果独立成一个子目录，可单独取用。

## 效果清单

| 效果 | 目录 | 说明 |
|------|------|------|
| 蜂窝菜单 Hex Menu | [`hex-menu/`](hex-menu/) | 无界二维蜂窝菜单：37 项彩色气泡六边形环面平铺、中心放大镜、拖拽惯性、弹簧吸附、滚轮缩放、点击气泡原地丝滑展开成卡片 |

## 构建方式

各效果为独立的静态库工程（`lib-ui`），通过 SDL 模拟器工程 `lv_port_pc_vscode-master` 的
`LVGL_PRO_PROJECT_DIR` 指向对应子目录来构建运行。以蜂窝菜单为例：

```
cmake -B build -DLVGL_PRO_PROJECT_DIR=<绝对路径>/hex-menu
cmake --build build -j
./bin/main
```

所有源文件在 `-Wall -Wextra -Werror` 下零警告编译（由 `lib-ui` 目标强制）。
