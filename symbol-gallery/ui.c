#include "ui.h"
#include "lvgl.h"

/* LVGL 内置符号总览。
 * 名单由 lvgl/font/lv_symbol_def.h 提取，排除了头文件宏与占位用的 DUMMY。
 * 这些符号取自 FontAwesome，码点位于 0xF0xx 私有区，可直接当字符串用：
 *     lv_label_set_text(label, LV_SYMBOL_WIFI " Connected");
 */

typedef struct {
    const char * sym;
    const char * name;
} sym_item_t;

static const sym_item_t SYMBOLS[] = {
    { LV_SYMBOL_BULLET, "BULLET" },
    { LV_SYMBOL_AUDIO, "AUDIO" },
    { LV_SYMBOL_VIDEO, "VIDEO" },
    { LV_SYMBOL_LIST, "LIST" },
    { LV_SYMBOL_OK, "OK" },
    { LV_SYMBOL_CLOSE, "CLOSE" },
    { LV_SYMBOL_POWER, "POWER" },
    { LV_SYMBOL_SETTINGS, "SETTINGS" },
    { LV_SYMBOL_HOME, "HOME" },
    { LV_SYMBOL_DOWNLOAD, "DOWNLOAD" },
    { LV_SYMBOL_DRIVE, "DRIVE" },
    { LV_SYMBOL_REFRESH, "REFRESH" },
    { LV_SYMBOL_MUTE, "MUTE" },
    { LV_SYMBOL_VOLUME_MID, "VOLUME_MID" },
    { LV_SYMBOL_VOLUME_MAX, "VOLUME_MAX" },
    { LV_SYMBOL_IMAGE, "IMAGE" },
    { LV_SYMBOL_TINT, "TINT" },
    { LV_SYMBOL_PREV, "PREV" },
    { LV_SYMBOL_PLAY, "PLAY" },
    { LV_SYMBOL_PAUSE, "PAUSE" },
    { LV_SYMBOL_STOP, "STOP" },
    { LV_SYMBOL_NEXT, "NEXT" },
    { LV_SYMBOL_EJECT, "EJECT" },
    { LV_SYMBOL_LEFT, "LEFT" },
    { LV_SYMBOL_RIGHT, "RIGHT" },
    { LV_SYMBOL_PLUS, "PLUS" },
    { LV_SYMBOL_MINUS, "MINUS" },
    { LV_SYMBOL_EYE_OPEN, "EYE_OPEN" },
    { LV_SYMBOL_EYE_CLOSE, "EYE_CLOSE" },
    { LV_SYMBOL_WARNING, "WARNING" },
    { LV_SYMBOL_SHUFFLE, "SHUFFLE" },
    { LV_SYMBOL_UP, "UP" },
    { LV_SYMBOL_DOWN, "DOWN" },
    { LV_SYMBOL_LOOP, "LOOP" },
    { LV_SYMBOL_DIRECTORY, "DIRECTORY" },
    { LV_SYMBOL_UPLOAD, "UPLOAD" },
    { LV_SYMBOL_CALL, "CALL" },
    { LV_SYMBOL_CUT, "CUT" },
    { LV_SYMBOL_COPY, "COPY" },
    { LV_SYMBOL_SAVE, "SAVE" },
    { LV_SYMBOL_BARS, "BARS" },
    { LV_SYMBOL_ENVELOPE, "ENVELOPE" },
    { LV_SYMBOL_CHARGE, "CHARGE" },
    { LV_SYMBOL_PASTE, "PASTE" },
    { LV_SYMBOL_BELL, "BELL" },
    { LV_SYMBOL_KEYBOARD, "KEYBOARD" },
    { LV_SYMBOL_GPS, "GPS" },
    { LV_SYMBOL_FILE, "FILE" },
    { LV_SYMBOL_WIFI, "WIFI" },
    { LV_SYMBOL_BATTERY_FULL, "BATTERY_FULL" },
    { LV_SYMBOL_BATTERY_3, "BATTERY_3" },
    { LV_SYMBOL_BATTERY_2, "BATTERY_2" },
    { LV_SYMBOL_BATTERY_1, "BATTERY_1" },
    { LV_SYMBOL_BATTERY_EMPTY, "BATTERY_EMPTY" },
    { LV_SYMBOL_USB, "USB" },
    { LV_SYMBOL_BLUETOOTH, "BLUETOOTH" },
    { LV_SYMBOL_TRASH, "TRASH" },
    { LV_SYMBOL_EDIT, "EDIT" },
    { LV_SYMBOL_BACKSPACE, "BACKSPACE" },
    { LV_SYMBOL_SD_CARD, "SD_CARD" },
    { LV_SYMBOL_NEW_LINE, "NEW_LINE" },
};
#define SYM_CNT ((uint32_t)(sizeof(SYMBOLS) / sizeof(SYMBOLS[0])))

#define GRID_COLS     10
#define CELL_W        76
#define CELL_H        56

static lv_obj_t * make_cell(lv_obj_t * parent, const sym_item_t * it)
{
    lv_obj_t * cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, CELL_W, CELL_H);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0x2A2F3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(cell, 8, LV_PART_MAIN);
    lv_obj_set_scrollable(cell, false);
    lv_obj_set_clickable(cell, false);

    /* 图标：用较大字号突出形状 */
    lv_obj_t * ico = lv_label_create(cell);
    lv_label_set_text(ico, it->sym);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ico, lv_color_hex(0xFFC24B), LV_PART_MAIN);
    lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 3);

    /* 名字：去掉 LV_SYMBOL_ 前缀，够短才能塞进格子 */
    lv_obj_t * name = lv_label_create(cell);
    lv_label_set_text(name, it->name);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(name, lv_color_hex(0xB9BFCC), LV_PART_MAIN);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(name, CELL_W - 4);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -2);

    return cell;
}

void ui_init(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x161A22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 8, LV_PART_MAIN);

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text_fmt(title, "LVGL built-in symbols  (%u)", (unsigned)SYM_CNT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF2F4F8), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    /* 网格容器：10 列 x 7 行恰好容下全部 61 个符号，无需滚动 */
    lv_obj_t * grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, GRID_COLS * (CELL_W + 3), LV_PCT(100) - 22);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, 3, LV_PART_MAIN);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);

    for(uint32_t i = 0; i < SYM_CNT; i++) {
        make_cell(grid, &SYMBOLS[i]);
    }
}
