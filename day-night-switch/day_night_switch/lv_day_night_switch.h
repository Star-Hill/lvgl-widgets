#ifndef LV_DAY_NIGHT_SWITCH_H
#define LV_DAY_NIGHT_SWITCH_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 日夜切换开关。移植自 uiverse.io 的纯 CSS 组件：
 * 白天为蓝底黄日 + 飘动的云，夜晚为黑底白月 + 月坑 + 滑入的闪烁星空。
 * 切换时太阳自转 360°、云随日月一同右移出界、星空自上方滑入。
 *
 * 原始 CSS 以 60x34 px 设计；本实现按 DNS_SCALE 整体放大以适配大屏。
 */

/** 创建开关。默认白天态。 */
lv_obj_t * lv_day_night_switch_create(lv_obj_t * parent);

/** 查询是否处于夜晚态 */
bool lv_day_night_switch_is_night(lv_obj_t * obj);

/**
 * 设置日夜状态。
 * @param anim  true 播放过渡动画，false 立即切换
 */
void lv_day_night_switch_set_night(lv_obj_t * obj, bool night, bool anim);

/**
 * 状态改变时以 LV_EVENT_VALUE_CHANGED 触发。
 * 回调中用 lv_day_night_switch_is_night() 读取新状态。
 */
void lv_day_night_switch_set_event_cb(lv_obj_t * obj, lv_event_cb_t cb, void * user_data);

#ifdef __cplusplus
}
#endif
#endif /*LV_DAY_NIGHT_SWITCH_H*/
