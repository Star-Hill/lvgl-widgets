# lvgl-widgets

LVGL 可复用自定义控件与交互效果合集。每个效果独立成一个子目录，可单独取用。

## 效果清单

| 效果 | 目录 | 说明 |
|------|------|------|
| 蜂窝菜单 Hex Menu | [`hex-menu/`](hex-menu/) | 无界二维蜂窝菜单：37 项彩色气泡六边形环面平铺、中心放大镜、拖拽惯性、弹簧吸附、滚轮缩放、点击气泡原地丝滑展开成卡片、收起时卡片朝气泡方向「神灯」吸走 |
| 日夜切换开关 Day / Night Switch | [`day-night-switch/`](day-night-switch/) | 移植自 uiverse.io 的纯 CSS 主题开关：日月位移并自转一圈、底色与日月颜色插值、月坑淡入、云随日月飘出视野、星空自上方滑入并持续闪烁。四角星用 ThorVG 还原原 SVG 路径 |
| 文件树下拉 File Tree Dropdown | [`file-tree-dropdown/`](file-tree-dropdown/) | 移植自一个 Tailwind CSS 组件：白色卡片作触发器，点击后在下方淡入带层级缩进的目录列表。双色文件夹图标由 ThorVG 绘制 |
| 内置符号总览 Symbol Gallery | [`symbol-gallery/`](symbol-gallery/) | 工具页：把 LVGL 全部内置 `LV_SYMBOL_*` 连同名字排成网格一屏展示，便于挑图标。名单由 `lv_symbol_def.h` 提取生成 |

### 移植 CSS 组件时的两条经验

- **`hover` 在触摸屏不存在**。原组件若靠 `:hover` / `group-hover` 触发，需改为点击切换或按压态。
- **`absolute` 没有直接对应物**。CSS 的绝对定位浮层脱离父容器，而 LVGL 中子对象一律被父对象
  裁剪（`LV_OBJ_FLAG_OVERFLOW_VISIBLE` 只影响 ext draw size，救不了越界）。下拉、气泡这类浮层
  应挂到 `lv_layer_top()`，并在每次显示时相对触发器重新对齐——它不会随组件一起移动，
  也不会被级联删除，需自行回收。

## 构建方式

各效果为独立的静态库工程（`lib-ui`），通过 SDL 模拟器工程 `lv_port_pc_vscode-master` 的
`LVGL_PRO_PROJECT_DIR` 指向对应子目录来构建运行。**一次只能指向一个子工程**，切换效果
即重新指向并重新构建。以蜂窝菜单为例：

```
cd <模拟器工程>/lv_port_pc_vscode-master
cmake -B build -DLVGL_PRO_PROJECT_DIR=<本仓库绝对路径>/hex-menu
cmake --build build -j
./bin/main
```

把末尾的目录名换成 `day-night-switch`、`file-tree-dropdown`、`symbol-gallery`
即可切换到其它效果，重新构建后运行同一个 `./bin/main`。

注意必须在**模拟器工程目录**下构建。子工程只产出 `lib-ui` 静态库，lvgl 由模拟器提供，
直接在子目录构建会失败——各子工程的 `CMakeLists.txt` 已加保护，误操作时会打印正确命令。

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

## 怎么量帧率与内存

同样在模拟器的 `lv_conf.h` 里打开性能与内存监视，右下角显示 FPS / CPU / 单帧渲染耗时，
左下角显示 LVGL 堆占用：

```c
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR  1
```

读数时注意：**FPS 的上限由 `LV_DEF_REFR_PERIOD` 锁定**（16ms 即封顶 60），所以满帧不代表
没有余量——真正看余量要看**单帧渲染耗时**相对每帧预算（16ms）还剩多少。

移植到真机后同样打开这两项，即可对比同一效果在 PC 与嵌入式上的实际代价。
