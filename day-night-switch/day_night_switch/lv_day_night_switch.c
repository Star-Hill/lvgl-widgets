#include "lv_day_night_switch.h"

/* ---------------- 尺寸：原始 CSS 以 60x34 设计，整体放大 DNS_SCALE 倍 ---------------- */
#define DNS_SCALE            4
#define DNS_S(v)             ((int32_t)((v) * DNS_SCALE))

#define DNS_W                60      /* 容器宽（CSS px） */
#define DNS_H                34      /* 容器高 */
#define DNS_SUN_D            26      /* 日/月直径 */
#define DNS_SUN_X             4      /* 日/月初始左上角 */
#define DNS_SUN_Y             4
#define DNS_SUN_TRAVEL       26      /* 切换时右移距离 */
#define DNS_STARS_RISE       32      /* 星空自上方滑入距离 */

/* ---------------- 颜色（对应 CSS） ---------------- */
#define DNS_C_DAY_BG         0x2196F3
#define DNS_C_NIGHT_BG       0x000000
#define DNS_C_SUN            0xFFFF00
#define DNS_C_MOON           0xFFFFFF
#define DNS_C_MOON_DOT       0x808080
#define DNS_C_CLOUD_LIGHT    0xEEEEEE
#define DNS_C_CLOUD_DARK     0xCCCCCC
#define DNS_C_STAR           0xFFFFFF

/* ---------------- 时长（对应 CSS） ---------------- */
#define DNS_TRANS_MS         400     /* transition: 0.4s */
#define DNS_ROTATE_MS        600     /* rotate-center 0.6s */
#define DNS_CLOUD_MS        6000     /* cloud-move 6s */
#define DNS_TWINKLE_MS      2000     /* star-twinkle 2s */
#define DNS_RAY_OPA           25     /* opacity 10% 约合 25/255 */
#define DNS_FULL_TURN       3600     /* LVGL 旋转单位为 0.1 度，一圈 = 3600 */

#define DNS_DOT_CNT            3
#define DNS_RAY_CNT            3
#define DNS_CLOUD_CNT          6
#define DNS_STAR_CNT           4

/* 云既要跟随日月平移(translate_x)，又要自身微动。微动改的是基准坐标(set_x)，
 * 与 translate_x 叠加而互不干扰，故需记住各自基准 x。 */
typedef struct {
    lv_obj_t * obj;
    int32_t    base_x;
} dns_cloud_t;

typedef struct {
    lv_obj_t *  root;
    lv_obj_t *  sun;
    lv_obj_t *  dots[DNS_DOT_CNT];
    lv_obj_t *  rays[DNS_RAY_CNT];
    dns_cloud_t clouds[DNS_CLOUD_CNT];
    lv_obj_t *  stars[DNS_STAR_CNT];
    void *      star_buf[DNS_STAR_CNT];   /* 星星 canvas 的像素缓冲，需自行释放 */
    bool        night;
} dns_ctx_t;

/* CSS 中各元素的坐标与尺寸（原始 px），与源样式逐条对应 */
static const int32_t DOT_XYD[DNS_DOT_CNT][3]   = { {10, 3, 6}, {2, 10, 10}, {16, 18, 3} };
/* 光晕相对日月左上角的偏移与直径；ray2 的 -50% 即 -13 */
static const int32_t RAY_OFF_D[DNS_RAY_CNT][3] = { {-8, -8, 43}, {-13, -13, 55}, {-18, -18, 60} };
/* 云相对日月左上角的偏移与直径；前 3 深色、后 3 浅色 */
static const int32_t CLOUD_OFF_D[DNS_CLOUD_CNT][3] = {
    {30, 15, 40}, {44, 10, 20}, {18, 24, 30},
    {36, 18, 40}, {48, 14, 20}, {22, 26, 30},
};
/* CSS 中深色云 animation-delay: 1s */
static const uint32_t CLOUD_DELAY[DNS_CLOUD_CNT] = { 1000, 1000, 1000, 0, 0, 0 };
/* 星的位置与尺寸，以及 CSS 的 animation-delay */
static const int32_t  STAR_XYD[DNS_STAR_CNT][3] = { {3, 2, 20}, {3, 16, 6}, {10, 20, 12}, {18, 0, 18} };
static const uint32_t STAR_DELAY[DNS_STAR_CNT]  = { 300, 0, 600, 1300 };

static dns_ctx_t * ctx_of(lv_obj_t * obj)
{
    return (dns_ctx_t *)lv_obj_get_user_data(obj);
}

/* ---------------- 动画执行器 ---------------- */

/* 主切换进度 t 属于 [0,1000]：底色、日月位移与变色、月坑淡入、星空滑入 */
static void dns_anim_exec(void * var, int32_t t)
{
    dns_ctx_t * c = ctx_of((lv_obj_t *)var);
    if(c == NULL) return;

    const uint8_t mix = (uint8_t)(t * 255 / 1000);

    lv_obj_set_style_bg_color(c->root,
        lv_color_mix(lv_color_hex(DNS_C_NIGHT_BG), lv_color_hex(DNS_C_DAY_BG), mix), LV_PART_MAIN);

    const int32_t dx = DNS_S(DNS_SUN_TRAVEL) * t / 1000;
    lv_obj_set_style_translate_x(c->sun, dx, LV_PART_MAIN);
    lv_obj_set_style_bg_color(c->sun,
        lv_color_mix(lv_color_hex(DNS_C_MOON), lv_color_hex(DNS_C_SUN), mix), LV_PART_MAIN);

    /* 光晕与云跟随日月平移；云因此被根容器裁掉，形成飘走的效果 */
    for(int i = 0; i < DNS_RAY_CNT; i++)   lv_obj_set_style_translate_x(c->rays[i], dx, LV_PART_MAIN);
    for(int i = 0; i < DNS_CLOUD_CNT; i++) lv_obj_set_style_translate_x(c->clouds[i].obj, dx, LV_PART_MAIN);

    for(int i = 0; i < DNS_DOT_CNT; i++)   lv_obj_set_style_opa(c->dots[i], mix, LV_PART_MAIN);

    const int32_t star_dy = -DNS_S(DNS_STARS_RISE) + DNS_S(DNS_STARS_RISE) * t / 1000;
    for(int i = 0; i < DNS_STAR_CNT; i++) {
        lv_obj_set_style_translate_y(c->stars[i], star_dy, LV_PART_MAIN);
        lv_obj_set_style_opa(c->stars[i], mix, LV_PART_MAIN);
    }
}

/* 切换瞬间日月自转一圈（对应 CSS 的 rotate-center） */
static void dns_rotate_exec(void * var, int32_t v)
{
    lv_obj_set_style_transform_rotation((lv_obj_t *)var, v, LV_PART_MAIN);
}

/* 云自身微动：0 到 +4 到 -4 再回 0（CSS cloud-move）。改基准 x，与跟随位移叠加 */
static void dns_cloud_drift_exec(void * var, int32_t v)
{
    dns_cloud_t * cl = (dns_cloud_t *)var;
    const int32_t amp = DNS_S(4);
    int32_t off;
    if(v < 400)      off = (amp * v) / 400;
    else if(v < 800) off = amp - (2 * amp * (v - 400)) / 400;
    else             off = -amp + (amp * (v - 800)) / 200;
    lv_obj_set_x(cl->obj, cl->base_x + off);
}

/* 星星闪烁：缩放 1 到 1.2 到 0.8 再回 1（CSS star-twinkle，256 表示 1.0） */
static void dns_twinkle_exec(void * var, int32_t v)
{
    int32_t scale;
    if(v < 400)      scale = 256 + (51 * v) / 400;
    else if(v < 800) scale = 307 - (102 * (v - 400)) / 400;
    else             scale = 205 + (51 * (v - 800)) / 200;
    lv_obj_set_style_transform_scale_x((lv_obj_t *)var, scale, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y((lv_obj_t *)var, scale, LV_PART_MAIN);
}

/* ---------------- 事件 ---------------- */

static void dns_click_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target_obj(e);
    dns_ctx_t * c = ctx_of(obj);
    if(c == NULL) return;
    lv_day_night_switch_set_night(obj, !c->night, true);
    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}

static void dns_delete_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target_obj(e);
    dns_ctx_t * c = ctx_of(obj);
    if(c == NULL) return;

    /* 动画持有指向 ctx 内部的 var，必须在释放前全部删除 */
    lv_anim_delete(obj, dns_anim_exec);
    lv_anim_delete(c->sun, dns_rotate_exec);
    for(int i = 0; i < DNS_CLOUD_CNT; i++) lv_anim_delete(&c->clouds[i], dns_cloud_drift_exec);
    for(int i = 0; i < DNS_STAR_CNT; i++)  lv_anim_delete(c->stars[i], dns_twinkle_exec);

    /* 星星 canvas 的像素缓冲由本组件分配，LVGL 不会代为回收 */
    for(int i = 0; i < DNS_STAR_CNT; i++) {
        if(c->star_buf[i] != NULL) lv_free(c->star_buf[i]);
    }

    lv_obj_set_user_data(obj, NULL);
    lv_free(c);
}

/* ---------------- 构建辅助 ---------------- */

/* 本组件里日月、月坑、光晕、云全都是纯色圆 */
static lv_obj_t * dns_circle(lv_obj_t * parent, int32_t x, int32_t y, int32_t d,
                             uint32_t color, lv_opa_t opa)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, DNS_S(d), DNS_S(d));
    lv_obj_set_pos(o, DNS_S(x), DNS_S(y));
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(o, opa, LV_PART_MAIN);
    lv_obj_set_clickable(o, false);
    lv_obj_set_scrollable(o, false);
    /* 子元素需能超出各自父边界，统一由根容器裁剪 */
    lv_obj_set_overflow_visible(o, true);
    return o;
}

/* 四角星：用 canvas + 矢量绘制还原 CSS 里那条 SVG path。
 *
 * 原始 path（20x20 viewBox）：
 *   M 0 10  C 10 10, 10 10, 0 10   C 10 10, 10 10, 10 20
 *           C 10 10, 10 10, 20 10  C 10 10, 10 10, 10 0
 *           C 10 10, 10 10, 0 10   Z
 * 即依次连接左中→下中→右中→上中→左中四个尖点，而每段三次贝塞尔的两个控制点
 * 都落在中心 (10,10)。控制点把曲线拽向圆心，于是四条边向内凹陷、尖角收细——
 * 这正是它不同于「加号」的地方。
 *
 * 画好的像素留在 canvas 自带的缓冲里，之后不再重绘；缩放与闪烁仍由
 * transform_scale 施加在 canvas 对象上。 */
static lv_obj_t * dns_star(lv_obj_t * parent, int32_t x, int32_t y, int32_t d, void ** out_buf)
{
    const int32_t full = DNS_S(d);

    lv_obj_t * s = lv_canvas_create(parent);
    lv_obj_remove_style_all(s);
    lv_obj_set_pos(s, DNS_S(x), DNS_S(y));
    lv_obj_set_clickable(s, false);
    lv_obj_set_scrollable(s, false);

    /* ARGB8888 便于让星形之外保持透明 */
    const uint32_t stride = (uint32_t)full * 4u;
    void * buf = lv_malloc_zeroed(stride * (uint32_t)full + LV_DRAW_BUF_ALIGN);
    LV_ASSERT_MALLOC(buf);
    if(buf == NULL) return s;
    *out_buf = buf;   /* 交回给 ctx，删除时释放 */
    lv_canvas_set_buffer(s, buf, full, full, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(s, lv_color_black(), LV_OPA_TRANSP);

    lv_layer_t layer;
    lv_canvas_init_layer(s, &layer);

    lv_vector_path_t * path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
    const float half = (float)full * 0.5f;   /* 中心，即 viewBox 的 (10,10) */
    const lv_fpoint_t c  = { half, half };                 /* 两个控制点重合于中心 */
    const lv_fpoint_t left  = { 0.0f,        half };
    const lv_fpoint_t down  = { half,        (float)full };
    const lv_fpoint_t right = { (float)full, half };
    const lv_fpoint_t up    = { half,        0.0f };

    lv_vector_path_move_to(path, &left);
    lv_vector_path_cubic_to(path, &c, &c, &down);
    lv_vector_path_cubic_to(path, &c, &c, &right);
    lv_vector_path_cubic_to(path, &c, &c, &up);
    lv_vector_path_cubic_to(path, &c, &c, &left);
    lv_vector_path_close(path);

    lv_draw_vector_dsc_t * dsc = lv_draw_vector_dsc_create(&layer);
    lv_draw_vector_dsc_set_fill_color(dsc, lv_color_hex(DNS_C_STAR));
    lv_draw_vector_dsc_add_path(dsc, path);
    lv_draw_vector(dsc);

    lv_draw_vector_dsc_delete(dsc);
    lv_vector_path_delete(path);
    lv_canvas_finish_layer(s, &layer);

    return s;
}

/* 启动一个无限循环动画 */
static void dns_loop_anim(void * var, lv_anim_exec_xcb_t exec, uint32_t dur, uint32_t delay)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, var);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, dur);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/* ---------------- 公共 API ---------------- */

lv_obj_t * lv_day_night_switch_create(lv_obj_t * parent)
{
    lv_obj_t * root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, DNS_S(DNS_W), DNS_S(DNS_H));
    lv_obj_set_style_radius(root, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(root, lv_color_hex(DNS_C_DAY_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    /* 对应 CSS 的 overflow: hidden，云与光晕越界后在此被裁掉 */
    lv_obj_set_style_clip_corner(root, true, LV_PART_MAIN);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_clickable(root, true);

    dns_ctx_t * c = lv_malloc_zeroed(sizeof(dns_ctx_t));
    LV_ASSERT_MALLOC(c);
    if(c == NULL) return root;
    c->root = root;
    c->night = false;
    lv_obj_set_user_data(root, c);

    /* 光晕先建，故位于日月之下（对应 CSS 的 z-index: -1） */
    for(int i = 0; i < DNS_RAY_CNT; i++) {
        c->rays[i] = dns_circle(root,
                                DNS_SUN_X + RAY_OFF_D[i][0],
                                DNS_SUN_Y + RAY_OFF_D[i][1],
                                RAY_OFF_D[i][2], DNS_C_MOON, DNS_RAY_OPA);
    }

    /* 日月本体，月坑作为其子对象，随之一起旋转 */
    c->sun = dns_circle(root, DNS_SUN_X, DNS_SUN_Y, DNS_SUN_D, DNS_C_SUN, LV_OPA_COVER);
    lv_obj_set_style_transform_pivot_x(c->sun, DNS_S(DNS_SUN_D) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(c->sun, DNS_S(DNS_SUN_D) / 2, LV_PART_MAIN);
    for(int i = 0; i < DNS_DOT_CNT; i++) {
        c->dots[i] = dns_circle(c->sun, DOT_XYD[i][0], DOT_XYD[i][1], DOT_XYD[i][2],
                                DNS_C_MOON_DOT, LV_OPA_TRANSP);
    }

    /* 云在日月之上（CSS 中云是 sun-moon 的最后一批子元素） */
    for(int i = 0; i < DNS_CLOUD_CNT; i++) {
        const uint32_t col = (i < 3) ? DNS_C_CLOUD_DARK : DNS_C_CLOUD_LIGHT;
        const int32_t bx = DNS_SUN_X + CLOUD_OFF_D[i][0];
        const int32_t by = DNS_SUN_Y + CLOUD_OFF_D[i][1];
        c->clouds[i].obj = dns_circle(root, bx, by, CLOUD_OFF_D[i][2], col, LV_OPA_COVER);
        c->clouds[i].base_x = DNS_S(bx);
    }

    /* 星空最后建，位于最上层；初始在容器上方且透明 */
    for(int i = 0; i < DNS_STAR_CNT; i++) {
        c->stars[i] = dns_star(root, STAR_XYD[i][0], STAR_XYD[i][1], STAR_XYD[i][2], &c->star_buf[i]);
        lv_obj_set_style_opa(c->stars[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_translate_y(c->stars[i], -DNS_S(DNS_STARS_RISE), LV_PART_MAIN);
    }

    /* 持续循环的两组动画：云微动、星闪烁 */
    for(int i = 0; i < DNS_CLOUD_CNT; i++) {
        dns_loop_anim(&c->clouds[i], dns_cloud_drift_exec, DNS_CLOUD_MS, CLOUD_DELAY[i]);
    }
    for(int i = 0; i < DNS_STAR_CNT; i++) {
        dns_loop_anim(c->stars[i], dns_twinkle_exec, DNS_TWINKLE_MS, STAR_DELAY[i]);
    }

    lv_obj_add_event_cb(root, dns_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(root, dns_delete_cb, LV_EVENT_DELETE, NULL);

    return root;
}

bool lv_day_night_switch_is_night(lv_obj_t * obj)
{
    dns_ctx_t * c = ctx_of(obj);
    return (c != NULL) && c->night;
}

void lv_day_night_switch_set_night(lv_obj_t * obj, bool night, bool anim)
{
    dns_ctx_t * c = ctx_of(obj);
    if(c == NULL) return;

    const int32_t from = c->night ? 1000 : 0;
    const int32_t to   = night ? 1000 : 0;
    c->night = night;

    lv_anim_delete(obj, dns_anim_exec);
    if(!anim) {
        dns_anim_exec(obj, to);
        return;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, dns_anim_exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, DNS_TRANS_MS);
    lv_anim_start(&a);

    /* 日月自转一圈 */
    lv_anim_delete(c->sun, dns_rotate_exec);
    lv_anim_t r;
    lv_anim_init(&r);
    lv_anim_set_var(&r, c->sun);
    lv_anim_set_exec_cb(&r, dns_rotate_exec);
    lv_anim_set_values(&r, 0, DNS_FULL_TURN);
    lv_anim_set_duration(&r, DNS_ROTATE_MS);
    lv_anim_set_path_cb(&r, lv_anim_path_ease_in_out);
    lv_anim_start(&r);
}

void lv_day_night_switch_set_event_cb(lv_obj_t * obj, lv_event_cb_t cb, void * user_data)
{
    if(cb != NULL) lv_obj_add_event_cb(obj, cb, LV_EVENT_VALUE_CHANGED, user_data);
}
