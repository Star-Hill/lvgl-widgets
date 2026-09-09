#include "ui.h"
#include "lvgl.h"
#include "lv_day_night_switch.h"
#include <stdio.h>

static lv_obj_t * s_hint = NULL;

static void on_toggled(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target_obj(e);
    const bool night = lv_day_night_switch_is_night(sw);

    if(s_hint != NULL) {
        lv_label_set_text(s_hint, night ? "Night" : "Day");
        lv_obj_set_style_text_color(s_hint,
            lv_color_hex(night ? 0xE8E8F0 : 0xF5C518), LV_PART_MAIN);
    }
    printf("day-night-switch: %s\n", night ? "NIGHT" : "DAY");
    fflush(stdout);
}

void ui_init(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x20242E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(scr, false);

    lv_obj_t * sw = lv_day_night_switch_create(scr);
    lv_obj_center(sw);
    lv_day_night_switch_set_event_cb(sw, on_toggled, NULL);

    s_hint = lv_label_create(scr);
    lv_label_set_text(s_hint, "Day");
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0xF5C518), LV_PART_MAIN);
    lv_obj_align_to(s_hint, sw, LV_ALIGN_OUT_BOTTOM_MID, 0, 28);

    lv_obj_t * tip = lv_label_create(scr);
    lv_label_set_text(tip, "Tap the switch to toggle day / night");
    lv_obj_set_style_text_color(tip, lv_color_hex(0x8A8A92), LV_PART_MAIN);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -24);
}
