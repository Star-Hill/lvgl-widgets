# 蜂窝丝滑菜单（Hex Menu）设计文档

日期：2026-09-07
状态：待实现

## 1. 目标

在 800×480 屏幕上实现一个类 Apple Watch 的蜂窝气泡菜单：37 个圆形气泡按正六边形网格排布，可朝任意方向无界拖拽，屏幕中心呈放大镜效果，松手后惯性滑行并弹簧吸附到最近一格，最靠近中心的气泡高亮，点击触发回调。

作为**独立 UI 工程**开发，通过 `lv_port_pc_vscode` 已有的 `LVGL_PRO_PROJECT_DIR` 机制挂载到 SDL 模拟器上运行，不依赖模拟器本身，可整体移植到 MCU。

目标环境：LVGL 9.6-dev，C99，无动态内存增长，无浮点强依赖（浮点仅用于每帧少量计算）。

## 2. 工程结构

```
LVGL_PRO_Workspace/
├── lv_port_pc_vscode-master/          模拟器（仅改 src/main.c 两行）
└── hex_menu_ui/                       本工程
    ├── CMakeLists.txt                 生成 lib-ui 静态库
    ├── ui.h / ui.c                    ui_init() 唯一入口，装配菜单数据
    ├── hex_menu/
    │   ├── lv_hex_menu.h              公共 API
    │   ├── lv_hex_menu.c              widget 主体：对象池 + 每帧刷新
    │   ├── hex_grid.h / hex_grid.c    纯数学：轴向坐标 ↔ 像素、37 格无缝平铺
    │   └── hex_physics.h / hex_physics.c  纯数学：惯性衰减 + 临界阻尼弹簧
    ├── test/
    │   └── test_hex_math.c            纯 gcc 编译的断言测试，不链接 LVGL
    └── docs/superpowers/specs/        本文档
```

`hex_grid` 与 `hex_physics` **不 include 任何 LVGL 头文件**，是可独立编译、独立测试的纯函数模块。这是本设计的核心隔离边界：几何与物理的正确性可以脱离 GUI 验证，`lv_hex_menu.c` 只负责把计算结果映射到 LVGL 对象上。

### 构建集成

`hex_menu_ui/CMakeLists.txt` 生成名为 `lib-ui` 的静态库（名字由模拟器顶层 `CMakeLists.txt:128` 写死要求）。挂载方式：

```bash
cmake -B build -G "MinGW Makefiles" \
  -DLVGL_PRO_PROJECT_DIR="D:/Program_Workspace/GUI_Workspace/LVGL_PRO_Workspace/hex_menu_ui" \
  ...（其余 SDL / 编译器参数同前）
```

模拟器侧改动仅限 `src/main.c`：
- `sdl_hal_init(320, 480)` → `sdl_hal_init(800, 480)`
- `lv_demo_widgets()` → `ui_init()`，并 include `ui.h`

示例/demo 源码保留在仓库中，只是不再被调用。

## 3. 核心数学

### 3.1 六边形网格坐标

采用 pointy-top 轴向坐标 `(q, r)`（整数），转像素：

```
x = S · √3 · (q + r/2)
y = S · 1.5 · r
```

`S` 为网格特征尺寸（六边形外接圆半径），是滚轮缩放唯一改变的量。相邻列间距 `S·√3`，相邻行间距 `S·1.5`。

反向（像素 → 最近格子）用标准的立方坐标取整：先算浮点 `(q, r)`，转立方坐标 `(x, y, z) = (q, -q-r, r)`，三者分别四舍五入后，把舍入误差最大的那一维用另两维反推修正。

### 3.2 37 格无缝平铺（已验证）

37 是 Löschian 数：`37 = 4² + 4·3 + 3²`（a=4, b=3）。因此 37 个格子可在无限六边形网格上构成一个**完美的环面铺砌**——朝任意方向拖拽，图案严丝合缝地周期重复，不存在接缝或错位。

晶格向量：`L1 = (a, b) = (4, 3)`，`L2 = (-b, a+b) = (-3, 7)`

槽位索引映射：

```c
#define HEX_SLOT_CNT 37
static inline int hex_slot_index(int q, int r)
{
    int v = (27 * q + r) % HEX_SLOT_CNT;
    return v < 0 ? v + HEX_SLOT_CNT : v;
}
```

系数 27 的来源：要求两个晶格向量都映射到 0，即 `4x + 3y ≡ 0 (mod 37)` 且 `-3x + 7y ≡ 0 (mod 37)`。由前者得 `x ≡ -3y·4⁻¹`，`4⁻¹ mod 37 = 28`，故 `x ≡ -84y ≡ 27y`；取 `y = 1` 得 `x = 27`。代入后者验证 `-81 + 7 = -74 = -2·37 ≡ 0` ✓。

已用穷举程序验证的五条性质（实现时须由 `test_hex_math.c` 覆盖）：

1. `37 == a² + ab + b²`
2. `hex_slot_index(4,3) == 0` 且 `hex_slot_index(-3,7) == 0`
3. 在 `q,r ∈ [-60, 60]` 全范围内恰好产生 37 个不同索引，不多不少
4. 对任意 `(q,r)` 与任意整数 `m,n`，`hex_slot_index(q,r) == hex_slot_index(q + m·L1q + n·L2q, r + m·L1r + n·L2r)`（晶格平移不变性）
5. 半径为 3 的正六边形区域恰含 37 格且索引互不重复——即 37 个菜单项本身正好拼成一个完整六边形，这是选择 37 而非 19 的额外好处

### 3.3 放大镜曲线

设影响半径为 `R`，`d` 为气泡在**未经压缩的世界坐标下**离焦点的距离：

```c
d = hypotf(world_x - fx, world_y - fy);
```

必须用未压缩距离，否则 `d` 依赖屏幕坐标、而屏幕坐标又依赖由 `d` 算出的压缩系数，形成循环依赖。

```c
t      = clamp(d / R, 0, 1);
smooth = t * t * (3 - 2*t);                    /* smoothstep，避免 exp/pow */
diam   = D_MAX + (D_MIN - D_MAX) * smooth;
opa    = OPA_MAX + (OPA_MIN - OPA_MAX) * smooth;
```

选 smoothstep 而非高斯是为了 MCU 友好：只有乘加，无超越函数，且在 `t=0` 与 `t=1` 处一阶导为零，边缘不会出现尺寸突变。

叠加**径向位移压缩**，这是"放大镜"与单纯"变小"的区别所在——越远的气泡朝中心方向轻微收拢：

```c
shrink = 1 - RADIAL_COMPRESS * smooth;         /* RADIAL_COMPRESS ≈ 0.12 */
screen_x = cx + (world_x - fx) * shrink;
screen_y = cy + (world_y - fy) * shrink;
```

## 4. 物理与交互

由单个 `lv_timer`（周期 16ms）驱动的三态状态机，作用于焦点世界坐标 `(fx, fy)` 与速度 `(vx, vy)`。

### DRAG
指针按下后，`(fx, fy)` 跟随指针增量反向移动。速度用指数滑动平均记录，`α = 0.35`——直接用最后一帧的增量会把松手瞬间的抖动放大成一次乱飞。

### GLIDE
```c
fx += vx * dt;  fy += vy * dt;
decay = expf(-dt_ms / TAU);                    /* TAU = 260ms */
vx *= decay;    vy *= decay;
```
用时间常数而非每帧固定系数，保证掉帧时手感不变。MCU 上可用查表或一阶近似替代 `expf`。

当 `hypot(vx, vy) < V_SNAP`（12 px/s）时转入 SNAP。

### SNAP
临界阻尼弹簧拉向最近格心，保证不来回振荡：

```c
ω = 12 rad/s;  k = ω² = 144;  c = 2ω = 24;
a  = -k * (f - target) - c * v;
v += a * dt;
f += v * dt;
```
约 400ms 内静止。目标格心由 3.1 的像素→格子反解得到。

### 高亮
每帧算出离屏幕中心最近的格子并置为高亮色 `#FF9500`。焦点跨格时该气泡做一次 `1.0 → 1.08 → 1.0`、180ms 的缩放脉冲作为触觉替代反馈。

### 滚轮缩放
改变 `S`（范围 60~120），经 `lv_anim` 平滑过渡而非硬跳。

### 点击判定
在 `LV_EVENT_RELEASED` 时，若本次按下期间累计位移 < 8px 且时长 < 400ms，判为点击；否则视为拖拽，不触发。命中气泡弹一下并触发用户回调。

## 5. 渲染与性能策略

**核心风险**：LVGL 的 `transform_scale` 会使对象走 layer 渲染路径（先渲染到临时 buffer 再缩放）。对约 100 个对象每帧执行，在 800×480 软件渲染下几乎必然掉帧。

**对策——完全不使用 `transform_scale`**：

- **圆底缩放**：直接改 `width`/`height`，配合 `radius = LV_RADIUS_CIRCLE`。缩放一个圆本质就是改直径，只需重绘一个圆角矩形，代价比 layer transform 低一个数量级。
- **图标缩放**：按当前直径选最接近的 Montserrat 字号档。`lv_conf.h` 中 12~48 步进 2 共 19 档全部开启，取 `font_size ≈ diam × 0.42` 后就近取档；800×480 下 2px 的字号跳变肉眼难辨，等效无级缩放。
- **脏区控制**：每帧只对位置或尺寸**确实发生变化**的对象调用 setter，避免无谓 `invalidate`。

### 对象池

不为无限网格创建对象。池大小按最小 `S` 时的最坏情况静态分配：

```
cols = ceil(W / (S_MIN·√3)) + 3
rows = ceil(H / (S_MIN·1.5)) + 3
```
`S_MIN = 60`、800×480 时约 11×9 = 99，取 `POOL_SIZE = 112`。

每帧遍历焦点附近的格子范围，把池中对象依次分配给可见坑位（类似 RecyclerView 的回收复用），多余的 `lv_obj_add_flag(LV_OBJ_FLAG_HIDDEN)`。池对象在初始化时一次性创建，运行期不再增删。

### 验收标准
满屏连续拖拽时稳定 60fps。若 width/height 方案视觉上不够顺滑，退化方案是仅对最靠近中心的少数几个气泡额外施加 `transform_scale`。

## 6. 视觉规格（800×480）

| 参数 | 值 |
|---|---|
| 屏幕 / 中心 | 800×480 / (400, 240) |
| `S` 默认 / 范围 | 80 / 60~120 |
| 列间距 / 行间距（S=80） | 138.6 / 120 |
| 气泡直径 `D_MAX` → `D_MIN` | 120 → 44 |
| 放大镜影响半径 `R` | 420 |
| 不透明度 `OPA_MAX` → `OPA_MIN` | 255 → 90 |
| 径向压缩系数 | 0.12 |
| 高亮色 | `#FF9500` |
| 同屏可见坑位（S=80） | 约 23 |

按面积算：屏幕 384000 px²，单格六边形面积 `(3√3/2)·S² = 16627 px²`，同屏约 23 格。37 个菜单项 > 23 个可见坑位，因此正常视野内不会出现重复图标——这是选 37 而非 19 的主要动机（19 项时同屏会看到同一图标出现两次）。

图标使用 LVGL 内置 `LV_SYMBOL_*` 字体符号，每项配一个色相均匀分布的圆底色，零外部资源依赖。

## 7. API

```c
typedef struct {
    const char * icon;      /* LV_SYMBOL_* 或任意 UTF-8 文本 */
    const char * label;
    lv_color_t   color;     /* 圆底色 */
} lv_hex_menu_item_t;

lv_obj_t * lv_hex_menu_create(lv_obj_t * parent);
void       lv_hex_menu_set_items(lv_obj_t * obj, const lv_hex_menu_item_t * items, uint32_t cnt);
void       lv_hex_menu_set_event_cb(lv_obj_t * obj, lv_event_cb_t cb, void * user_data);
int32_t    lv_hex_menu_get_focused(lv_obj_t * obj);   /* 当前中心项，0..cnt-1 */
```

平铺周期固定为 37 个槽位；实际项数 `cnt` 通过 `items[slot % cnt]` 映射。`cnt == 37` 时铺砌完美无重复，是设计目标场景；`cnt < 37` 仍可正常工作，只是一个周期内会出现重复项。

## 8. 测试策略

`test/test_hex_math.c` 用裸 gcc 编译（不链接 LVGL），覆盖：

- 3.2 节列出的五条平铺性质
- 像素↔格子往返：对格心坐标反解必须得回原格子；对随机点反解得到的格子必须确实是最近格
- smoothstep 曲线单调、端点值正确、`t∈[0,1]` 内不越界
- 弹簧：从任意初始偏移与速度出发必定收敛，且过冲不超过初始偏移的 2%（临界阻尼特征）
- 摩擦：速度严格单调递减，且有限步内低于阈值

交互手感无法自动化验证，靠模拟器实跑观察。

## 9. 明确不做（YAGNI）

- 点击后"炸开 + 全页面板"展开动画——第二阶段需求，本期只做选中反馈与回调
- 图片（PNG/C 数组）图标资源
- 键盘 / 编码器导航
- 多级菜单嵌套
- 主题切换、深浅色适配
