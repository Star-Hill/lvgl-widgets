#ifndef LV_HEX_MENU_H
#define LV_HEX_MENU_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char * icon;    /**< LV_SYMBOL_* 或任意 UTF-8 文本 */
    const char * label;   /**< 保留给第二阶段的展开面板，当前不绘制 */
    lv_color_t   color;   /**< 圆底色 */
} lv_hex_menu_item_t;

/** 创建蜂窝菜单。默认填满父对象。 */
lv_obj_t * lv_hex_menu_create(lv_obj_t * parent);

/**
 * 设置菜单项。items 数组由调用方保证生命周期长于 widget（不做拷贝）。
 * 平铺周期固定 37 槽；cnt < 37 时按 items[slot % cnt] 映射，一个周期内会重复。
 * cnt == 37 时铺砌完美无重复，是设计目标场景。
 */
void lv_hex_menu_set_items(lv_obj_t * obj, const lv_hex_menu_item_t * items, uint32_t cnt);

/** 当前离屏幕中心最近的菜单项索引，0..cnt-1；未设置菜单项时返回 -1 */
int32_t lv_hex_menu_get_focused(lv_obj_t * obj);

/**
 * 点击菜单项时以 LV_EVENT_VALUE_CHANGED 触发。
 * 回调中用 `(int32_t)(lv_uintptr_t)lv_event_get_param(e)` 取得菜单项索引；
 * 索引 0 会编码为 NULL，但仍是合法值。用 lv_event_get_user_data() 取 user_data。
 * 再次调用会替换此前由本接口注册的回调；cb == NULL 表示解除注册。
 * 用户回调可以删除菜单对象。
 */
void lv_hex_menu_set_event_cb(lv_obj_t * obj, lv_event_cb_t cb, void * user_data);

/**
 * 取最近一次点击命中气泡的屏幕坐标与底色，用于从气泡原地展开的动画起点。
 * 仅在点击回调（LV_EVENT_VALUE_CHANGED）中调用有效。
 * @param out_area   命中气泡的屏幕绝对坐标框（可为 NULL 表示不需要）
 * @param out_color  命中气泡的底色（可为 NULL 表示不需要）
 * @return true 表示有有效的点击几何；false 表示尚无点击记录，输出参数不被修改
 */
bool lv_hex_menu_get_last_click_geom(lv_obj_t * obj, lv_area_t * out_area, lv_color_t * out_color);

#ifdef __cplusplus
}
#endif
#endif /*LV_HEX_MENU_H*/
