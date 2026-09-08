# lvgl-widgets

LVGL 可复用自定义控件与交互效果合集。每个效果独立成一个子目录，可单独取用。

## 效果清单

| 效果 | 目录 | 说明 |
|------|------|------|
| 蜂窝菜单 Hex Menu | [`hex-menu/`](hex-menu/) | 无界二维蜂窝菜单：37 项彩色气泡六边形环面平铺、中心放大镜、拖拽惯性、弹簧吸附、滚轮缩放、点击气泡原地丝滑展开成卡片、收起时卡片朝气泡方向「神灯」吸走 |

## 构建方式

各效果为独立的静态库工程（`lib-ui`），通过 SDL 模拟器工程 `lv_port_pc_vscode-master` 的
`LVGL_PRO_PROJECT_DIR` 指向对应子目录来构建运行。以蜂窝菜单为例：

```
cmake -B build -DLVGL_PRO_PROJECT_DIR=<绝对路径>/hex-menu
cmake --build build -j
./bin/main
```

所有源文件在 `-Wall -Wextra -Werror` 下零警告编译（由 `lib-ui` 目标强制）。

## 运行须知：刷新率必须设 60fps

模拟器工程的 `lv_conf.h` 默认 `LV_DEF_REFR_PERIOD 33`（≈30fps），该值同时决定显示刷新与
**动画步进周期**。30fps 下卡片展开/神灯收起等快速动画会明显跳帧、不丝滑。

**运行本合集的效果前，请把它改为 16（≈60fps）：**

```c
/* lv_port_pc_vscode-master/lv_conf.h */
#define LV_DEF_REFR_PERIOD 16
```

该文件属于模拟器工程、不在本仓库内，故此设置无法随本仓库一起分发——每次在新环境
搭建模拟器都需手动确认。真机移植时按芯片性能自行权衡（60fps 会增加 CPU/GPU 负担）。
