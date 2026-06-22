#include "ble_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <cstring>
#include "config.h"
#include "led_status.h"
#include "stats.h"

class StatsCharCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pChar) override {
        std::string val = pChar->getValue();
        if (val.empty()) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, (const char *)val.c_str(), val.size());
        if (err) return;

        Stats tmp;
        portENTER_CRITICAL(&g_mux);
        tmp = g_stats;
        portEXIT_CRITICAL(&g_mux);

        tmp.cpu       = doc["cpu"]       | tmp.cpu;
        tmp.gpu_used  = doc["gpu_used"]  | tmp.gpu_used;
        tmp.gpu_temp  = doc["gpu_temp"]  | tmp.gpu_temp;
        tmp.ram_used  = doc["ram_used"]  | tmp.ram_used;
        tmp.ram_total = doc["ram_total"] | tmp.ram_total;
        tmp.net_up    = doc["net_up"]    | tmp.net_up;
        tmp.net_down  = doc["net_down"]  | tmp.net_down;
        tmp.valid     = true;

        if (!doc["cpu_name"].isNull()) {
            const char *s = doc["cpu_name"];
            if (s) strncpy(tmp.cpu_name, s, sizeof(tmp.cpu_name) - 1);
        }
        if (!doc["gpu_name"].isNull()) {
            const char *s = doc["gpu_name"];
            if (s) strncpy(tmp.gpu_name, s, sizeof(tmp.gpu_name) - 1);
        }
        if (!doc["wifi_name"].isNull()) {
            const char *s = doc["wifi_name"];
            if (s) strncpy(tmp.wifi_name, s, sizeof(tmp.wifi_name) - 1);
        }
        if (!doc["datetime"].isNull()) {
            const char *s = doc["datetime"];
            if (s) strncpy(tmp.datetime_str, s, sizeof(tmp.datetime_str) - 1);
        }

        portENTER_CRITICAL(&g_mux);
        g_stats    = tmp;
        g_new_data = true;
        portEXIT_CRITICAL(&g_mux);
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *) override {
        g_ble_connected = true;
        setLedConnected();
    }

    void onDisconnect(NimBLEServer *pSrv) override {
        g_ble_connected = false;
        setLedDisconnected();
        pSrv->startAdvertising();
    }
};

void initBleServer() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService *pSvc = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic *pChar = pSvc->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pChar->setCallbacks(new StatsCharCallback());
    pSvc->start();

    NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setName(DEVICE_NAME);
    pAdv->start();

    Serial.println("BLE advertising started");
}
