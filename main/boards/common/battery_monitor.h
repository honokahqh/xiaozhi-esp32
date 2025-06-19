#ifndef __BATTERY_MONITOR_H__
#define __BATTERY_MONITOR_H__

#include <esp_timer.h>

#include "board.h"
#include "driver/adc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

class BatteryMonitor {
   public:
    static BatteryMonitor& GetInstance() {
        static BatteryMonitor instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    BatteryMonitor(const BatteryMonitor&) = delete;
    BatteryMonitor& operator=(const BatteryMonitor&) = delete;

    void Init();
    void Deinit();
    bool UpdateState();

   private:
    BatteryMonitor();
    ~BatteryMonitor();

    adc_channel_t adc_channel_;
    float adc_ratio_;
    gpio_num_t charging_io_;
    bool chargingIOState_;
    bool charging_state_;
    adc_oneshot_unit_handle_t adc_handle_;
    adc_cali_handle_t adc_cali_handle_;

    esp_timer_handle_t update_timer_ = nullptr;
};

#endif