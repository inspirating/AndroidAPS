/*
================================================================================
M640GKit ESP32 泵模拟器 - Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

硬件要求:
- ESP32 开发板 (推荐 ESP32-WROOM-32)
- Arduino ESP32 核心库 (内置 BLE, 见 pump_manager/gatt_server.h)

使用方法:
1. 在 Arduino IDE 中打开此文件
2. 选择 "工具" -> "开发板" -> "ESP32 Dev Module"
3. 选择正确的串口
4. 点击上传
5. 打开串口监视器查看日志 (115200 baud)

对应 Python: main.py + boot.py
================================================================================
*/

#include "pump_simulator.h"

// 使用 M640GKit 命名空间
using namespace M640GKit;

// ESP32-C3 LED 引脚定义 (不同开发板可能不同)
// 常见: GPIO8, GPIO2, GPIO3
#ifndef LED_BUILTIN
  #define LED_BUILTIN 8
#endif

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

void setup() {
    // 初始化串口
    Serial.begin(115200);
    
    // 给串口一些时间初始化
    delay(1000);

    Serial.println("ESP32 starting...");
    Serial.println("Version: 1.0.0");
    Serial.flush();
    
    // 检查复位原因
    esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.print("Reset reason: ");
    Serial.println(reset_reason);

    Serial.println("\n========================================");
    Serial.println("  M640GKit ESP32 泵模拟器");
    Serial.println("========================================");
    Serial.println("正在初始化...");
    Serial.flush();

    // 初始化 LED (ESP32-C3 常见引脚: 8, 2, 3)
    Serial.print("Initializing LED on pin ");
    Serial.println(LED_BUILTIN);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // 初始化 GPIO1 作为 1Hz 输出
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    Serial.println("GPIO1 initialized as 1Hz output");

    // 设置全局实例指针 (用于回调)
    gSimulator = &pumpSimulator;

    // 初始化模拟器
    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("LED will blink fast when advertising, slow when connected");
    
    // 打印内存信息
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.println("========================================\n");
    Serial.flush();
}

void loop() {
    // 运行模拟器主循环
    pumpSimulator.loop();

    // LED 心跳指示器 - 快速闪烁表示正在广播
    static uint32_t lastLedToggle = 0;
    static bool ledState = false;
    uint32_t now = millis();
    
    uint32_t interval;
    if (ledState) {
        interval = 200;
    } else {
        interval = pumpSimulator.getIsConnected() ? 5000 : 200;
    }
    
    if (now - lastLedToggle >= interval) {
        lastLedToggle = now;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }
}

// 如果你的ESP32-C3 LED不亮，尝试修改 LED_BUILTIN 为以下值之一:
// #define LED_BUILTIN 2  // 某些开发板使用GPIO2
// #define LED_BUILTIN 3  // 某些开发板使用GPIO3
// #define LED_BUILTIN 8  // 某些开发板使用GPIO8 (默认)
