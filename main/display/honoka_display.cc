#include "honoka_display.h"

#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "board.h"
#include "screenMain.h"
#include "settings.h"

#define TAG "HonokaDisplay"

HonokaDisplay::HonokaDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height) : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    // Load theme from settings
    Settings settings("display", false);
    current_theme_name_ = settings.GetString("theme", "light");
}

HonokaSpiLcdDisplay::HonokaSpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x, int offset_y,
                                         bool mirror_x, bool mirror_y, bool swap_xy)
    : HonokaDisplay(panel_io, panel, width, height) {
    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 20),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation =
            {
                .swap_xy = swap_xy,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .swap_bytes = 1,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    DisplayLockGuard lock(this);

    ScreenMain_init();

    notification_label_ = ui_MainLabelNotification;
    status_label_ = ui_MainLabelStatus;
}

HonokaDisplay::~HonokaDisplay() {}

bool HonokaDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void HonokaDisplay::Unlock() { lvgl_port_unlock(); }

void HonokaDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    ScreenMain_setStatus(status);
}

void HonokaDisplay::ShowNotification(const std::string& notification, int duration_ms) { ShowNotification(notification.c_str(), duration_ms); }

void HonokaDisplay::ShowNotification(const char* notification, int duration_ms) {
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "ShowNotification: %s", notification);
    ScreenMain_setNotification(notification);

    esp_timer_stop(notification_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));
}

void HonokaDisplay::SetEmotion(const char* emotion) {}

void HonokaDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "SetChatMessage: %s: %s", role, content);
    if (strcmp(role, "user") == 0) {
        ScreenMain_setRolerText(COMM_ROLER_USER, content);
    } else if (strcmp(role, "assistant") == 0) {
        ScreenMain_setRolerText(COMM_ROLER_ROBOT, content);
    } else if (strcmp(role, "system") == 0) {
        ScreenMain_setLoadingText(content);
    }
}

void HonokaDisplay::SetIcon(const char* icon) {}

void HonokaDisplay::UpdateStatusBar(bool update_all) {
    auto& board = Board::GetInstance();

    esp_pm_lock_acquire(pm_lock_);
    // 更新电池图标
    int battery_level;
    bool charging, discharging;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        uint32_t battery_color = BAT_CRIT_COLOR;
        screen_main_bat_icon_t levels[] = {
            BAT_ICON_LEVEL_0,  // 0-24%
            BAT_ICON_LEVEL_1,  // 25-59%
            BAT_ICON_LEVEL_2,  // 40-74%
            BAT_ICON_LEVEL_3,  // 75-99%
            BAT_ICON_LEVEL_3,  // 100%
        };

        screen_main_bat_icon_t icon = levels[battery_level / 25];
        if (icon == BAT_ICON_LEVEL_0) {
            battery_color = BAT_CRIT_COLOR;
        } else if (icon == BAT_ICON_LEVEL_1) {
            battery_color = BAT_WARN_COLOR;
        } else {
            battery_color = BAT_SAFE_COLOR;
        }

        if (charging) {
            bat_anime = !bat_anime;
            if (icon != BAT_ICON_LEVEL_3) {
                icon = static_cast<screen_main_bat_icon_t>(icon + bat_anime);  // 强转回枚举类型
            }
        }
        DisplayLockGuard lock(this);
        ScreenMain_setBatIcon(icon, battery_color, battery_level);
    } else {
        ScreenMain_setBatIcon(BAT_ICON_NONE, 0, 0);
    }

    {
        // 更新WiFi图标
        DisplayLockGuard lock(this);
        int8_t rssi;
        auto wifiConnected = board.GetNetworkRssi(rssi);
        auto ssid = board.GetNetWorkSsid();
        screen_main_wifi_icon_t wifi_strength = WIIF_ICON_NONE;
        if (!wifiConnected) {
            wifi_strength = WIIF_ICON_NONE;
        } else if (rssi >= -60) {
            wifi_strength = WIFI_ICON_EXCELLENT;
        } else if (rssi >= -70) {
            wifi_strength = WIFI_ICON_GOOD;
        } else {
            wifi_strength = WIFI_ICON_WEEK;
        }
        ScreenMain_setWiFiIcon(wifi_strength, ssid.c_str());
    }

    esp_pm_lock_release(pm_lock_);
}

void HonokaDisplay::SetMainPanel(ScreenPanelId panel_id) {
    if (panel_id == panel_id_) {
        return;
    }
    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "SetMainPanel: %d", panel_id);
    ScreenMain_setMainPanel(static_cast<screen_main_state_t>(panel_id));  // 定义一致,直接转换
    panel_id_ = panel_id;
}