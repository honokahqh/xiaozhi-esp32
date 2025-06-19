#include "battery_monitor.h"

#include "esp_log.h"

#define TAG "BatteryMonitor"

// ADC校准方案
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define ADC_CALI_SCHEME_SUPPORTED ADC_CALI_SCHEME_VER_LINE_FITTING
#define ADC_CALI_CREATE_SCHEME(config, handle) adc_cali_create_scheme_line_fitting(config, handle)
#define ADC_CALI_DELETE_SCHEME(handle) adc_cali_delete_scheme_line_fitting(handle)
#define ADC_CALI_CONFIG_TYPE adc_cali_line_fitting_config_t
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C5
#define ADC_CALI_SCHEME_SUPPORTED ADC_CALI_SCHEME_VER_CURVE_FITTING
#define ADC_CALI_CREATE_SCHEME(config, handle) adc_cali_create_scheme_curve_fitting(config, handle)
#define ADC_CALI_DELETE_SCHEME(handle) adc_cali_delete_scheme_curve_fitting(handle)
#define ADC_CALI_CONFIG_TYPE adc_cali_curve_fitting_config_t
#else
#error "Unsupported target"
#endif

typedef struct {
    float voltage;
    int percentage;
} battery_info_t;

#if CHARGCHIP_TP5400
// bat真实电压3.88-4.11,充电后均表现为4.11
battery_info_t battery_info[] = {
    {4.11, 100}, {4.02, 90}, {3.94, 80}, {3.88, 70}, {3.83, 60}, {3.79, 50}, {3.76, 40}, {3.73, 30}, {3.71, 20}, {3.65, 10}, {3.3, 0},
};
#else
battery_info_t battery_info[] = {
    {4.15, 100}, {4.06, 90}, {3.97, 80}, {3.90, 70}, {3.84, 60}, {3.79, 50}, {3.76, 40}, {3.73, 30}, {3.71, 20}, {3.65, 10}, {3.3, 0},
};
#endif

BatteryMonitor::BatteryMonitor() {}

BatteryMonitor::~BatteryMonitor() {}

void BatteryMonitor::Init() {
    bool res = Board::GetInstance().GetBatteryADCInfo(adc_channel_, adc_ratio_, charging_io_, chargingIOState_);
    if (res == false) {
        ESP_LOGI(TAG, "No battery ADC channel found");
        return;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

    adc_oneshot_chan_cfg_t chan_cfg = {
        chan_cfg.atten = ADC_ATTEN_DB_11,
        chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_cfg));

    // 初始化ADC校准
    ADC_CALI_CONFIG_TYPE cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = adc_channel_,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t ret = ADC_CALI_CREATE_SCHEME(&cali_config, &adc_cali_handle_);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration created successfully");
    } else {
        ESP_LOGE(TAG, "Failed to create ADC calibration: %s", esp_err_to_name(ret));
        return;
    }

    // 如果配置了充电检测IO，初始化为输入模式
    if (charging_io_ != GPIO_NUM_NC) {
        gpio_config_t io_conf;
        io_conf.pin_bit_mask = (1ULL << charging_io_);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        if (chargingIOState_) {
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // 充电时IO高电平,常态拉低
        } else {
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // 充电时IO低电平,常态拉高
        }
        gpio_config(&io_conf);
    }

    // Update display timer
    esp_timer_create_args_t update_battery_timer_args = {
        .callback =
            [](void* arg) {
                BatteryMonitor* batteryMonitor = static_cast<BatteryMonitor*>(arg);
                batteryMonitor->UpdateState();
            },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "battery_update_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_battery_timer_args, &update_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_timer_, 1000000));

    ESP_LOGI(TAG, "BatteryMonitor init success");   
}

void BatteryMonitor::Deinit() {
    if (update_timer_) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
        update_timer_ = nullptr;
    }
}

bool BatteryMonitor::UpdateState() {
    if (adc_handle_ == nullptr) {
        return false;
    }

    static float average_voltage = 0.0f;
    static bool needRefreshData = true;
    static bool isCharging = false;

    auto& board = Board::GetInstance();

    if (charging_io_ != GPIO_NUM_NC) {
        if (gpio_get_level(charging_io_) == chargingIOState_) {
            board.bat_charging_ = true;
        } else {
            board.bat_charging_ = false;
        }
    }
    if (board.bat_charging_ != isCharging) {
        isCharging = board.bat_charging_;
        needRefreshData = true;
    }

    float voltage = 0.0f;
    // 获取ADC电压并计算电池电压
    int raw = 0;
    adc_oneshot_read(adc_handle_, adc_channel_, &raw);

    int mv = 0;
    adc_cali_raw_to_voltage(adc_cali_handle_, raw, &mv);

    voltage = (float)mv / 1000.0f * adc_ratio_;
    if (needRefreshData) {
        average_voltage = voltage;
        needRefreshData = false;
    }

    average_voltage = (average_voltage * 0.95f) + (voltage * 0.05f);

    int percentage = 0;
    size_t table_size = sizeof(battery_info) / sizeof(battery_info[0]);
    for (size_t i = 0; i < table_size - 1; ++i) {
        float v_high = battery_info[i].voltage;
        float v_low = battery_info[i + 1].voltage;
        int p_high = battery_info[i].percentage;
        int p_low = battery_info[i + 1].percentage;

        if (average_voltage <= v_high && average_voltage >= v_low) {
            float ratio = (average_voltage - v_low) / (v_high - v_low);
            percentage = static_cast<int>(p_low + ratio * (p_high - p_low));
            break;
        }
    }

    // 若电压高于最高点，设为100%，低于最低点，设为0%
    if (average_voltage >= battery_info[0].voltage) {
        percentage = 100;
    } else if (average_voltage <= battery_info[table_size - 1].voltage) {
        percentage = 0;
    }

    if (board.bat_charging_) {
        // 25%并不准确,但用于充电时的动画时整体效果较好
        (percentage > 25) ? (percentage -= 25) : (percentage = 0);
    }
    board.bat_level_ = percentage;

    ESP_LOGI(TAG, "电池电压:%.2fV, 电量:%d, 充电状态:%s", average_voltage, board.bat_level_, board.bat_charging_ ? "充电中" : "未充电");
    return true;
}