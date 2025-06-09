#ifndef __SCREEN_MAIN_H__
#define __SCREEN_MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "ui.h"

#define DEFAULT_TEXT_COLOR 0x202020
#define BAT_SAFE_COLOR 0x43B131
#define BAT_WARN_COLOR 0xE78429
#define BAT_CRIT_COLOR 0xB52020
#define BAT_CHARGE_COLOR BAT_SAFE_COLOR

#define WIFI_NONE_COLOR BAT_CRIT_COLOR
#define WIFI_WEEK_COLOR BAT_WARN_COLOR
#define WIFI_GOOD_COLOR 0x7E7E7E

typedef enum {
    SCREEN_MAIN_STATE_SLEEP = 0,
    SCREEN_MAIN_STATE_COMM = 1,
    SCREEN_MAIN_STATE_LOADING = 2,
} screen_main_state_t;

typedef enum {
    COMM_ROLER_USER = 0,
    COMM_ROLER_ROBOT = 1,
} screen_main_comm_roler_t;

typedef enum {
    WIIF_ICON_AP = 0,
    WIIF_ICON_NONE,
    WIFI_ICON_WEEK,
    WIFI_ICON_GOOD,
    WIFI_ICON_EXCELLENT,
} screen_main_wifi_icon_t;

typedef enum {
    BAT_ICON_LEVEL_0 = 0,
    BAT_ICON_LEVEL_1,
    BAT_ICON_LEVEL_2,
    BAT_ICON_LEVEL_3,
    BAT_ICON_NONE,
} screen_main_bat_icon_t;

void ScreenMain_init();
void ScreenMain_setMainPanel(screen_main_state_t state);
void ScreenMain_setLoadingText(const char *text);
void ScreenMain_setStatus(const char *text);
void ScreenMain_setNotification(const char *text);
void ScreenMain_setWiFiIcon(screen_main_wifi_icon_t icon, const char *ssid);
void ScreenMain_setRolerText(screen_main_comm_roler_t roler, const char *text);
void ScreenMain_setBatIcon(screen_main_bat_icon_t icon, uint32_t color, int percent);

#ifdef __cplusplus
}
#endif

#endif