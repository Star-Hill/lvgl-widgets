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

/** 点击菜单项时触发。回调中用 lv_event_get_user_data() 取 user_data。 */
void lv_hex_menu_set_event_cb(lv_obj_t * obj, lv_event_cb_t cb, void * user_data);

#ifdef __cplusplus
}
#endif
#endif /*LV_HEX_MENU_H*/
