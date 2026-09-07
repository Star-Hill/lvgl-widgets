#include "lv_hex_menu.h"
#include "hex_grid.h"
#include "hex_physics.h"
#include <math.h>

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
    int32_t    slot;        /* 当前占用槽位，-1 表示本帧未使用 */
    int32_t    last_d;      /* 上次直径，用于跳过无变化的 setter */
    int32_t    last_x;
    int32_t    last_y;
    int32_t    last_font;   /* 上次字体档位索引 */
    lv_opa_t   last_opa;
} hex_cell_t;

typedef struct {
    const lv_hex_menu_item_t * items;
    uint32_t                   item_cnt;

    hex_cell_t pool[HEX_POOL_SIZE];

    float fx, fy;      /* 焦点世界坐标 */
    float vx, vy;      /* 速度 px/s */
    float s;           /* 当前网格尺寸 */

    int32_t focus_slot;

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

    /* 径向压缩会把远处的气泡拉进视野，所以搜索范围要按压缩前放大 */
    const float margin = 1.0f / (1.0f - HEX_RADIAL_COMPRESS);
    const float half_w = cx * margin + m->s;
    const float half_h = cy * margin + m->s;

    int used = 0;
    float best_d2 = 1e30f;
    int32_t best_slot = -1;

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

            const int32_t sx = (int32_t)lroundf(cx + ox * shrink);
            const int32_t sy = (int32_t)lroundf(cy + oy * shrink);
            const int32_t diam = (int32_t)lroundf(diam_f);

            /* 完全在屏幕外的不占用池对象 */
            const int32_t half = diam / 2 + 1;
            if(sx + half < 0 || sx - half > w || sy + half < 0 || sy - half > h) continue;

            const int32_t slot = hex_slot_index(q, r);
            const float d2 = ox * ox + oy * oy;
            if(d2 < best_d2) {
                best_d2 = d2;
                best_slot = slot;
            }

            hex_cell_t * c = &m->pool[used++];
            const lv_hex_menu_item_t * it = item_of_slot(m, slot);
            const lv_opa_t opa = (lv_opa_t)(HEX_OPA_MAX + (HEX_OPA_MIN - HEX_OPA_MAX) * t);

            if(c->slot != slot) {
                c->slot = slot;
                if(it != NULL) {
                    lv_obj_set_style_bg_color(c->bubble, it->color, LV_PART_MAIN);
                    lv_label_set_text(c->icon, it->icon);
                }
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

    m->focus_slot = best_slot;

    for(int i = used; i < HEX_POOL_SIZE; i++) {
        m->pool[i].slot = -1;
        lv_obj_set_hidden(m->pool[i].bubble, true);
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

    /* Task 5 会在此处插入物理状态机 */
    hex_layout(obj);
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

    hex_menu_ctx_t * m = (hex_menu_ctx_t *)lv_malloc_zeroed(sizeof(hex_menu_ctx_t));
    LV_ASSERT_MALLOC(m);
    if(m == NULL) return obj;

    m->s = HEX_S_DEF;
    m->focus_slot = -1;
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
    }

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
    /* Task 6 实现 */
    LV_UNUSED(obj);
    LV_UNUSED(cb);
    LV_UNUSED(user_data);
}
