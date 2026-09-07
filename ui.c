#include "ui.h"
#include "lvgl.h"
#include "lv_hex_menu.h"

#define UI_ITEM_CNT 37

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

    lv_obj_t * menu = lv_hex_menu_create(scr);
    lv_hex_menu_set_items(menu, s_items, UI_ITEM_CNT);
}
