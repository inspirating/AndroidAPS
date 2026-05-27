/*
================================================================================
GATT 服务器 (C++ 版本 - Arduino BLE 库)
================================================================================

作为 GATT Server 运行, 模拟 M640G 泵设备

对应 Python: pump_manager/gatt_server.py
================================================================================
*/

#ifndef M640G_GATT_SERVER_H
#define M640G_GATT_SERVER_H

#include <Arduino.h>
#include <string>
#include <vector>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include "enums.h"
#include "crc8.h"

namespace M640GKit {

// 回调函数类型定义
typedef void (*WriteRequestCallback)(const uint8_t* data, size_t len);
typedef void (*SubscribeCallback)(bool subscribed);
typedef void (*DisconnectCallback)();
typedef void (*ConnectCallback)();

// 前向声明，避免循环引用
class GATTServer;

class GATTServerCallbacks : public BLEServerCallbacks, public BLECharacteristicCallbacks, public BLEDescriptorCallbacks {
public:
    GATTServer* server = nullptr;

    GATTServerCallbacks() = default;

    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
    void onWrite(BLECharacteristic* pCharacteristic) override;
    void onWrite(BLEDescriptor* pDescriptor) override;
};

class GATTServer {
public:
    bool isRunning = false;
    WriteRequestCallback onWriteRequest = nullptr;
    SubscribeCallback onSubscribe = nullptr;
    DisconnectCallback onDisconnect = nullptr;
    ConnectCallback onConnect = nullptr;

    GATTServer() : server(nullptr), service(nullptr), readCharacteristic(nullptr),
                   writeCharacteristic(nullptr) {}

    void start() {
        if (isRunning) {
            Serial.println("[GATT] Server already running");
            return;
        }

        Serial.println("[GATT] Initializing BLE device...");
        // 初始化 BLE with more memory for larger MTU
        BLEDevice::init("MT");
        Serial.println("[GATT] BLE device initialized with name 'MT'");
        
        Serial.println("[GATT] Creating BLE server...");
        server = BLEDevice::createServer();
        if (!server) {
            Serial.println("[GATT] ERROR: Failed to create BLE server!");
            return;
        }
        server->setCallbacks(&callbacks);
        callbacks.server = this;
        Serial.println("[GATT] BLE server created with callbacks");

        // 创建服务
        Serial.println("[GATT] Creating BLE service...");
        service = server->createService(SERVICE_UUID);
        if (!service) {
            Serial.println("[GATT] ERROR: Failed to create BLE service!");
            return;
        }
        Serial.println("[GATT] BLE service created");

        // 创建写入特征 (可写, 可通知)
        Serial.println("[GATT] Creating write characteristic...");
        writeCharacteristic = service->createCharacteristic(
            WRITE_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_NOTIFY
        );
        if (!writeCharacteristic) {
            Serial.println("[GATT] ERROR: Failed to create write characteristic!");
            return;
        }
        BLE2902* writeDesc = new BLE2902();
        writeDesc->setCallbacks(&callbacks);
        writeCharacteristic->addDescriptor(writeDesc);
        writeCharacteristic->setCallbacks(&callbacks);
        Serial.println("[GATT] Write characteristic created");

        // 创建读取/通知特征 (可读, 可通知)
        Serial.println("[GATT] Creating read characteristic...");
        readCharacteristic = service->createCharacteristic(
            READ_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        if (!readCharacteristic) {
            Serial.println("[GATT] ERROR: Failed to create read characteristic!");
            return;
        }
        BLE2902* readDesc = new BLE2902();
        readDesc->setCallbacks(&callbacks);
        readCharacteristic->addDescriptor(readDesc);
        Serial.println("[GATT] Read characteristic created");

        // 启动服务
        Serial.println("[GATT] Starting BLE service...");
        service->start();
        Serial.println("[GATT] BLE service started");

        // 开始广播
        Serial.println("[GATT] Starting BLE advertising...");
        startAdvertising();
        Serial.println("[GATT] BLE advertising started");

        isRunning = true;
        Serial.println("[GATT] GATT Server is now running!");
    }

    void stop() {
        if (!isRunning) return;

        BLEDevice::stopAdvertising();
        if (service) {
            service->stop();
        }
        isRunning = false;
    }

    bool advertisingSuspended = false;

    void disconnectAll() {
        if (server && server->getConnectedCount() > 0) {
            for (int i = 0; i < server->getConnectedCount(); i++) {
                server->disconnect(i);
            }
        }
    }

    void startAdvertising() {
        if (advertisingSuspended) {
            Serial.println("[ADV] Advertising suspended, skipping...");
            return;
        }
        Serial.println("[ADV] Configuring BLE advertising...");
        
        BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
        
        // Configure advertising parameters for better discoverability
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMaxPreferred(0x12);

        // Match iOS `BluetoothManager.centralManager(_:didDiscover:)` parsing:
        // [0-1] company id LE, [2-5] pump SN, [6] device type, [7] version (>= 8 bytes).
        // Note: pumpSN in advertisement must match pumpSNState.reversed() in ensureConnected
        const uint8_t mfg[] = {
            0x59,
            0x6A,
            0x65,
            0xD1,
            0x79,
            0x98,
            0x01,
            0x01,
        };

        Serial.print("[ADV] Manufacturer data (");
        Serial.print(sizeof(mfg));
        Serial.print(" bytes): ");
        for (size_t i = 0; i < sizeof(mfg); i++) {
            Serial.printf("%02X ", mfg[i]);
        }
        Serial.println("");

        // Build Arduino String from binary data (all bytes are >= 0x01, so no null terminator issue)
        String mfgStr;
        mfgStr.reserve(sizeof(mfg));
        for (size_t i = 0; i < sizeof(mfg); i++) {
            mfgStr += (char)mfg[i];
        }

        // Create advertisement data
        BLEAdvertisementData advData;
        advData.setName("MT");
        advData.setManufacturerData(mfgStr);
        
        pAdvertising->setAdvertisementData(advData);

        Serial.println("[ADV] Starting BLE advertising...");
        BLEDevice::startAdvertising();
        Serial.println("[ADV] BLE advertising is now active!");
        Serial.println("[ADV] Device name: MT");
        Serial.println("[ADV] Service UUID: " + String(SERVICE_UUID));
        Serial.println("[ADV] Waiting for mobile device to connect...");
    }

    bool sendNotification(const uint8_t* data, size_t len, bool useCrcHack = true) {
        if (readCharacteristic == nullptr) return false;

        std::vector<uint8_t> payload(data, data + len);

        if (useCrcHack && len > 0 && len >= 6 && data[1] != 0x00) {
            uint8_t expectedCrc = crc8Calculate(data, len - 1);
            if (payload[len - 1] != expectedCrc) {
                payload[len - 1] = expectedCrc;
            }
        }

        readCharacteristic->setValue(payload.data(), payload.size());
        readCharacteristic->notify();
        return true;
    }

    bool sendNotificationWithCrcHack(const uint8_t* data, size_t len) {
        return sendNotification(data, len, true);
    }

    bool sendRawNotification(const uint8_t* data, size_t len) {
        if (readCharacteristic == nullptr) return false;
        readCharacteristic->setValue(data, len);
        readCharacteristic->notify();
        return true;
    }

    bool sendResponse(const uint8_t* data, size_t len) {
        if (writeCharacteristic == nullptr) return false;

        writeCharacteristic->setValue(data, len);
        writeCharacteristic->notify();
        return true;
    }

private:
    BLEServer* server;
    BLEService* service;
    BLECharacteristic* readCharacteristic;
    BLECharacteristic* writeCharacteristic;
    GATTServerCallbacks callbacks;

    friend class GATTServerCallbacks;
};

inline void GATTServerCallbacks::onConnect(BLEServer* pServer) {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[BLE] CLIENT CONNECTED!");
    Serial.print("[BLE] Connected clients: ");
    Serial.println(pServer->getConnectedCount());
    Serial.println("========================================");
    Serial.println("");
    
    if (server && server->onConnect) {
        server->onConnect();
    }
}

inline void GATTServerCallbacks::onDisconnect(BLEServer* pServer) {
    Serial.println("");
    Serial.println("========================================");
    Serial.println("[BLE] CLIENT DISCONNECTED!");
    Serial.print("[BLE] Connected clients: ");
    Serial.println(pServer->getConnectedCount());
    Serial.println("========================================");
    Serial.println("");
    
    if (server && server->onDisconnect) {
        server->onDisconnect();
    }
    if (server) {
        Serial.println("[BLE] Restarting advertising...");
        server->startAdvertising();
    }
}

inline void GATTServerCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    if (server && server->onWriteRequest) {
        String value = pCharacteristic->getValue();
        if (value.length() > 0) {
            server->onWriteRequest(reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
        }
    }
}

inline void GATTServerCallbacks::onWrite(BLEDescriptor* pDescriptor) {
    Serial.println("[BLE] Descriptor write callback triggered");
    
    // Check if this is a CCCD (Client Characteristic Configuration Descriptor)
    // CCCD UUID is 0x2902
    if (pDescriptor->getUUID().equals(BLEUUID((uint16_t)0x2902))) {
        Serial.println("[BLE] CCCD descriptor write detected");
        
        // Get the descriptor value - returns uint8_t* in this BLE library version
        uint8_t* value = pDescriptor->getValue();
        size_t len = pDescriptor->getLength();
        if (len >= 2) {
            uint16_t cccdValue = value[0] | (value[1] << 8);
            bool notifyEnabled = (cccdValue & 0x0001) != 0;
            bool indicateEnabled = (cccdValue & 0x0002) != 0;
            
            Serial.print("[BLE] CCCD value: 0x");
            Serial.print(cccdValue, HEX);
            Serial.print(" - Notify: ");
            Serial.print(notifyEnabled ? "ON" : "OFF");
            Serial.print(", Indicate: ");
            Serial.println(indicateEnabled ? "ON" : "OFF");
            
            if (server && server->onSubscribe) {
                server->onSubscribe(notifyEnabled || indicateEnabled);
            }
        }
    }
}

} // namespace M640GKit

#endif // M640G_GATT_SERVER_H
