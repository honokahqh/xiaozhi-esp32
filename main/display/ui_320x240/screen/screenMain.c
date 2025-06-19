#include "screenMain.h"

#include <esp_log.h>
#include <esp_lvgl_port.h>

#define COMM_TEXT_WIDTH 160
#define LOADING_TEXT_WIDTH 220

static const char *TAG = "screenMain";

void ScreenMain_init() {
    ui_init();
    lv_spinner_set_anim_params(ui_MainSpinLoad, 1000 * 2, 200);
    ScreenMain_setMainPanel(SCREEN_MAIN_STATE_LOADING);
    ScreenMain_setRolerText(COMM_ROLER_ROBOT, "。。。");
    ScreenMain_setRolerText(COMM_ROLER_USER, "hi");
    ScreenMain_setLoadingText("正在加载...");

    lv_label_set_text(ui_MainLabelWiFiInfo, "");
    lv_label_set_text(ui_MainLabelStatus, "");
    lv_label_set_text(ui_MainLabelBat, "");
    lv_obj_add_flag(ui_MainLabelBat, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "ScreenMain_init");
}

void ScreenMain_setMainPanel(screen_main_state_t state) {
    switch (state) {
        case SCREEN_MAIN_STATE_SLEEP:
            lv_obj_clear_flag(ui_MainPanelSleep, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_MainPanelComm, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_MainLoading, LV_OBJ_FLAG_HIDDEN);
            break;
        case SCREEN_MAIN_STATE_COMM:
            lv_obj_add_flag(ui_MainPanelSleep, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_MainPanelComm, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_MainLoading, LV_OBJ_FLAG_HIDDEN);
            break;
        case SCREEN_MAIN_STATE_LOADING:
            lv_obj_add_flag(ui_MainPanelSleep, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_MainPanelComm, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_MainLoading, LV_OBJ_FLAG_HIDDEN);
            break;
        default:
            break;
    }
}

void ScreenMain_setLoadingText(const char *text) {
    lv_point_t size;
    lv_txt_get_size(&size, text, &ui_font_CN16, 0, 0, LOADING_TEXT_WIDTH, LV_TEXT_FLAG_NONE);
    if (size.x >= LOADING_TEXT_WIDTH - 16) {  // width - font size
        lv_obj_set_x(ui_MainLabelLoad, 0);
    } else {
        lv_obj_set_x(ui_MainLabelLoad, (LOADING_TEXT_WIDTH - 16 - size.x) / 2);
    }
    lv_label_set_text(ui_MainLabelLoad, text);
}

void ScreenMain_setStatus(const char *text) {
    lv_obj_clear_flag(ui_MainLabelStatus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MainLabelNotification, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_MainLabelStatus, text);
}

void ScreenMain_setNotification(const char *text) {
    lv_obj_add_flag(ui_MainLabelStatus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_MainLabelNotification, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_MainLabelNotification, text);
}

void ScreenMain_setWiFiIcon(screen_main_wifi_icon_t icon, const char *ssid) {
    switch (icon) {
        case WIIF_ICON_AP:
        case WIIF_ICON_NONE:
            lv_label_set_text(ui_MainIconWiFi, "无");
            lv_obj_set_style_text_color(ui_MainIconWiFi, lv_color_hex(WIFI_NONE_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
            break;
        case WIFI_ICON_WEEK:
        case WIFI_ICON_GOOD:
        case WIFI_ICON_EXCELLENT:
            lv_label_set_text(ui_MainIconWiFi, "有");
            lv_obj_set_style_text_color(ui_MainIconWiFi, lv_color_hex(WIFI_GOOD_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
            break;
        default:
            break;
    }
    if (ssid) {
        lv_label_set_text(ui_MainLabelWiFiInfo, ssid);
    } else {
        lv_label_set_text(ui_MainLabelWiFiInfo, "");
    }
}

void ScreenMain_setBatIcon(screen_main_bat_icon_t icon, uint32_t color, int percent) {
    if (icon == BAT_ICON_NONE) {
        lv_obj_add_flag(ui_MainIconBat, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_MainLabelBat, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(ui_MainIconBat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_MainLabelBat, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(ui_MainLabelBat, "%d%%", percent);
    lv_obj_set_style_text_color(ui_MainIconBat, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    switch (icon) {
        case BAT_ICON_LEVEL_0:
            lv_label_set_text(ui_MainIconBat, "0");
            break;
        case BAT_ICON_LEVEL_1:
            lv_label_set_text(ui_MainIconBat, "1");
            break;
        case BAT_ICON_LEVEL_2:
            lv_label_set_text(ui_MainIconBat, "2");
            break;
        case BAT_ICON_LEVEL_3:
            lv_label_set_text(ui_MainIconBat, "3");
            break;
        default:
            break;
    }
}

void ScreenMain_setRolerText(screen_main_comm_roler_t roler, const char *text) {
    lv_point_t size;
    lv_txt_get_size(&size, text, &ui_font_CN16, 0, 0, COMM_TEXT_WIDTH, LV_TEXT_FLAG_NONE);
    if (roler == COMM_ROLER_USER) {
        if (size.x == 0) {
            lv_obj_set_width(ui_MainPanelUserComm, 60);
            lv_obj_set_height(ui_MainPanelUserComm, 40);
            lv_label_set_text(ui_MainLabelUserComm, "。。");
            lv_obj_set_width(ui_MainLabelUserComm, 40);
            lv_obj_set_height(ui_MainLabelUserComm, 18);
        } else {
            lv_obj_set_width(ui_MainPanelUserComm, 40 + size.x);
            lv_obj_set_height(ui_MainPanelUserComm, 24 + size.y);
            lv_label_set_text(ui_MainLabelUserComm, text);
            lv_obj_set_width(ui_MainLabelUserComm, size.x);
            lv_obj_set_height(ui_MainLabelUserComm, size.y);
        }
    } else {
        if (size.x == 0) {
            lv_obj_set_width(ui_MainPanelRobotComm, 60);
            lv_obj_set_height(ui_MainPanelRobotComm, 40);
            lv_label_set_text(ui_MainLabelRobotComm, "。。");
            lv_obj_set_width(ui_MainLabelRobotComm, 40);
            lv_obj_set_height(ui_MainLabelRobotComm, 18);
        } else {
            lv_obj_set_width(ui_MainPanelRobotComm, 40 + size.x);
            lv_obj_set_height(ui_MainPanelRobotComm, 24 + size.y);
            lv_label_set_text(ui_MainLabelRobotComm, text);
            lv_obj_set_width(ui_MainLabelRobotComm, size.x);
            lv_obj_set_height(ui_MainLabelRobotComm, size.y);
        }
    }
}