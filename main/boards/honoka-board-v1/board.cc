#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <wifi_station.h>

#include "audio_codecs/box_audio_codec.h"
#include "ble_server.h"
#include "board_config.h"
#include "button.h"
#include "display/honoka_display.h"
#include "esp_lcd_ili9341.h"
#include "iot/thing_manager.h"
#include "wifi_board.h"

#define TAG "HonokaV11Board"

class HonokaV11Board : public WifiBoard {
   private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button volup_button_;
    Button voldown_button_;

    HonokaDisplay* display_;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        esp_sleep_enable_ext0_wakeup(BOOT_BUTTON_GPIO, false);  // 下降沿唤醒
    }

    void InitializeST7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGI(TAG, "Install lcd_panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_FREQ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGI(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, false);
        esp_lcd_panel_mirror(panel, false, false);

        display_ = new HonokaSpiLcdDisplay(panel_io, panel, DISPLAY_HEIGHT, DISPLAY_WIDTH, 0, 0, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
        thing_manager.AddThing(iot::CreateThing("Sleep"));
    }

   public:
    HonokaV11Board() : boot_button_(BOOT_BUTTON_GPIO), volup_button_(VOLUME_UP_BUTTON_GPIO), voldown_button_(VOLUME_DOWN_BUTTON_GPIO) {
        gpio_config_t pwr_gpio_config = {0};
        pwr_gpio_config.mode = GPIO_MODE_OUTPUT;
        pwr_gpio_config.pin_bit_mask = 1ULL << AUDIO_POWER_ON_GPIO;
        ESP_ERROR_CHECK(gpio_config(&pwr_gpio_config));
        ESP_ERROR_CHECK(gpio_set_level(AUDIO_POWER_ON_GPIO, 0));
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(gpio_set_level(AUDIO_POWER_ON_GPIO, 1));
        vTaskDelay(100 / portTICK_PERIOD_MS);

        InitializeI2c();
        InitializeSpi();
        InitializeST7789Display();
        InitializeButtons();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
                                         AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR,
                                         AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryADCInfo(adc_channel_t& ch, float& ratio, gpio_num_t& chargingIO, bool& chargingIOState) override {
        ch = (adc_channel_t)ADC1_CHANNEL_0;
        ratio = BATADC_RATIO;
        chargingIO = BATADC_CHARGING_IO;
        chargingIOState = false;  // 低电平代表充电
        return true;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        level = bat_level_;
        charging = bat_charging_;
        discharging = !bat_charging_;
        return true;
    }

    virtual void EnterWifiConfigMode() override {
        BleServer server;
        server.init();
        while (1) {
            int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
            ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
};

DECLARE_BOARD(HonokaV11Board);
