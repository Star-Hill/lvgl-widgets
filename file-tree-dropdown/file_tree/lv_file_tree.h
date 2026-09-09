#ifndef LV_FILE_TREE_H
#define LV_FILE_TREE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 文件树下拉组件。移植自一个 Tailwind CSS 组件：
 * 白色圆角卡片作触发器（黄色文件夹图标 + 标题），下方弹出带层级缩进的目录列表。
 *
 * 原组件靠 `group-hover` 展开；触摸屏没有 hover，故改为点击切换：
 * 点触发器展开／收起，点列表以外的地方也收起。展开与收起均为淡入淡出
 * （对应 CSS 的 transition-opacity duration-1000）。
 */

/** 目录项类型，决定用哪个图标 */
typedef enum {
    LV_FILE_TREE_FOLDER,
    LV_FILE_TREE_FILE,
} lv_file_tree_kind_t;

typedef struct {
    const char *        name;   /**< 显示的名字 */
    lv_file_tree_kind_t kind;   /**< 文件夹或文件 */
    uint8_t             depth;  /**< 缩进层级，0 为顶层（对应 CSS 的 pl-4 / pl-8） */
} lv_file_tree_item_t;

/**
 * 创建组件。
 * @param parent  父对象
 * @param title   触发器上的标题，如 "Project Structure"
 */
lv_obj_t * lv_file_tree_create(lv_obj_t * parent, const char * title);

/**
 * 设置目录内容。items 由调用方保证生命周期长于组件（不做拷贝）。
 */
void lv_file_tree_set_items(lv_obj_t * obj, const lv_file_tree_item_t * items, uint32_t cnt);

/** 展开或收起下拉列表 */
void lv_file_tree_set_open(lv_obj_t * obj, bool open, bool anim);

/** 当前是否展开 */
bool lv_file_tree_is_open(lv_obj_t * obj);

#ifdef __cplusplus
}
#endif
#endif /*LV_FILE_TREE_H*/
