#include "ui.h"
#include "lvgl.h"
#include "lv_hex_menu.h"
#include <stdio.h>

#define UI_ITEM_CNT 37

/* 卡片展开动画参数 */
#define CARD_W        320   /* 展开后卡片宽 */
#define CARD_H        360   /* 展开后卡片高 */
#define CARD_RADIUS    22
#define CARD_ANIM_MS  360   /* 展开时长 */
#define GENIE_MS      460   /* 神灯收起时长 */
#define GENIE_SCALE_FULL 256 /* LVGL transform_scale 基准：256 = 1.0 */
#define GENIE_SCALE_MIN   28 /* 收起终点缩放 ≈0.11，缩到接近气泡大小 */

static lv_hex_menu_item_t s_items[UI_ITEM_CNT];

static const char * const s_icons[UI_ITEM_CNT] = {
    LV_SYMBOL_HOME,      LV_SYMBOL_SETTINGS,  LV_SYMBOL_BELL,      LV_SYMBOL_WIFI,
    LV_SYMBOL_BLUETOOTH, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_CHARGE, LV_SYMBOL_USB,
    LV_SYMBOL_SD_CARD,   LV_SYMBOL_SAVE,      LV_SYMBOL_DOWNLOAD,  LV_SYMBOL_UPLOAD,
    LV_SYMBOL_REFRESH,   LV_SYMBOL_COPY,      LV_SYMBOL_PASTE,     LV_SYMBOL_TRASH,
    LV_SYMBOL_EDIT,      LV_SYMBOL_CUT,       LV_SYMBOL_KEYBOARD,  LV_SYMBOL_GPS,
    LV_SYMBOL_CALL,      LV_SYMBOL_ENVELOPE,  LV_SYMBOL_IMAGE,     LV_SYMBOL_VIDEO,
    LV_SYMBOL_AUDIO,     LV_SYMBOL_PLAY,      LV_SYMBOL_PAUSE,     LV_SYMBOL_STOP,
    LV_SYMBOL_NEXT,      LV_SYMBOL_PREV,      LV_SYMBOL_VOLUME_MAX,LV_SYMBOL_LIST,
    LV_SYMBOL_FILE,      LV_SYMBOL_DIRECTORY, LV_SYMBOL_EYE_OPEN,  LV_SYMBOL_POWER,
    LV_SYMBOL_OK,
};

static const char * const s_labels[UI_ITEM_CNT] = {
    "Home",     "Settings", "Alerts",   "Wi-Fi",    "Bluetooth","Battery",  "Charge",
    "USB",      "SD Card",  "Save",     "Download", "Upload",   "Refresh",  "Copy",
    "Paste",    "Trash",    "Edit",     "Cut",      "Keyboard", "GPS",      "Call",
    "Mail",     "Photos",   "Video",    "Audio",    "Play",     "Pause",    "Stop",
    "Next",     "Prev",     "Volume",   "List",     "Files",    "Folders",  "Preview",
    "Power",    "Done",
};

/* ---- 展开卡片：文件级单例（本工程为单菜单单屏应用） ---- */
static lv_obj_t * s_menu     = NULL;   /* 蜂窝菜单对象 */
static lv_obj_t * s_veil     = NULL;   /* 半透明遮罩，兼作点击外部收起的捕获层 */
static lv_obj_t * s_card     = NULL;   /* 展开卡片本体 */
static lv_obj_t * s_card_ico = NULL;   /* 卡片顶部大图标 */
static lv_obj_t * s_card_title = NULL; /* 卡片标题 */
static lv_obj_t * s_card_body  = NULL; /* 卡片内容区（占位控件） */

/* 展开动画的起止几何：随每次点击更新 */
static lv_area_t   s_from;             /* 起点 = 被点气泡屏幕框 */
static lv_color_t  s_from_color;       /* 起点底色 */
static bool        s_open = false;     /* 卡片当前是否处于展开态 */

/* 目标（展开后）居中卡片几何 */
static void card_target_area(lv_area_t * out)
{
    lv_obj_t * scr = lv_screen_active();
    const int32_t sw = lv_obj_get_width(scr);
    const int32_t sh = lv_obj_get_height(scr);
    out->x1 = (sw - CARD_W) / 2;
    out->y1 = (sh - CARD_H) / 2;
    out->x2 = out->x1 + CARD_W - 1;
    out->y2 = out->y1 + CARD_H - 1;
}

/* 单进度动画：t ∈ [0,1000]，在起点气泡与居中卡片之间插值 */
static void card_anim_exec(void * var, int32_t t)
{
    lv_obj_t * card = (lv_obj_t *)var;

    lv_area_t to;
    card_target_area(&to);

    const int32_t fw = lv_area_get_width(&s_from), fh = lv_area_get_height(&s_from);
    const int32_t tw = lv_area_get_width(&to),     th = lv_area_get_height(&to);

    /* 线性插值（缓动已由 path_cb 施加在 t 上） */
    const int32_t x = s_from.x1 + (to.x1 - s_from.x1) * t / 1000;
    const int32_t y = s_from.y1 + (to.y1 - s_from.y1) * t / 1000;
    const int32_t w = fw + (tw - fw) * t / 1000;
    const int32_t h = fh + (th - fh) * t / 1000;

    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    /* 圆角从「圆形」(=直径一半) 收敛到卡片圆角；半透明遮罩与内容随进度淡入淡出 */
    lv_obj_set_style_radius(card, (fw / 2) + (CARD_RADIUS - fw / 2) * t / 1000, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_veil, (lv_opa_t)(t * 180 / 1000), LV_PART_MAIN);
    lv_obj_set_style_opa(s_card_body, (lv_opa_t)(t >= 700 ? (t - 700) * 255 / 300 : 0), LV_PART_MAIN);
    /* 底色从气泡色过渡到卡片深色：前段保持气泡色，后段切深色 */
    lv_obj_set_style_bg_color(card, t < 500 ? s_from_color : lv_color_hex(0x1b1b22), LV_PART_MAIN);
}

static void card_closed_cb(lv_anim_t * a)
{
    (void)a;
    /* 本版 LVGL 中 lv_obj_add_flag(HIDDEN) 弃用触发 -Werror，用 set_hidden */
    lv_obj_set_hidden(s_veil, true);
    lv_obj_set_hidden(s_card, true);
    /* 清除神灯遗留的 transform，避免影响下次展开 */
    lv_obj_set_style_translate_x(s_card, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(s_card, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(s_card, GENIE_SCALE_FULL, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(s_card, GENIE_SCALE_FULL, LV_PART_MAIN);
    lv_obj_set_style_opa(s_card, LV_OPA_COVER, LV_PART_MAIN);
}

/* 神灯收起：卡片朝气泡方向加速位移 + 各向同性缩小 + 渐隐。t ∈ [0,1000] */
static void card_genie_exec(void * var, int32_t t)
{
    lv_obj_t * card = (lv_obj_t *)var;

    /* 居中卡片中心 → 目标气泡中心的位移矢量 */
    lv_area_t centered;
    card_target_area(&centered);
    const int32_t cx0 = (centered.x1 + centered.x2) / 2;
    const int32_t cy0 = (centered.y1 + centered.y2) / 2;
    const int32_t tx  = (s_from.x1 + s_from.x2) / 2;
    const int32_t ty  = (s_from.y1 + s_from.y2) / 2;

    /* 位移用加速曲线 acc=p²（越接近气泡越快）；缩放/透明用线性 t */
    const int32_t acc = t * t / 1000;   /* 0..1000 的平方归一 */
    lv_obj_set_style_translate_x(card, (tx - cx0) * acc / 1000, LV_PART_MAIN);
    lv_obj_set_style_translate_y(card, (ty - cy0) * acc / 1000, LV_PART_MAIN);

    /* 缩放 256→GENIE_SCALE_MIN，各向同性 */
    const int32_t sc = GENIE_SCALE_FULL + (GENIE_SCALE_MIN - GENIE_SCALE_FULL) * t / 1000;
    lv_obj_set_style_transform_scale_x(card, sc, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(card, sc, LV_PART_MAIN);

    /* 渐隐；遮罩同步淡出 */
    lv_obj_set_style_opa(card, (lv_opa_t)(255 - 255 * t / 1000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_veil, (lv_opa_t)(180 - 180 * t / 1000), LV_PART_MAIN);
}
static void card_animate(int32_t from_t, int32_t to_t, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_card);
    lv_anim_set_values(&a, from_t, to_t);
    lv_anim_set_duration(&a, CARD_ANIM_MS);
    lv_anim_set_exec_cb(&a, card_anim_exec);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    if(done) lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

static void card_close(void)
{
    if(!s_open) return;
    s_open = false;

    /* 停掉可能还在跑的展开动画，锁定卡片为完整居中态作为神灯基准 */
    lv_anim_delete(s_card, card_anim_exec);
    lv_area_t centered;
    card_target_area(&centered);
    lv_obj_set_pos(s_card, centered.x1, centered.y1);
    lv_obj_set_size(s_card, CARD_W, CARD_H);
    lv_obj_set_style_radius(s_card, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(0x1b1b22), LV_PART_MAIN);
    /* 缩放围绕卡片中心 */
    lv_obj_set_style_transform_pivot_x(s_card, CARD_W / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(s_card, CARD_H / 2, LV_PART_MAIN);

    /* 神灯吸走：位移+缩小+渐隐，结束后隐藏并清 transform */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_card);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_duration(&a, GENIE_MS);
    lv_anim_set_exec_cb(&a, card_genie_exec);
    /* path 用线性；加速感由 exec 内各曲线自行控制，避免双重加速 */
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, card_closed_cb);
    lv_anim_start(&a);
}

static void card_open(int32_t idx)
{
    if(idx < 0 || idx >= UI_ITEM_CNT) return;

    /* 起点几何/底色取自被点气泡 */
    if(!lv_hex_menu_get_last_click_geom(s_menu, &s_from, &s_from_color)) return;

    lv_label_set_text(s_card_ico, s_icons[idx]);
    lv_obj_set_style_bg_color(s_card_ico, s_from_color, LV_PART_MAIN);
    lv_label_set_text(s_card_title, s_labels[idx]);

    /* 若上一次神灯收起还没跑完，先停掉并复位其遗留的 transform/opa */
    lv_anim_delete(s_card, card_genie_exec);
    lv_obj_set_style_translate_x(s_card, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(s_card, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(s_card, GENIE_SCALE_FULL, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(s_card, GENIE_SCALE_FULL, LV_PART_MAIN);
    lv_obj_set_style_opa(s_card, LV_OPA_COVER, LV_PART_MAIN);

    /* 起始与气泡完全重合的圆 */
    lv_obj_set_hidden(s_veil, false);
    lv_obj_set_hidden(s_card, false);
    lv_obj_set_style_opa(s_card_body, 0, LV_PART_MAIN);
    lv_obj_move_foreground(s_veil);
    lv_obj_move_foreground(s_card);

    s_open = true;
    lv_anim_delete(s_card, card_anim_exec);
    card_animate(0, 1000, NULL);
}

static void on_item_clicked(lv_event_t * e)
{
    const int32_t idx = (int32_t)(lv_uintptr_t)lv_event_get_param(e);
    if(idx >= 0 && idx < UI_ITEM_CNT) card_open(idx);
}

static void on_veil_clicked(lv_event_t * e)
{
    (void)e;
    card_close();   /* 点遮罩空白处收起 */
}

static void on_back_clicked(lv_event_t * e)
{
    (void)e;
    card_close();
}

static void build_card(void)
{
    lv_obj_t * top = lv_layer_top();   /* 顶层：盖住菜单，且不受菜单拖拽/缩放影响 */

    /* 半透明遮罩，铺满全屏，可点击收起 */
    s_veil = lv_obj_create(top);
    lv_obj_remove_style_all(s_veil);
    lv_obj_set_size(s_veil, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_veil, lv_color_hex(0x101014), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_veil, 0, LV_PART_MAIN);
    lv_obj_set_clickable(s_veil, true);
    lv_obj_add_event_cb(s_veil, on_veil_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_hidden(s_veil, true);

    /* 卡片本体 */
    s_card = lv_obj_create(top);
    lv_obj_remove_style_all(s_card);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(0x1b1b22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_card, CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_card, true, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_card, 40, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_card, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_hidden(s_card, true);

    /* 顶部大图标（圆底） */
    s_card_ico = lv_label_create(s_card);
    lv_obj_set_size(s_card_ico, 64, 64);
    lv_obj_set_style_radius(s_card_ico, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_card_ico, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_card_ico, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_card_ico, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_card_ico, LV_ALIGN_TOP_LEFT, 26, 26);
    /* 图标垂直居中：用内容对齐 */
    lv_obj_set_style_pad_top(s_card_ico, 20, LV_PART_MAIN);

    s_card_title = lv_label_create(s_card);
    lv_obj_set_style_text_color(s_card_title, lv_color_hex(0xf2f2f5), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_card_title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_card_title, LV_ALIGN_TOP_LEFT, 104, 40);

    /* 内容区：整体随动画后段淡入 */
    s_card_body = lv_obj_create(s_card);
    lv_obj_remove_style_all(s_card_body);
    lv_obj_set_size(s_card_body, CARD_W - 40, CARD_H - 130);
    lv_obj_align(s_card_body, LV_ALIGN_TOP_MID, 0, 108);
    lv_obj_set_flex_flow(s_card_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_card_body, 12, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_card_body, LV_SCROLLBAR_MODE_OFF);

    /* 两个占位行 */
    const char * rows[] = { "Enable", "Status" };
    for(uint32_t i = 0; i < 2; i++) {
        lv_obj_t * row = lv_obj_create(s_card_body);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x23232c), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, 16, LV_PART_MAIN);
        lv_obj_t * lb = lv_label_create(row);
        lv_label_set_text(lb, rows[i]);
        lv_obj_set_style_text_color(lb, lv_color_hex(0xd6d6dc), LV_PART_MAIN);
        lv_obj_align(lb, LV_ALIGN_LEFT_MID, 0, 0);
    }

    /* 返回按钮 */
    lv_obj_t * back = lv_button_create(s_card);
    lv_obj_set_size(back, CARD_W - 52, 46);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2f2f3a), LV_PART_MAIN);
    lv_obj_set_style_radius(back, 13, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back, on_back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t * blb = lv_label_create(back);
    lv_label_set_text(blb, LV_SYMBOL_LEFT "  Back");
    lv_obj_set_style_text_color(blb, lv_color_hex(0xf2f2f5), LV_PART_MAIN);
    lv_obj_center(blb);
}

void ui_init(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101014), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(scr, false);

    for(uint32_t i = 0; i < UI_ITEM_CNT; i++) {
        s_items[i].icon  = s_icons[i];
        s_items[i].label = s_labels[i];
        /* 色相均匀铺开；饱和度与亮度固定，保证白色图标始终可读 */
        s_items[i].color = lv_color_hsv_to_rgb((uint16_t)(i * 360u / UI_ITEM_CNT), 62, 88);
    }

    s_menu = lv_hex_menu_create(scr);
    lv_hex_menu_set_items(s_menu, s_items, UI_ITEM_CNT);
    lv_hex_menu_set_event_cb(s_menu, on_item_clicked, NULL);

    build_card();

    /* 确保鼠标光标位于其所属系统层的最前。 */
    lv_indev_t * indev = NULL;
    while((indev = lv_indev_get_next(indev)) != NULL) {
        lv_obj_t * cursor = lv_indev_get_cursor(indev);
        if(cursor != NULL) lv_obj_move_to_index(cursor, -1);
    }
}


