#include "ui.h"
#include "lvgl.h"
#include "lv_file_tree.h"

/* 与原组件 <ul> 里的条目一一对应；depth 对应 CSS 的 pl-4 / pl-8 */
static const lv_file_tree_item_t s_items[] = {
    { "src",         LV_FILE_TREE_FOLDER, 0 },
    { "app",         LV_FILE_TREE_FOLDER, 1 },
    { "layout.js",   LV_FILE_TREE_FILE,   2 },
    { "page.js",     LV_FILE_TREE_FILE,   2 },
    { "components",  LV_FILE_TREE_FOLDER, 1 },
    { "header.js",   LV_FILE_TREE_FILE,   2 },
    { "footer.js",   LV_FILE_TREE_FILE,   2 },
    { "styles",      LV_FILE_TREE_FOLDER, 1 },
    { "globals.css", LV_FILE_TREE_FILE,   2 },
};
#define UI_ITEM_CNT ((uint32_t)(sizeof(s_items) / sizeof(s_items[0])))

void ui_init(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF3F4F6), LV_PART_MAIN);   /* 浅灰底衬出白卡片 */
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(scr, false);

    lv_obj_t * ft = lv_file_tree_create(scr, "Project Structure");
    lv_file_tree_set_items(ft, s_items, UI_ITEM_CNT);
    /* 靠上放，给展开的面板留出空间 */
    lv_obj_align(ft, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t * tip = lv_label_create(scr);
    lv_label_set_text(tip, "Tap the card to open / close the tree");
    lv_obj_set_style_text_color(tip, lv_color_hex(0x6B7280), LV_PART_MAIN);
    lv_obj_align(tip, LV_ALIGN_BOTTOM_MID, 0, -24);
}
