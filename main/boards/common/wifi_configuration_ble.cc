#include "wifi_configuration_ble.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <cstring>

#include "ssid_manager.h"

#define TAG "WiFiConfigurationBLE"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

struct WifiCreds {
    char ssid[32];
    char password[64];
};

WiFiConfigurationBLE& WiFiConfigurationBLE::GetInstance() {
    static WiFiConfigurationBLE instance;
    return instance;
}

WiFiConfigurationBLE::WiFiConfigurationBLE() {
    // 注册事件回调
    event_group_ = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, WiFiConfigurationBLE::WifiEventHandler, this, &instance_any_id_));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, WiFiConfigurationBLE::IpEventHandler, this, &instance_got_ip_));

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiConfigurationBLE::StartConnection(std::string ssid, std::string password) {
    if (is_connecting_) {
        ESP_LOGW(TAG, "Already connecting, skipping new attempt");
        return;
    }

    is_connecting_ = true;

    auto* creds = new WifiCreds();
    strncpy(creds->ssid, ssid.c_str(), sizeof(creds->ssid) - 1);
    creds->ssid[sizeof(creds->ssid) - 1] = '\0';
    strncpy(creds->password, password.c_str(), sizeof(creds->password) - 1);
    creds->password[sizeof(creds->password) - 1] = '\0';

    WiFiConfigurationBLE* self = this;

    xTaskCreate(
        [](void* arg) {
            auto* data = static_cast<std::pair<WiFiConfigurationBLE*, WifiCreds*>*>(arg);
            WiFiConfigurationBLE* self = data->first;
            WifiCreds* creds = data->second;

            if (self->ConnectWiFi(creds->ssid, creds->password)) {
                ESP_LOGI(TAG, "Connected to WiFi: %s", creds->ssid);
            } else {
                ESP_LOGE(TAG, "Failed to connect to WiFi: %s", creds->ssid);
            }

            delete creds;
            delete data;
            vTaskDelete(nullptr);
        },
        "wifi_connect", 4096, new std::pair<WiFiConfigurationBLE*, WifiCreds*>(self, creds), 5, nullptr);
}

bool WiFiConfigurationBLE::ConnectWiFi(std::string ssid, std::string password) {
    is_connecting_ = false;

    do {
        if (ssid.empty()) {
            ESP_LOGE(TAG, "SSID cannot be empty");
            break;
        }

        if (ssid.length() > 32) {  // WiFi SSID 最大长度
            ESP_LOGE(TAG, "SSID too long");
            break;
        }

        is_connecting_ = true;
        xEventGroupClearBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

        wifi_config_t wifi_config;
        bzero(&wifi_config, sizeof(wifi_config));
        strcpy((char*)wifi_config.sta.ssid, ssid.c_str());
        strcpy((char*)wifi_config.sta.password, password.c_str());
        wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifi_config.sta.failure_retry_cnt = 1;

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        auto ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to WiFi: %d", ret);
            break;
        }
        ESP_LOGI(TAG, "Connecting to WiFi %s", ssid.c_str());

        // Wait for the connection to complete for 5 seconds
        EventBits_t bits = xEventGroupWaitBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Save SSID %s %d", ssid.c_str(), ssid.length());
            SsidManager::GetInstance().AddSsid(ssid, password);
            esp_wifi_disconnect();
            is_connecting_ = false;
            return true;
        } else {
            break;
        }
    } while (0);

    is_connecting_ = false;
    return false;
}

void WiFiConfigurationBLE::WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiConfigurationBLE* self = static_cast<WiFiConfigurationBLE*>(arg);
    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(self->event_group_, WIFI_CONNECTED_BIT);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(self->event_group_, WIFI_FAIL_BIT);
    }
}

void WiFiConfigurationBLE::IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiConfigurationBLE* self = static_cast<WiFiConfigurationBLE*>(arg);
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(self->event_group_, WIFI_CONNECTED_BIT);
    }
}
