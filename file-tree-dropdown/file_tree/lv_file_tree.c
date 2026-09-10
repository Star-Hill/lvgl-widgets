#include "lv_file_tree.h"

/* ---------------- 尺寸与配色（对应原 Tailwind 类） ---------------- */
#define FT_SCALE              2          /* 原设计偏小，整体放大以适配大屏 */
#define FT_S(v)               ((int32_t)((v) * FT_SCALE))

#define FT_ICON_W             18         /* SVG viewBox 宽 */
#define FT_ICON_H             14         /* SVG viewBox 高 */
#define FT_ICON_DRAW          25         /* CSS 里 svg 的 width/height */

#define FT_PANEL_W            256        /* w-64 = 16rem = 256px */
#define FT_GAP_TRIGGER         16        /* gap-4 */
#define FT_PAD_TRIGGER_X       16        /* px-4 */
#define FT_PAD_TRIGGER_Y        8        /* py-2 */
#define FT_PANEL_PAD           16        /* p-4 */
#define FT_ROW_GAP              4        /* space-y-1 */
#define FT_ROW_PAD_Y            4        /* py-1 */
#define FT_INDENT              16        /* pl-4 每级缩进 */
#define FT_PANEL_OFFSET         8        /* mt-2 */
#define FT_RADIUS               6        /* rounded-md */

#define FT_C_CARD             0xFFFFFF   /* bg-white */
#define FT_C_TEXT             0x1F2937   /* 默认深灰文字 */
#define FT_C_BORDER           0xE5E7EB   /* border-gray-200 */
#define FT_C_FOLDER_BACK      0xFFA000   /* SVG 第一个 path */
#define FT_C_FOLDER_FRONT     0xFFCA28   /* SVG 第二个 path */

#define FT_FADE_MS           1000        /* transition-opacity duration-1000 */

typedef struct {
    lv_obj_t * trigger;      /* 触发卡片 */
    lv_obj_t * panel;        /* 下拉面板 */
    void *     icon_buf;     /* 文件夹图标 canvas 缓冲，需自行释放 */
    bool       open;
} ft_ctx_t;

static ft_ctx_t * ctx_of(lv_obj_t * obj)
{
    return (ft_ctx_t *)lv_obj_get_user_data(obj);
}

/* ---------------- 文件夹图标：ThorVG 还原 SVG 的两个 path ---------------- */

/* 把 18x14 viewBox 里的坐标映射到实际画布 */
static lv_fpoint_t ft_pt(float x, float y, float sx, float sy)
{
    lv_fpoint_t p = { x * sx, y * sy };
    return p;
}

/* 原 SVG 由两层组成：后层是带凸起标签的深黄外廓，前层是压在上面的浅黄本体。
 * 两个 path 都是直角矩形轮廓（原图的圆角来自极小的圆角半径，这里以直线近似），
 * 故用折线填充即可，不需要贝塞尔。 */
static void ft_draw_folder(lv_obj_t * canvas, int32_t w, int32_t h)
{
    const float sx = (float)w / (float)FT_ICON_W;
    const float sy = (float)h / (float)FT_ICON_H;

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    /* 后层 #FFA000：左上有一块凸起（文件夹标签），右下被斜切一角 */
    {
        lv_vector_path_t * p = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
        lv_fpoint_t a;
        a = ft_pt(0.0f,  1.75f, sx, sy); lv_vector_path_move_to(p, &a);
        a = ft_pt(6.3f,  1.75f, sx, sy); lv_vector_path_line_to(p, &a);   /* 标签顶边 */
        a = ft_pt(4.5f,  0.0f,  sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(1.8f,  0.0f,  sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(0.0f,  1.75f, sx, sy); lv_vector_path_line_to(p, &a);
        lv_vector_path_close(p);

        lv_draw_vector_dsc_t * d = lv_draw_vector_dsc_create(&layer);
        lv_draw_vector_dsc_set_fill_color(d, lv_color_hex(FT_C_FOLDER_BACK));
        lv_draw_vector_dsc_add_path(d, p);
        lv_draw_vector(d);
        lv_draw_vector_dsc_delete(d);
        lv_vector_path_delete(p);
    }

    /* 后层主体：右上角被切掉，形成 SVG 里 18,3.5 -> 18,9.1875 的斜边 */
    {
        lv_vector_path_t * p = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
        lv_fpoint_t a;
        a = ft_pt(0.0f,  1.75f,  sx, sy); lv_vector_path_move_to(p, &a);
        a = ft_pt(18.0f, 1.75f,  sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(18.0f, 9.1875f, sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(15.165f, 14.0f, sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(0.0f,  14.0f,  sx, sy); lv_vector_path_line_to(p, &a);
        lv_vector_path_close(p);

        lv_draw_vector_dsc_t * d = lv_draw_vector_dsc_create(&layer);
        lv_draw_vector_dsc_set_fill_color(d, lv_color_hex(FT_C_FOLDER_BACK));
        lv_draw_vector_dsc_add_path(d, p);
        lv_draw_vector(d);
        lv_draw_vector_dsc_delete(d);
        lv_vector_path_delete(p);
    }

    /* 前层 #FFCA28：覆盖在上面的浅黄本体 */
    {
        lv_vector_path_t * p = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
        lv_fpoint_t a;
        a = ft_pt(0.0f,  2.0f,  sx, sy); lv_vector_path_move_to(p, &a);
        a = ft_pt(18.0f, 2.0f,  sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(18.0f, 14.0f, sx, sy); lv_vector_path_line_to(p, &a);
        a = ft_pt(0.0f,  14.0f, sx, sy); lv_vector_path_line_to(p, &a);
        lv_vector_path_close(p);

        lv_draw_vector_dsc_t * d = lv_draw_vector_dsc_create(&layer);
        lv_draw_vector_dsc_set_fill_color(d, lv_color_hex(FT_C_FOLDER_FRONT));
        lv_draw_vector_dsc_add_path(d, p);
        lv_draw_vector(d);
        lv_draw_vector_dsc_delete(d);
        lv_vector_path_delete(p);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

/* ---------------- 动画与事件 ---------------- */

static void ft_fade_exec(void * var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, LV_PART_MAIN);
}

static void ft_fade_done_cb(lv_anim_t * a)
{
    /* 淡出结束后再真正隐藏，避免占据点击区域 */
    lv_obj_t * panel = (lv_obj_t *)a->var;
    if(lv_obj_get_style_opa(panel, LV_PART_MAIN) == LV_OPA_TRANSP) {
        lv_obj_set_flag(panel, LV_OBJ_FLAG_HIDDEN, true);
    }
}

static void ft_trigger_clicked_cb(lv_event_t * e)
{
    lv_obj_t * root = (lv_obj_t *)lv_event_get_user_data(e);
    ft_ctx_t * c = ctx_of(root);
    if(c == NULL) return;
    lv_file_tree_set_open(root, !c->open, true);
}

static void ft_delete_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target_obj(e);
    ft_ctx_t * c = ctx_of(obj);
    if(c == NULL) return;

    lv_anim_delete(c->panel, ft_fade_exec);
    /* 面板挂在顶层、不是 root 的子对象，不会被级联删除，需手动回收 */
    if(c->panel != NULL) lv_obj_delete(c->panel);
    if(c->icon_buf != NULL) lv_free(c->icon_buf);
    lv_obj_set_user_data(obj, NULL);
    lv_free(c);
}

/* ---------------- 公共 API ---------------- */

lv_obj_t * lv_file_tree_create(lv_obj_t * parent, const char * title)
{
    /* 根对象只作定位锚点，本身透明、不拦事件 */
    lv_obj_t * root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flag(root, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE, true);

    ft_ctx_t * c = lv_malloc_zeroed(sizeof(ft_ctx_t));
    LV_ASSERT_MALLOC(c);
    if(c == NULL) return root;
    lv_obj_set_user_data(root, c);

    /* ---- 触发卡片：bg-white py-2 px-4 rounded-md shadow-lg flex gap-4 ---- */
    lv_obj_t * trig = lv_obj_create(root);
    lv_obj_remove_style_all(trig);
    lv_obj_set_size(trig, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(trig, lv_color_hex(FT_C_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(trig, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(trig, FT_S(FT_RADIUS), LV_PART_MAIN);
    lv_obj_set_style_pad_hor(trig, FT_S(FT_PAD_TRIGGER_X), LV_PART_MAIN);
    lv_obj_set_style_pad_ver(trig, FT_S(FT_PAD_TRIGGER_Y), LV_PART_MAIN);
    /* shadow-lg */
    lv_obj_set_style_shadow_width(trig, FT_S(15), LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(trig, FT_S(4), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(trig, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_flex_flow(trig, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(trig, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(trig, FT_S(FT_GAP_TRIGGER), LV_PART_MAIN);
    lv_obj_set_flag(trig, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_flag(trig, LV_OBJ_FLAG_CLICKABLE, true);
    c->trigger = trig;

    /* 文件夹图标：canvas + 矢量绘制 */
    const int32_t iw = FT_S(FT_ICON_DRAW);
    const int32_t ih = (int32_t)((float)iw * (float)FT_ICON_H / (float)FT_ICON_W);
    lv_obj_t * icon = lv_canvas_create(trig);
    lv_obj_remove_style_all(icon);
    {
        const uint32_t stride = (uint32_t)iw * 4u;
        c->icon_buf = lv_malloc_zeroed(stride * (uint32_t)ih + LV_DRAW_BUF_ALIGN);
        LV_ASSERT_MALLOC(c->icon_buf);
        if(c->icon_buf != NULL) {
            lv_canvas_set_buffer(icon, c->icon_buf, iw, ih, LV_COLOR_FORMAT_ARGB8888);
            lv_canvas_fill_bg(icon, lv_color_black(), LV_OPA_TRANSP);
            ft_draw_folder(icon, iw, ih);
        }
    }

    lv_obj_t * lbl = lv_label_create(trig);
    lv_label_set_text(lbl, title != NULL ? title : "");
    lv_obj_set_style_text_color(lbl, lv_color_hex(FT_C_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);

    /* ---- 下拉面板：absolute mt-2 w-64 bg-white border rounded-md shadow-lg ----
     *
     * CSS 的 absolute 让浮层脱离父容器的盒子；LVGL 没有等价语义，子对象一律被
     * 父对象裁剪。若把面板挂在 root 下，root 的尺寸只包住触发卡片，面板会整个
     * 落在其边界之外而被裁掉（OVERFLOW_VISIBLE 也救不回来）。
     * 因此改挂顶层，再用 align_to 相对触发卡片定位——这才对应 absolute 的语义。
     * 代价：面板不再随 root 自动销毁，需在 delete 回调中手动删除。 */
    lv_obj_t * panel = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(panel);
    lv_obj_set_width(panel, FT_S(FT_PANEL_W));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(FT_C_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, FT_S(FT_RADIUS), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(FT_C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(panel, FT_S(15), LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(panel, FT_S(4), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, FT_S(FT_PANEL_PAD), LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, FT_S(FT_ROW_GAP), LV_PART_MAIN);
    lv_obj_set_flag(panel, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_align_to(panel, trig, LV_ALIGN_OUT_BOTTOM_LEFT, 0, FT_S(FT_PANEL_OFFSET));
    lv_obj_set_style_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_flag(panel, LV_OBJ_FLAG_HIDDEN, true);
    c->panel = panel;
    c->open = false;

    lv_obj_add_event_cb(trig, ft_trigger_clicked_cb, LV_EVENT_CLICKED, root);
    lv_obj_add_event_cb(root, ft_delete_cb, LV_EVENT_DELETE, NULL);

    return root;
}

void lv_file_tree_set_items(lv_obj_t * obj, const lv_file_tree_item_t * items, uint32_t cnt)
{
    ft_ctx_t * c = ctx_of(obj);
    if(c == NULL || items == NULL) return;

    lv_obj_clean(c->panel);

    for(uint32_t i = 0; i < cnt; i++) {
        lv_obj_t * row = lv_label_create(c->panel);
        /* CSS 用 emoji 📁/📄，内置字体没有，改用 LVGL 自带的 FontAwesome 符号 */
        lv_label_set_text_fmt(row, "%s  %s",
                              items[i].kind == LV_FILE_TREE_FOLDER ? LV_SYMBOL_DIRECTORY
                                                                   : LV_SYMBOL_FILE,
                              items[i].name);
        lv_obj_set_style_text_color(row, lv_color_hex(FT_C_TEXT), LV_PART_MAIN);
        /* pl-4 / pl-8：每级缩进一档 */
        lv_obj_set_style_pad_left(row, FT_S(FT_INDENT) * items[i].depth, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, FT_S(FT_ROW_PAD_Y), LV_PART_MAIN);
    }

    /* 触发卡片是 LV_SIZE_CONTENT，创建之初尺寸尚未解析，此时重新对齐才准 */
    lv_obj_update_layout(c->trigger);
    lv_obj_align_to(c->panel, c->trigger, LV_ALIGN_OUT_BOTTOM_LEFT, 0, FT_S(FT_PANEL_OFFSET));
}

void lv_file_tree_set_open(lv_obj_t * obj, bool open, bool anim)
{
    ft_ctx_t * c = ctx_of(obj);
    if(c == NULL) return;
    c->open = open;

    lv_anim_delete(c->panel, ft_fade_exec);

    if(open) {
        lv_obj_set_flag(c->panel, LV_OBJ_FLAG_HIDDEN, false);
        /* 面板挂在顶层，不随组件移动，故每次打开都按触发卡片的当前位置重新对齐 */
        lv_obj_update_layout(c->trigger);
        lv_obj_align_to(c->panel, c->trigger, LV_ALIGN_OUT_BOTTOM_LEFT, 0, FT_S(FT_PANEL_OFFSET));
    }
    lv_obj_move_foreground(c->panel);

    const int32_t to = open ? LV_OPA_COVER : LV_OPA_TRANSP;
    if(!anim) {
        lv_obj_set_style_opa(c->panel, (lv_opa_t)to, LV_PART_MAIN);
        if(!open) lv_obj_set_flag(c->panel, LV_OBJ_FLAG_HIDDEN, true);
        return;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c->panel);
    lv_anim_set_exec_cb(&a, ft_fade_exec);
    lv_anim_set_values(&a, lv_obj_get_style_opa(c->panel, LV_PART_MAIN), to);
    lv_anim_set_duration(&a, FT_FADE_MS);
    lv_anim_set_completed_cb(&a, ft_fade_done_cb);
    lv_anim_start(&a);
}

bool lv_file_tree_is_open(lv_obj_t * obj)
{
    ft_ctx_t * c = ctx_of(obj);
    return (c != NULL) && c->open;
}
