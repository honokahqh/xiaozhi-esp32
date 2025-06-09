#ifndef __WIFI_CONFIGURATION_BLE_H__
#define __WIFI_CONFIGURATION_BLE_H__

#include <esp_http_server.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <mutex>
#include <string>
#include <vector>

#include "dns_server.h"

class WiFiConfigurationBLE {
   public:
    static WiFiConfigurationBLE& GetInstance();
    void StartConnection(std::string ssid, std::string password);

   private:
    WiFiConfigurationBLE();
    ~WiFiConfigurationBLE() = default;

    WiFiConfigurationBLE(const WiFiConfigurationBLE&) = delete;
    WiFiConfigurationBLE& operator=(const WiFiConfigurationBLE&) = delete;

    bool ConnectWiFi(std::string ssid, std::string password);

    // ✅ 设为 static
    static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    bool is_connecting_ = false;
    esp_event_handler_instance_t instance_any_id_;
    esp_event_handler_instance_t instance_got_ip_;
    EventGroupHandle_t event_group_ = nullptr;
};

#endif  // __WIFI_CONFIGURATION_BLE_H__
