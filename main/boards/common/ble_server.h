#ifndef __BLE_SERVER_H__
#define __BLE_SERVER_H__

#include <string>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16 0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16 0xABF1

class BleServer {
public:
    BleServer();
    void init();
    static void hostTask(void* param);

private:
    static void onReset(int reason);
    static void onSync();
    static void onDataReceived(uint8_t* data, size_t len);
    static int gapEvent(struct ble_gap_event* event, void* arg);
    static int gattHandler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg);
    static void registerCallback(struct ble_gatt_register_ctxt* ctxt, void* arg);
    static void advertise();
    static int initServices();

    static uint8_t own_addr_type;
    static bool conn_handle_subs[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];
    static uint16_t read_val_handle;
};

#endif
