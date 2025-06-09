#ifndef HONOKA_DISPLAY_H
#define HONOKA_DISPLAY_H

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <atomic>

#include "display.h"

class HonokaDisplay : public Display {
   protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_draw_buf_t draw_buf_;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

   protected:
    // 添加protected构造函数
    HonokaDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height);
    bool bat_anime = false;

   public:
    ~HonokaDisplay();

    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void ShowNotification(const std::string& notification, int duration_ms = 3000) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetIcon(const char* icon) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetMainPanel(ScreenPanelId panel_id) override;
};

// // SPI LCD显示器
class HonokaSpiLcdDisplay : public HonokaDisplay {
   public:
    HonokaSpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x, int offset_y, bool mirror_x,
                        bool mirror_y, bool swap_xy);
};

#endif  // HONOKA_DISPLAY_H
