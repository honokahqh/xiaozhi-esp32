#ifndef BOARD_H
#define BOARD_H

#include <http.h>
#include <web_socket.h>
#include <mqtt.h>
#include <udp.h>
#include <string>

#include "led/led.h"
#include "driver/adc.h"
#include "backlight.h"
#include "camera.h"

void* create_board();
class AudioCodec;
class Display;
class Board {
private:
    Board(const Board&) = delete; // 禁用拷贝构造函数
    Board& operator=(const Board&) = delete; // 禁用赋值操作

protected:
    Board();
    std::string GenerateUuid();

    // 软件生成的设备唯一标识
    std::string uuid_;

public:
    static Board& GetInstance() {
        static Board* instance = static_cast<Board*>(create_board());
        return *instance;
    }

    virtual ~Board() = default;
    virtual std::string GetBoardType() = 0;
    virtual std::string GetUuid() { return uuid_; }
    virtual Backlight* GetBacklight() { return nullptr; }
    virtual Led* GetLed();
    virtual AudioCodec* GetAudioCodec() = 0;
    virtual bool GetTemperature(float& esp32temp);
    virtual Display* GetDisplay();
    virtual Camera* GetCamera();
    virtual Http* CreateHttp() = 0;
    virtual WebSocket* CreateWebSocket() = 0;
    virtual Mqtt* CreateMqtt() = 0;
    virtual Udp* CreateUdp() = 0;
    virtual void StartNetwork() = 0;
    virtual const char* GetNetworkStateIcon() = 0;
    virtual bool GetNetworkRssi(int8_t& rssi) { return -1; }
    virtual std::string GetNetWorkSsid() { return ""; } 
    virtual bool GetBatteryADCInfo(adc_channel_t& ch, float& ratio, gpio_num_t& chargingIO, bool& chargingIOState) { return false; }
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging);
    virtual std::string GetJson();
    virtual void SetPowerSaveMode(bool enabled) = 0;
    virtual std::string GetBoardJson() = 0;
    virtual std::string GetDeviceStatusJson() = 0;

    // 电池状态
    int bat_level_ = 0;
    bool bat_charging_ = false;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif // BOARD_H
