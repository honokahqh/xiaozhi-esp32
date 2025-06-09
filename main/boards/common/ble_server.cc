#include "ble_server.h"

#include <cJSON.h>

#include <cassert>
#include <cstring>
#include <vector>

#include "console/console.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "wifi_configuration_ble.h"

#define TAG "BLE_SERVER"

uint8_t BleServer::own_addr_type;
bool BleServer::conn_handle_subs[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];
uint16_t BleServer::read_val_handle;

BleServer::BleServer() {}

void BleServer::init() {
    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble");
        return;
    }

    for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        conn_handle_subs[i] = false;
    }

    ble_hs_cfg.reset_cb = onReset;
    ble_hs_cfg.sync_cb = onSync;
    ble_hs_cfg.gatts_register_cb = registerCallback;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = 3;
    ble_hs_cfg.sm_sc = 0;

    assert(initServices() == 0);
    assert(ble_svc_gap_device_name_set("szkj_xiaozhi") == 0);

    nimble_port_freertos_init(hostTask);
}

void BleServer::hostTask(void* param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void BleServer::onReset(int reason) { ESP_LOGW(TAG, "Resetting BLE stack; reason=%d", reason); }

void BleServer::onSync() {
    assert(ble_hs_util_ensure_addr(0) == 0);
    assert(ble_hs_id_infer_auto(0, &own_addr_type) == 0);
    advertise();
}

void BleServer::advertise() {
    ble_gap_adv_params adv_params = {};
    ble_hs_adv_fields fields = {};
    const char* name = ble_svc_gap_device_name();

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t*)name;
    fields.name_len = std::strlen(name);
    fields.name_is_complete = 1;
    ble_uuid16_t service_uuid = BLE_UUID16_INIT(BLE_SVC_SPP_UUID16);
    fields.uuids16 = &service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &adv_params, gapEvent, nullptr);
}

int BleServer::gapEvent(struct ble_gap_event* event, void* arg) {
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_LINK_ESTAB:
            /* A new connection was established or a connection attempt failed. */
            ESP_LOGI(TAG, "connection %s; status=%d ", event->connect.status == 0 ? "established" : "failed", event->connect.status);
            if (event->connect.status == 0) {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
            }
            ESP_LOGI(TAG, "\n");
            if (event->connect.status != 0 || CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1) {
                /* Connection failed or if multiple connection allowed; resume advertising. */
                advertise();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);

            conn_handle_subs[event->disconnect.conn.conn_handle] = false;

            /* Connection terminated; resume advertising. */
            advertise();
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            /* The central has updated the connection parameters. */
            ESP_LOGI(TAG, "connection updated; status=%d ", event->conn_update.status);
            rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            assert(rc == 0);
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
            advertise();
            return 0;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG,
                     "subscribe event; conn_handle=%d attr_handle=%d "
                     "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                     event->subscribe.conn_handle, event->subscribe.attr_handle, event->subscribe.reason, event->subscribe.prev_notify,
                     event->subscribe.cur_notify, event->subscribe.prev_indicate, event->subscribe.cur_indicate);
            conn_handle_subs[event->subscribe.conn_handle] = true;
            return 0;

        default:
            return 0;
    }
}

int BleServer::gattHandler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "Read request on handle: %d, attr: %d", conn_handle, attr_handle);
            for (int i = 0; i <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
                /* Check if client has subscribed to notifications */
                if (conn_handle_subs[i]) {
                    struct os_mbuf* txom;
                    txom = ble_hs_mbuf_from_flat("Honokahqh", 10);
                    int rc = ble_gatts_indicate_custom(i, read_val_handle, txom);
                    if (rc == 0) {
                        ESP_LOGI(TAG, "Notification sent successfully");
                    } else {
                        ESP_LOGI(TAG, "Error in sending notification rc = %d", rc);
                    }
                }
            }
            break;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            onDataReceived(ctxt->om->om_data, ctxt->om->om_len);
            break;
        default:
            break;
    }
    return 0;
}

void BleServer::registerCallback(struct ble_gatt_register_ctxt* ctxt, void* arg) {
    char buf[BLE_UUID_STR_LEN];
    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGI(TAG, "Registered service: %s", ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf));
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGI(TAG, "Registered characteristic: %s", ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf));
            break;
        default:
            break;
    }
}

static ble_gatt_svc_def gatt_services[2];
static ble_gatt_chr_def characteristics[2];
int BleServer::initServices() {
    static ble_uuid16_t svc_uuid = BLE_UUID16_INIT(BLE_SVC_SPP_UUID16);
    static ble_uuid16_t chr_uuid = BLE_UUID16_INIT(BLE_SVC_SPP_CHR_UUID16);

    characteristics[0].uuid = &chr_uuid.u;
    characteristics[0].access_cb = gattHandler;
    characteristics[0].val_handle = &read_val_handle;
    characteristics[0].flags = static_cast<uint8_t>(BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY);

    gatt_services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    gatt_services[0].uuid = &svc_uuid.u;
    gatt_services[0].characteristics = characteristics;

    int rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) return rc;
    return ble_gatts_add_svcs(gatt_services);
}

void BleServer::onDataReceived(uint8_t* data, size_t len) {
    std::vector<char> jsonBuffer(len + 1, 0);  // 分配 len+1 大小，并自动填充 0
    memcpy(jsonBuffer.data(), data, len);      // 拷贝 BLE 接收到的数据

    cJSON* root = cJSON_Parse(jsonBuffer.data());

    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return;
    }

    cJSON* cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cmd || !cJSON_IsObject(cmd)) {
        ESP_LOGE(TAG, "Invalid or missing 'cmd' field in JSON");
        cJSON_Delete(root);
        return;
    }
    cJSON* setWiFi = cJSON_GetObjectItem(cmd, "setWifi");
    if (setWiFi && cJSON_IsObject(setWiFi)) {
        cJSON* ssid = cJSON_GetObjectItem(setWiFi, "ssid");
        cJSON* password = cJSON_GetObjectItem(setWiFi, "pass");
        if (ssid && password && cJSON_IsString(ssid) && cJSON_IsString(password)) {
            ESP_LOGI(TAG, "Received WiFi SSID: %s, Password: %s", ssid->valuestring, password->valuestring);
            WiFiConfigurationBLE::GetInstance().StartConnection(ssid->valuestring, password->valuestring);
        } else {
            ESP_LOGE(TAG, "Invalid or missing 'ssid' or 'password' in 'setWifi'");
        }
    } else {
        ESP_LOGE(TAG, "Invalid or missing 'setWifi' field in 'cmd'");
    }

    cJSON_Delete(root);
}
