#include "lv_hex_menu.h"
#include "hex_grid.h"
#include "hex_physics.h"
#include <math.h>
#include <stdint.h>

/* ------------------ 可调参数（对应 spec 第 6 节） ------------------ */
#define HEX_S_DEF            80.0f    /* 网格尺寸默认值 */
#define HEX_S_MIN            60.0f
#define HEX_S_MAX           120.0f
#define HEX_D_MAX           120.0f    /* 中心气泡直径 */
#define HEX_D_MIN            44.0f    /* 最远气泡直径 */
#define HEX_R_INFLUENCE     420.0f    /* 放大镜影响半径 */
#define HEX_OPA_MAX           255
#define HEX_OPA_MIN            90
#define HEX_RADIAL_COMPRESS   0.12f   /* 径向位移压缩系数 */
#define HEX_HL_COLOR          0xFF9500

#define HEX_POOL_SIZE         128     /* S_MIN 时最坏约需 108，留余量 */
#define HEX_PERIOD_MS          16
#define HEX_SQRT3             1.7320508075688772f

#define HEX_TAU_MS          260.0f   /* 惯性时间常数 */
#define HEX_V_SNAP           12.0f   /* 低于此速度转入吸附，px/s */
#define HEX_OMEGA            12.0f   /* 弹簧角频率 rad/s */
#define HEX_VEL_EMA           0.35f  /* 拖拽测速的指数滑动平均系数 */

#define HEX_CLICK_TRAVEL       8     /* 累计位移不超过此值才算点击，px */
#define HEX_CLICK_MS         400     /* 按下时长不超过此值才算点击 */
#define HEX_PULSE_MS         180     /* 点击/跨格的缩放脉冲时长 */
#define HEX_PULSE_GAIN         0.08f /* 脉冲峰值放大比例 */

/* Montserrat 12..48 步进 2，共 19 档。lv_conf.h 中已全部开启。 */
static const lv_font_t * const HEX_FONTS[] = {
    &lv_font_montserrat_12, &lv_font_montserrat_14, &lv_font_montserrat_16,
    &lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_22,
    &lv_font_montserrat_24, &lv_font_montserrat_26, &lv_font_montserrat_28,
    &lv_font_montserrat_30, &lv_font_montserrat_32, &lv_font_montserrat_34,
    &lv_font_montserrat_36, &lv_font_montserrat_38, &lv_font_montserrat_40,
    &lv_font_montserrat_42, &lv_font_montserrat_44, &lv_font_montserrat_46,
    &lv_font_montserrat_48,
};
#define HEX_FONT_CNT ((int32_t)(sizeof(HEX_FONTS) / sizeof(HEX_FONTS[0])))

/* ------------------ 内部类型 ------------------ */

typedef struct {
    lv_obj_t * bubble;
    lv_obj_t * icon;
    hex_axial_t axial;      /* 当前世界格身份；池对象和 slot 都可能复用 */
    int32_t    slot;        /* 当前占用槽位，-1 表示本帧未使用 */
    int32_t    last_d;      /* 上次直径，用于跳过无变化的 setter */
    int32_t    last_x;
    int32_t    last_y;
    int32_t    last_font;   /* 上次字体档位索引 */
    lv_opa_t   last_opa;
    int8_t     last_hl;
} hex_cell_t;

typedef enum {
    HEX_ST_IDLE,
    HEX_ST_DRAG,
    HEX_ST_GLIDE,
    HEX_ST_SNAP,
} hex_state_t;

typedef struct {
    const lv_hex_menu_item_t * items;
    uint32_t                   item_cnt;

    hex_cell_t pool[HEX_POOL_SIZE];

    float fx, fy;      /* 焦点世界坐标 */
    float vx, vy;      /* 速度 px/s */
    float s;           /* 当前网格尺寸 */

    hex_state_t state;
    lv_point_t  last_pt;      /* 上一帧指针位置 */
    lv_point_t  press_pt;     /* 按下时的指针位置 */
    uint32_t    press_tick;
    uint32_t    press_last_tick;   /* 仅供 PRESSING 测速使用，与定时器的 last_tick 互不干扰 */
    int32_t     press_travel; /* 按下期间累计位移，用于区分点击与拖拽 */
    float       target_x;     /* 吸附目标 */
    float       target_y;

    int32_t     focus_slot;
    hex_axial_t focus_axial;
    int32_t     pulse_slot;
    hex_axial_t pulse_axial;
    uint32_t    pulse_tick;

    lv_event_dsc_t * user_event;

    lv_timer_t * timer;
    uint32_t     last_tick;
} hex_menu_ctx_t;

static hex_menu_ctx_t * ctx_of(lv_obj_t * obj)
{
    return (hex_menu_ctx_t *)lv_obj_get_user_data(obj);
}

/* ------------------ 工具 ------------------ */

static int32_t font_idx_for_diam(int32_t diam)
{
    int32_t idx = ((diam * 42) / 100 - 12) / 2;   /* 字号 ≈ 直径 × 0.42 */
    if(idx < 0) idx = 0;
    if(idx >= HEX_FONT_CNT) idx = HEX_FONT_CNT - 1;
    return idx;
}

static const lv_hex_menu_item_t * item_of_slot(hex_menu_ctx_t * m, int32_t slot)
{
    if(m->items == NULL || m->item_cnt == 0) return NULL;
    return &m->items[(uint32_t)slot % m->item_cnt];
}

/* ------------------ 布局 ------------------ */

static void hex_layout(lv_obj_t * obj)
{
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL) return;

    const int32_t w = lv_obj_get_width(obj);
    const int32_t h = lv_obj_get_height(obj);
    const float cx = (float)w * 0.5f;
    const float cy = (float)h * 0.5f;

    if(m->pulse_slot >= 0 && lv_tick_elaps(m->pulse_tick) >= HEX_PULSE_MS) {
        m->pulse_slot = -1;
    }

    /* 径向压缩会把远处的气泡拉进视野，所以搜索范围要按压缩前放大 */
    const float margin = 1.0f / (1.0f - HEX_RADIAL_COMPRESS);
    const float half_w = cx * margin + m->s;
    const float half_h = cy * margin + m->s;

    int used = 0;
    float best_d2 = 1e30f;
    int32_t best_slot = -1;
    hex_axial_t best_axial = { 0, 0 };

    const int r0 = (int)floorf((m->fy - half_h) / (m->s * 1.5f));
    const int r1 = (int)ceilf((m->fy + half_h) / (m->s * 1.5f));

    for(int r = r0; r <= r1 && used < HEX_POOL_SIZE; r++) {
        const int q0 = (int)floorf((m->fx - half_w) / (m->s * HEX_SQRT3) - (float)r * 0.5f);
        const int q1 = (int)ceilf((m->fx + half_w) / (m->s * HEX_SQRT3) - (float)r * 0.5f);

        for(int q = q0; q <= q1 && used < HEX_POOL_SIZE; q++) {
            hex_axial_t a = { q, r };
            float wx, wy;
            hex_axial_to_px(a, m->s, &wx, &wy);

            /* 相对焦点的偏移。距离必须用未压缩值，否则屏幕坐标与压缩系数互相
             * 依赖会形成循环。 */
            const float ox = wx - m->fx;
            const float oy = wy - m->fy;
            const float d = sqrtf(ox * ox + oy * oy);

            const float t = hex_magnifier_t(d, HEX_R_INFLUENCE);
            const float diam_f = HEX_D_MAX + (HEX_D_MIN - HEX_D_MAX) * t;
            const float shrink = 1.0f - HEX_RADIAL_COMPRESS * t;
            const int32_t slot = hex_slot_index(q, r);

            const int32_t sx = (int32_t)lroundf(cx + ox * shrink);
            const int32_t sy = (int32_t)lroundf(cy + oy * shrink);
            int32_t diam = (int32_t)lroundf(diam_f);
            if(m->pulse_slot == slot &&
               m->pulse_axial.q == q && m->pulse_axial.r == r) {
                const uint32_t el = lv_tick_elaps(m->pulse_tick);
                const float ph = (float)el / (float)HEX_PULSE_MS;
                const float amp = (ph < 0.5f) ? (ph * 2.0f) : ((1.0f - ph) * 2.0f);
                diam = (int32_t)lroundf(diam_f * (1.0f + HEX_PULSE_GAIN * amp));
            }

            /* 完全在屏幕外的不占用池对象 */
            const int32_t half = diam / 2 + 1;
            if(sx + half < 0 || sx - half > w || sy + half < 0 || sy - half > h) continue;

            const float d2 = ox * ox + oy * oy;
            if(d2 < best_d2) {
                best_d2 = d2;
                best_slot = slot;
                best_axial = a;
            }

            hex_cell_t * c = &m->pool[used++];
            const lv_hex_menu_item_t * it = item_of_slot(m, slot);
            const lv_opa_t opa = (lv_opa_t)(HEX_OPA_MAX + (HEX_OPA_MIN - HEX_OPA_MAX) * t);

            const bool hl = m->focus_slot >= 0 &&
                            m->focus_axial.q == q && m->focus_axial.r == r;
            if(c->slot != slot) {
                c->slot = slot;
                lv_label_set_text(c->icon, it != NULL ? it->icon : "");
                c->last_hl = -1;
            }
            c->axial = a;

            if(c->last_hl != (int8_t)hl) {
                c->last_hl = (int8_t)hl;
                lv_obj_set_style_bg_color(
                    c->bubble,
                    hl ? lv_color_hex(HEX_HL_COLOR) : (it != NULL ? it->color : lv_color_black()),
                    LV_PART_MAIN);
            }

            if(c->last_d != diam) {
                c->last_d = diam;
                lv_obj_set_size(c->bubble, diam, diam);

                const int32_t fi = font_idx_for_diam(diam);
                if(c->last_font != fi) {
                    c->last_font = fi;
                    lv_obj_set_style_text_font(c->icon, HEX_FONTS[fi], LV_PART_MAIN);
                }
            }

            const int32_t px = sx - diam / 2;
            const int32_t py = sy - diam / 2;
            if(c->last_x != px || c->last_y != py) {
                c->last_x = px;
                c->last_y = py;
                lv_obj_set_pos(c->bubble, px, py);
            }

            if(c->last_opa != opa) {
                c->last_opa = opa;
                lv_obj_set_style_bg_opa(c->bubble, opa, LV_PART_MAIN);
                lv_obj_set_style_text_opa(c->icon, opa, LV_PART_MAIN);
            }
            lv_obj_set_hidden(c->bubble, false);
        }
    }

    if(best_slot >= 0 && m->focus_slot >= 0 &&
       (best_axial.q != m->focus_axial.q || best_axial.r != m->focus_axial.r)) {
        m->pulse_slot = best_slot;
        m->pulse_axial = best_axial;
        m->pulse_tick = lv_tick_get();
    }

    m->focus_slot = best_slot;
    if(best_slot >= 0) m->focus_axial = best_axial;

    for(int i = used; i < HEX_POOL_SIZE; i++) {
        m->pool[i].slot = -1;
        lv_obj_set_hidden(m->pool[i].bubble, true);
    }
}

/* ------------------ 物理状态机 ------------------ */

static void hex_pick_snap_target(hex_menu_ctx_t * m)
{
    hex_axial_t a = hex_px_to_axial(m->fx, m->fy, m->s);
    hex_axial_to_px(a, m->s, &m->target_x, &m->target_y);
}

static void hex_physics_update(hex_menu_ctx_t * m, float dt, float dt_ms)
{
    switch(m->state) {
        case HEX_ST_DRAG:
            /* 位置在事件回调里直接跟随指针，这里不动 */
            break;

        case HEX_ST_GLIDE: {
            m->fx += m->vx * dt;
            m->fy += m->vy * dt;

            const float decay = hex_friction_decay(dt_ms, HEX_TAU_MS);
            m->vx *= decay;
            m->vy *= decay;

            const float speed = sqrtf(m->vx * m->vx + m->vy * m->vy);
            if(speed < HEX_V_SNAP) {
                hex_pick_snap_target(m);
                m->state = HEX_ST_SNAP;
            }
            break;
        }

        case HEX_ST_SNAP: {
            hex_spring_step(&m->fx, &m->vx, m->target_x, HEX_OMEGA, dt);
            hex_spring_step(&m->fy, &m->vy, m->target_y, HEX_OMEGA, dt);

            const float dx = m->fx - m->target_x;
            const float dy = m->fy - m->target_y;
            if(dx * dx + dy * dy < 0.25f &&
               m->vx * m->vx + m->vy * m->vy < 1.0f) {
                m->fx = m->target_x;
                m->fy = m->target_y;
                m->vx = 0.0f;
                m->vy = 0.0f;
                m->state = HEX_ST_IDLE;
            }
            break;
        }

        case HEX_ST_IDLE:
        default:
            break;
    }
}

/* ------------------ 定时器 ------------------ */

static void hex_timer_cb(lv_timer_t * t)
{
    lv_obj_t * obj = (lv_obj_t *)lv_timer_get_user_data(t);
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL) return;

    const uint32_t now = lv_tick_get();
    float dt_ms = (float)(now - m->last_tick);
    m->last_tick = now;

    if(dt_ms <= 0.0f) return;
    if(dt_ms > 100.0f) dt_ms = 100.0f;   /* 掉帧保护，避免一帧飞出天际 */

    hex_physics_update(m, dt_ms * 0.001f, dt_ms);
    hex_layout(obj);
}

/* ------------------ 指针事件 ------------------ */

static void hex_drag_sample(hex_menu_ctx_t * m, lv_point_t p)
{
    const int32_t dx = p.x - m->last_pt.x;
    const int32_t dy = p.y - m->last_pt.y;

    m->press_travel += LV_ABS(dx) + LV_ABS(dy);

    /* 指针右移 => 焦点在世界坐标里左移 */
    m->fx -= (float)dx;
    m->fy -= (float)dy;

    /* 用指数滑动平均测速，零位移样本也必须参与，以支持按住刹停。 */
    const uint32_t now = lv_tick_get();
    float dt_ms = (float)(now - m->press_last_tick);
    if(dt_ms < 1.0f) dt_ms = 1.0f;
    m->press_last_tick = now;

    const float inst_vx = -(float)dx * 1000.0f / dt_ms;
    const float inst_vy = -(float)dy * 1000.0f / dt_ms;
    m->vx += (inst_vx - m->vx) * HEX_VEL_EMA;
    m->vy += (inst_vy - m->vy) * HEX_VEL_EMA;
    m->last_pt = p;
}

static void hex_handle_click(lv_obj_t * obj, hex_menu_ctx_t * m, lv_point_t p)
{
    lv_obj_update_layout(obj);

    hex_cell_t * hit = NULL;
    int64_t best_d2 = INT64_MAX;

    for(int i = 0; i < HEX_POOL_SIZE; i++) {
        hex_cell_t * c = &m->pool[i];
        if(c->slot < 0 || lv_obj_is_hidden(c->bubble)) continue;

        lv_area_t area;
        lv_obj_get_coords(c->bubble, &area);

        /* 以两倍坐标计算，保留奇数直径的半像素圆心。 */
        const int64_t dx = 2 * (int64_t)p.x - (int64_t)area.x1 - (int64_t)area.x2;
        const int64_t dy = 2 * (int64_t)p.y - (int64_t)area.y1 - (int64_t)area.y2;
        const int64_t diam = (int64_t)lv_area_get_width(&area);
        const int64_t d2 = dx * dx + dy * dy;

        if(d2 <= diam * diam && d2 < best_d2) {
            best_d2 = d2;
            hit = c;
        }
    }

    if(hit == NULL || m->items == NULL || m->item_cnt == 0) return;

    m->pulse_slot = hit->slot;
    m->pulse_axial = hit->axial;
    m->pulse_tick = lv_tick_get();

    const int32_t idx = (int32_t)((uint32_t)hit->slot % m->item_cnt);
    (void)lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, (void *)(lv_uintptr_t)idx);
}

static void hex_pointer_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target_obj(e);
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL) return;

    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t * indev = lv_event_get_indev(e);

    if(code == LV_EVENT_PRESS_LOST) {
        if(indev != NULL && lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) return;
        if(m->state == HEX_ST_DRAG) {
            m->vx = 0.0f;
            m->vy = 0.0f;
            hex_pick_snap_target(m);
            m->state = HEX_ST_SNAP;
        }
        return;
    }

    if(indev == NULL || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    switch(code) {
        case LV_EVENT_PRESSED: {
            const uint32_t t = lv_tick_get();
            m->state = HEX_ST_DRAG;
            m->vx = 0.0f;
            m->vy = 0.0f;
            m->last_pt = p;
            m->press_pt = p;
            m->press_tick = t;
            m->press_last_tick = t;
            m->press_travel = 0;
            break;
        }

        case LV_EVENT_PRESSING: {
            if(m->state != HEX_ST_DRAG) break;
            hex_drag_sample(m, p);
            break;
        }

        case LV_EVENT_RELEASED: {
            if(m->state != HEX_ST_DRAG) break;

            /* RELEASED 之前不保证再发一次 PRESSING，补上最后一段位移。 */
            if(p.x != m->last_pt.x || p.y != m->last_pt.y) hex_drag_sample(m, p);

            const bool click = m->press_travel <= HEX_CLICK_TRAVEL &&
                               lv_tick_elaps(m->press_tick) <= HEX_CLICK_MS;
            m->state = HEX_ST_GLIDE;
            if(click) {
                m->vx = 0.0f;
                m->vy = 0.0f;
                hex_handle_click(obj, m, p);
                return; /* 用户回调可能删除 obj 和 m */
            }
            break;
        }

        default:
            break;
    }
}

/* ------------------ 生命周期 ------------------ */

static void hex_delete_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target_obj(e);
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL) return;

    if(m->timer != NULL) lv_timer_delete(m->timer);
    lv_obj_set_user_data(obj, NULL);
    lv_free(m);
}

lv_obj_t * lv_hex_menu_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_clickable(obj, true);

    hex_menu_ctx_t * m = (hex_menu_ctx_t *)lv_malloc_zeroed(sizeof(hex_menu_ctx_t));
    LV_ASSERT_MALLOC(m);
    if(m == NULL) return obj;

    m->s = HEX_S_DEF;
    m->focus_slot = -1;
    m->pulse_slot = -1;
    lv_obj_set_user_data(obj, m);

    for(int i = 0; i < HEX_POOL_SIZE; i++) {
        hex_cell_t * c = &m->pool[i];

        c->bubble = lv_obj_create(obj);
        lv_obj_set_clickable(c->bubble, false);   /* 事件交给根对象统一处理 */
        lv_obj_set_scrollable(c->bubble, false);
        lv_obj_set_hidden(c->bubble, true);
        lv_obj_set_style_radius(c->bubble, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(c->bubble, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(c->bubble, 0, LV_PART_MAIN);
        lv_obj_set_size(c->bubble, (int32_t)HEX_D_MIN, (int32_t)HEX_D_MIN);

        c->icon = lv_label_create(c->bubble);
        lv_obj_center(c->icon);
        lv_obj_set_style_text_color(c->icon, lv_color_white(), LV_PART_MAIN);
        lv_label_set_text(c->icon, "");

        c->slot = -1;
        c->last_d = -1;
        c->last_x = INT32_MIN;
        c->last_y = INT32_MIN;
        c->last_font = -1;
        c->last_opa = 0;
        c->last_hl = -1;
    }

    lv_obj_add_event_cb(obj, hex_pointer_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, hex_pointer_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, hex_pointer_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(obj, hex_pointer_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(obj, hex_delete_cb, LV_EVENT_DELETE, NULL);

    m->last_tick = lv_tick_get();
    m->timer = lv_timer_create(hex_timer_cb, HEX_PERIOD_MS, obj);

    return obj;
}

void lv_hex_menu_set_items(lv_obj_t * obj, const lv_hex_menu_item_t * items, uint32_t cnt)
{
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL) return;

    m->items = items;
    m->item_cnt = cnt;
    m->pulse_slot = -1;

    for(int i = 0; i < HEX_POOL_SIZE; i++) m->pool[i].slot = -1;   /* 强制刷新内容 */
    hex_layout(obj);
}

int32_t lv_hex_menu_get_focused(lv_obj_t * obj)
{
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL || m->items == NULL || m->item_cnt == 0 || m->focus_slot < 0) return -1;
    return (int32_t)((uint32_t)m->focus_slot % m->item_cnt);
}

void lv_hex_menu_set_event_cb(lv_obj_t * obj, lv_event_cb_t cb, void * user_data)
{
    hex_menu_ctx_t * m = ctx_of(obj);
    if(m == NULL) return;

    if(m->user_event != NULL) {
        (void)lv_obj_remove_event_dsc(obj, m->user_event);
        m->user_event = NULL;
    }

    if(cb != NULL) {
        m->user_event = lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
}
