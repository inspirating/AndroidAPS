/*
================================================================================
M640GKit ESP32 泵模拟器核心 (C++ 版本)
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 GATT Server 运行,
供 iOS Loop app 或其他 BLE 客户端连接和通信

通信流程:
  1. BLE 连接 → 2. 认证(AUTH_REQ) → 3. 同步(SYNCHRONIZE) →
  4. 订阅(SUBSCRIBE) → 5. 预充(PRIME) → 6. 激活(ACTIVATE) →
  7. 正常运行(大剂量/基础率/暂停等)

对应 Python: pump_simulator.py
================================================================================
*/

#ifndef M640G_PUMP_SIMULATOR_H
#define M640G_PUMP_SIMULATOR_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
// nRF52840: 用 LittleFS(InternalFS) 包装类替代 ESP32 的 Preferences(NVS)。
// preferences_nrf52.h 提供同名同 API 的 Preferences 类, 下方 11 处调用点零改动。
#include "preferences_nrf52.h"

#include "enums.h"
#include "crc8.h"
#include "crypto.h"
#include "base_packet.h"
#include "authorize_packet.h"
#include "synchronize_packet.h"
#include "subscribe_packet.h"
#include "bolus_packet.h"
#include "basal_packet.h"
#include "pump_control_packet.h"
#include "time_packet.h"
#include "misc_packet.h"
#include "gatt_server.h"
#include "connection_tracker.h"
// nRF52840 (Adafruit nRF52): FreeRTOS 由板卡包自带。
// ESP32 用 <freertos/semphr.h>(带子目录前缀), Adafruit nRF52 的头文件布局不同——
// 它把 FreeRTOS 头平铺在 core 里, 正确入口是 <rtos.h>, 一次性引入
// FreeRTOS.h / task.h / semphr.h 等。直接 <freertos/semphr.h> 会找不到文件。
#include <rtos.h>

namespace M640GKit {

// 常量定义
static constexpr const char* PUMP_NAME = "MT";
static constexpr uint8_t PUMP_SN[4] = {0x98, 0x79, 0xD1, 0x65};
static constexpr uint8_t DEVICE_TYPE = 1;
static constexpr const char* SW_VERSION = "1.0.0";
static constexpr uint16_t MANUFACTURER_ID = 0x6A59;

static constexpr double MAX_RESERVOIR = 300.0;
static constexpr double MAX_BOLUS = 20.0;
static constexpr double MAX_BASAL_RATE = 20.0;
static constexpr double DEFAULT_HOURLY_MAX = 25.0;
static constexpr double DEFAULT_DAILY_MAX = 80.0;

static constexpr uint32_t M640G_BASE_UNIX = 1388534400; // 2014-01-01T00:00:00+0000

// 日志级别
enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

class Logger {
public:
    static LogLevel currentLevel;

    static void setLevel(LogLevel level) {
        currentLevel = level;
    }

    static void log(LogLevel level, const char* tag, const char* message) {
        if (static_cast<uint8_t>(level) >= static_cast<uint8_t>(currentLevel)) {
            const char* levelStr = (level == LogLevel::DEBUG) ? "D" :
                                   (level == LogLevel::INFO) ? "I" :
                                   (level == LogLevel::WARNING) ? "W" : "E";
            Serial.printf("[%s][%s] %s\n", tag, levelStr, message);
        }
    }

    static void hexDump(const char* tag, const char* direction, const uint8_t* data, size_t len) {
        if (static_cast<uint8_t>(LogLevel::DEBUG) >= static_cast<uint8_t>(currentLevel)) {
            Serial.printf("[%s] %s (%d bytes): ", tag, direction, len);
            for (size_t i = 0; i < len; i++) {
                Serial.printf("%02X ", data[i]);
            }
            Serial.println();
        }
    }

    static void debug(const char* msg) { log(LogLevel::DEBUG, "BLE", msg); }
    static void info(const char* msg) { log(LogLevel::INFO, "BLE", msg); }
    static void warning(const char* msg) { log(LogLevel::WARNING, "BLE", msg); }
    static void error(const char* msg) { log(LogLevel::ERROR, "BLE", msg); }

    static void debug(const String& msg) { log(LogLevel::DEBUG, "BLE", msg.c_str()); }
    static void info(const String& msg) { log(LogLevel::INFO, "BLE", msg.c_str()); }
    static void warning(const String& msg) { log(LogLevel::WARNING, "BLE", msg.c_str()); }
    static void error(const String& msg) { log(LogLevel::ERROR, "BLE", msg.c_str()); }
};

LogLevel Logger::currentLevel = LogLevel::DEBUG;

// 模拟器状态
enum class SimulatorState : uint8_t {
    INITIALIZING = 0,
    READY,
    RUNNING,
    SUSPENDED,
    EJECTING,
    ERROR
};

// 大剂量数据结构
struct BolusInfo {
    uint8_t type;
    double amount;
    double delivered;
    uint32_t startTime;
};

// 临时基础率数据结构
struct TempBasalInfo {
    uint8_t type;
    double rate;
    uint16_t durationMinutes;
    uint32_t startTime;
};

// ========== 队列输注系统 ==========
static constexpr double STEP_SIZE = 0.5;
static constexpr double CARRYOVER_MAX = 2.0;  // carryOver 硬上限: 防止撞上限时欠债无限累积, 配额恢复时一次性补打成过量
// Nice!Nano: STEP_PIN 用 P0.17 = D2 排针引脚 (Adafruit nRF52 引脚号 17)。
// Nice!Nano 引脚映射: P0.x = x; P1.x = 32 + x。P1.02 = 34, 空闲数字 GPIO, 无硬件冲突。
// PCA10056 DK 原值为 33 (P1.01), ESP32 版原值为 7 (GPIO7)。
static constexpr int STEP_PIN = 29;

struct InsulinAction {
    uint32_t executeTimeMs;
    double stepAmount;
};

// 前向声明: gpioDeliveryTask 在文件末尾定义
void gpioDeliveryTask(void *parameter);

void safeDelay(int ms) {
    // long start = millis();
    // while (millis() - start < ms) {
    //     // 【关键】如果你的模拟器有 update() 或处理通信的方法，必须在这里调用
    //     // 比如：simulator.update(); 或者 BLEDevice::handle();
    //     // 这样可以防止 BLE 协议栈崩溃导致泵页面被强制撤销
    //     vTaskDelay(pdMS_TO_TICKS(100)); 
    // }

    vTaskDelay(pdMS_TO_TICKS(ms)); 
}


class M640GPumpSimulator {
public:
    friend void gpioDeliveryTask(void *parameter);

    M640GPumpSimulator() : initialized(false), lastUpdateTime(0), updateIntervalMs(200),
        patchState(PatchState::FILLED), simulatorState(SimulatorState::INITIALIZING),
        reservoir(MAX_RESERVOIR), activeInsulin(0.0), batteryVoltage(3.8), batteryLevel(100),
        patchStartTime(0), totalElapsedTime(0), currentBolus(nullptr),
        bolusDeliveryProgress(0), lastReportedBolusProgress(0), lastBolusProgressReportTime(0), primeProgress(0), tempBasal(nullptr), tempBasalRemaining(0),
        isConnected(false), isSubscribed(false), isAuthenticated(false), sessionToken{0}, pumpTimezone(0),
        timeSyncPending(false), sequenceNumber(0), pingCounter(0), lastPingTime(0),
        connectionTimeoutMs(15000), lastActivityTime(0), patchId(0),
        lastNotifiedState(PatchState::FILLED), patchStateDirty(false), pendingBleDisconnect(false), nvsClearPending(false), advertisingResumeTime(0),
        hourlyMaxInsulin(DEFAULT_HOURLY_MAX), dailyMaxInsulin(DEFAULT_DAILY_MAX),
        hourlyDelivered(0.0), dailyDelivered(0.0),
        currentBasalRate(0.6), basalSequence(0),
        expirationTimer(0), alarmSetting(0),
        predictiveLowSuspend(0), predictiveLowSuspendRange(30), lastPrimeNotificationTime(0),
        basalQueueIdx(0), tempBasalQueueIdx(0), bolusQueueIdx(0),
        tempBasalActive(false), basalSuspended(false), suspendResumeTimeMs(0), stepCarryOver(0.0), tempBasalStartMs(0), lastDeliveryScanTime(0), everActivated(false), lastHourResetSec(0), lastDayResetSec(0),
        gpioDeliveryStartMs(0), gpioRemainingSteps(0), gpioCurrentStep(0), isDeliveryTaskRunning(false), xSemaphore(nullptr), pendingSubscribeNotify(false) {
        currentBolus = nullptr;
        tempBasal = nullptr;
    }

    bool getIsConnected() const { return isConnected; }

    void setup() {
        Serial.println("[PUMP] Starting setup...");
        
        Logger::info("========================================");
        Logger::info("  M640G pump simulator initlize");
        Logger::info("========================================");

        // 初始化随机数
        randomSeed(millis());

        // 创建信号量
        xSemaphore = xSemaphoreCreateMutex();
        if (xSemaphore != nullptr) {
            xSemaphoreGive(xSemaphore);
        }

        // 生成会话令牌
        generateSessionToken();

        // 设置初始时间 (M640G时间: 从2014-01-01开始的秒数)
        Preferences prefs;
        prefs.begin("pump", true);
        uint32_t savedStartTime = prefs.getUInt("patchStart", 0);
        uint8_t savedPatchState = prefs.getUChar("patchState", 0);
        uint32_t savedElapsedTime = prefs.getUInt("elapsedTime", 0);
        prefs.end();

        if (savedStartTime > 0) {
            patchStartTime = savedStartTime;
            totalElapsedTime = savedElapsedTime;
            if (savedPatchState > 0) {
                patchState = static_cast<PatchState>(savedPatchState);
            } else {
                patchState = PatchState::ACTIVE;
            }
            if (patchState == PatchState::ACTIVE || patchState == PatchState::ACTIVE_ALT) {
                simulatorState = SimulatorState::RUNNING;
            } else if (patchState == PatchState::SUSPENDED || patchState == PatchState::PAUSED) {
                simulatorState = SimulatorState::SUSPENDED;
            }

            // 恢复输注统计数据
            Preferences statsPrefs;
            statsPrefs.begin("pumpStats", true);
            hourlyDelivered = statsPrefs.getDouble("hourlyDel", 0.0);
            dailyDelivered = statsPrefs.getDouble("dailyDel", 0.0);
            reservoir = statsPrefs.getDouble("reservoir", MAX_RESERVOIR);
            stepCarryOver = statsPrefs.getDouble("stepCarry", 0.0);
            // 防御: NVS 里的旧值 (可能来自更早版本固件四舍五入产生的负值, 或异常累积) 统一封顶到 [0, CARRYOVER_MAX]
            if (stepCarryOver < 0) stepCarryOver = 0;
            if (stepCarryOver > CARRYOVER_MAX) stepCarryOver = CARRYOVER_MAX;
            uint32_t savedLastHourReset = statsPrefs.getUInt("lastHourRst", 0);
            uint32_t savedLastDayReset = statsPrefs.getUInt("lastDayRst", 0);
            statsPrefs.end();

            // 恢复 resetHourlyDailyCounters 的计时基准
            // 如果重启间隔超过1小时/1天, 计数器应归零
            uint32_t nowSec = millis() / 1000;
            if (savedLastHourReset > 0 && nowSec - savedLastHourReset < 3600) {
                // 在同一小时内, 恢复计数和计时基准
                lastHourResetSec = savedLastHourReset;
            } else {
                hourlyDelivered = 0.0;
                lastHourResetSec = nowSec;
            }
            if (savedLastDayReset > 0 && nowSec - savedLastDayReset < 86400) {
                // 在同一天内, 恢复计数和计时基准
                lastDayResetSec = savedLastDayReset;
            } else {
                dailyDelivered = 0.0;
                lastDayResetSec = nowSec;
            }

            Logger::info("从NVS恢复: patchStartTime=" + String(patchStartTime) + " state=" + String(getStateName(patchState)) + " elapsed=" + String(totalElapsedTime));
            Logger::info("统计恢复: hourlyDel=" + String(hourlyDelivered) + " dailyDel=" + String(dailyDelivered) + " reservoir=" + String(reservoir) + " carryOver=" + String(stepCarryOver));
        } else {
            patchStartTime = 0;
            // 检查patch是否曾经被激活过（独立持久化标记，不会被handleStopPatchRequest清除）
            Preferences metaPrefs;
            metaPrefs.begin("pumpMeta", true);
            uint8_t wasActivated = metaPrefs.getUChar("activated", 0);
            metaPrefs.end();
            if (wasActivated) {
                patchState = PatchState::EXPIRED;
                everActivated = true;
                Logger::info("检测到patch曾激活过, 设置状态为EXPIRED, 避免iOS跳转到Serial Number页面");
            } else {
                Logger::info("首次启动: 等待激活流程");
            }
        }
        patchId = random(65535) + 1;

        // 创建默认基础率配置文件
        createDefaultBasalProfile();

        // 初始化 GATT Server
        Serial.println("[PUMP] Setting up GATT server callbacks...");
        gattServer.onWriteRequest = handleWriteRequestStatic;
        gattServer.onConnect = handleConnectStatic;
        gattServer.onDisconnect = handleDisconnectStatic;
        gattServer.onSubscribe = handleSubscribeStatic;

        Serial.println("[PUMP] Starting GATT server...");
        gattServer.start();
        Serial.println("[PUMP] GATT server started");
        
        simulatorState = SimulatorState::READY;
        initialized = true;
        lastUpdateTime = millis();

        Logger::info("device name: MT");
        Logger::info("serial number: 9879D165");
        Logger::info("device type: 1");
        Logger::info("software version: 1.0.0");
        Logger::info("initial state: " + String(getStateName(patchState)));
        Logger::info("Patch ID: " + String(patchId));
        Logger::info("========================================");
        
        Serial.println("[PUMP] Setup complete!");
    }

    void loop() {
        if (!initialized) {
            Logger::error("pump simulator not initailized yet, please call setup() first");
            return;
        }
        // 不再用 isGpioDeliveryComplete() 门控 update():
        // 原逻辑会导致 prime 进度 (updatePrimeProgress) 在 GPIO 输注期间完全停滞。
        // processDeliveryQueues 内部已有 isDeliveryTaskRunning 检查, 不会重复触发 GPIO。
        uint32_t currentTime = millis();
        if (currentTime - lastUpdateTime >= updateIntervalMs) {
            lastUpdateTime = currentTime;
            update();
        }
    }

    // void loop() {
    //     // 【终极测试锁】
    //     // 如果大剂量线程正在运行，让 loop 陷入绝对死循环，什么都不执行，连 delay 都不给！
    //     while (isDeliveryTaskRunning) {
    //         // 纯卡死，不让出任何 CPU 给 loop 后面的 simulator.update() 
    //         // 注：为了防止 ESP32 的任务喂狗报错，这里只用最原始的底层微秒延迟
    //         esp_rom_delay_us(1000); 
    //     }

    //     // 正常情况下的模拟器刷新
    //     uint32_t currentTime = millis();
    //     if (currentTime - lastUpdateTime >= updateIntervalMs) {
    //         lastUpdateTime = currentTime;
    //         update();
    //     }
    // }
    // void loop() {
    //     int steps = 10;

    //     safeDelay(5000);

    //     // 1. 唤醒屏幕
    //     digitalWrite(STEP_PIN, LOW);
    //     safeDelay(200);
    //     digitalWrite(STEP_PIN, HIGH);
    //     safeDelay(1000);

    //     // 2. 触发进入模式
    //     digitalWrite(STEP_PIN, LOW);
    //     safeDelay(2000);
    //     digitalWrite(STEP_PIN, HIGH);
    //     safeDelay(2000);

    //     // 3. 循环输入步数
    //     for (int i = 1; i <= steps; i++) {
    //         digitalWrite(STEP_PIN, LOW);
    //         safeDelay(400);
    //         digitalWrite(STEP_PIN, HIGH);
    //         safeDelay(600);
    //     }

    //     safeDelay(3000);

    //     // 4. 第一次长按确认
    //     digitalWrite(STEP_PIN, LOW);
    //     safeDelay(4000);
    //     digitalWrite(STEP_PIN, HIGH);
    //     safeDelay(steps * 3000);

    //     // 5. 第二次长按执行
    //     digitalWrite(STEP_PIN, LOW);
    //     safeDelay(2000);
    //     digitalWrite(STEP_PIN, HIGH);

    //     // 死循环卡住，任务完成就不要再发了
    //     while(1) {
    //         delay(1000);
    //     }
    // }

    // // 极性假设：如果这样不行，把所有的 LOW 换成 HIGH，HIGH 换成 LOW 再试一次！
    // void loop() {
    //     delay(5000); // 每 5 秒尝试一次，给你时间观察

    //     Serial.println(">>> 1. 短按唤醒屏幕...");
    //     digitalWrite(STEP_PIN, LOW); // 按下
    //     delay(200);                  
    //     digitalWrite(STEP_PIN, HIGH); // 松开
        
    //     delay(1000); // 等待屏幕完全亮起，给泵一点反应时间

    //     Serial.println(">>> 2. 长按尝试触发声响大剂量...");
    //     digitalWrite(STEP_PIN, LOW); // 按下
    //     delay(2000);                 // 狠按 2 秒！保证触发
    //     digitalWrite(STEP_PIN, HIGH); // 松开
        
    //     Serial.println(">>> 触发结束，观察泵是否进入了大剂量页面？");
        
    //     // 死循环卡住，防止重复发干扰观察
    //     while(1) {
    //         delay(1000);
    //     }
    // }

    // // 假设 STEP_PIN 已经在 setup() 里 pinMode(STEP_PIN, OUTPUT) 并且 digitalWrite(STEP_PIN, HIGH) 了
    // void loop() {
    //     // 给你 5 秒钟准备观察
    //     delay(5000); 
    //     Serial.println("====== 开始模拟完整声响大剂量输注 ======");

    //     // 第一步：唤醒屏幕 (短按)
    //     Serial.println(">>> 1. 唤醒屏幕...");
    //     digitalWrite(STEP_PIN, LOW);
    //     delay(200);
    //     digitalWrite(STEP_PIN, HIGH);
    //     delay(1000); // 必须等屏幕完全亮起

    //     // 第二步：触发进入声响大剂量模式 (长按)
    //     Serial.println(">>> 2. 触发进入声响大剂量...");
    //     digitalWrite(STEP_PIN, LOW);
    //     delay(2000); // 长按 2 秒
    //     digitalWrite(STEP_PIN, HIGH);
    //     delay(2000); // 等待泵“滴”一声，并且界面完全切换过去

    //     // 第三步：连续输入步数（假设输入 3 步）
    //     int steps = 10;
    //     Serial.println(">>> 3. 开始输入步数: " + String(steps) + " 步");
    //     for (int i = 1; i <= steps; i++) {
    //         Serial.println("    -> 按下第 " + String(i) + " 步");
    //         digitalWrite(STEP_PIN, LOW);
    //         delay(400);  // 短按 0.4 秒
    //         digitalWrite(STEP_PIN, HIGH);
    //         delay(600);  // 抬起 0.6 秒（留足时间给泵发声反馈，这一步极易翻车）
    //     }
    //     delay(3000); // 输入完毕后，等一下再确认

    //     // 第四步：第一次长按确认（泵会回放步数声音）
    //     Serial.println(">>> 4. 确认输入的步数...");
    //     digitalWrite(STEP_PIN, LOW);
    //     delay(4000);
    //     digitalWrite(STEP_PIN, HIGH);
    //     delay(3000*8); // 【关键】泵会“滴滴滴”把刚才的 3 步响一遍给你听，必须等它响完！

    //     // 第五步：第二次长按确认（真正开始推药）
    //     Serial.println(">>> 5. 最终确认执行输注...");
    //     digitalWrite(STEP_PIN, LOW);
    //     delay(2000);
    //     digitalWrite(STEP_PIN, HIGH);

    //     Serial.println("====== 指令发送完毕，观察泵是否开始推推杆 ======");

    //     // 死循环卡住，任务完成就不要再发了
    //     while(1) {
    //         delay(1000);
    //     }
    // }
// 5000 wait
// 200 1000 第一步：唤醒屏幕 (短按)
// 2000 2000 第二步：触发进入声响大剂量模式 (长按)
// 400 600 loop
// 3000 输入完毕后，等一下再确认
// 4000 3000*steps 第四步：第一次长按确认（泵会回放步数声音）
// 2000 第五步：第二次长按确认（真正开始推药）

    // void loop() {
    //     // delay(5000); 
    //     vTaskDelay(pdMS_TO_TICKS(5000));

    //     int steps = 10;
        
    //     // 1. 唤醒屏幕
    //     digitalWrite(STEP_PIN, LOW);
    //     vTaskDelay(pdMS_TO_TICKS(200));   // FreeRTOS 标准的 delay 写法，不会阻塞主线程
    //     digitalWrite(STEP_PIN, HIGH);
    //     vTaskDelay(pdMS_TO_TICKS(1000)); 

    //     // 2. 触发进入模式
    //     digitalWrite(STEP_PIN, LOW);
    //     vTaskDelay(pdMS_TO_TICKS(2000)); 
    //     digitalWrite(STEP_PIN, HIGH);
    //     vTaskDelay(pdMS_TO_TICKS(2000)); 

    //     // 3. 循环输入步数
    //     for (int i = 1; i <= steps; i++) {
    //         digitalWrite(STEP_PIN, LOW);
    //         vTaskDelay(pdMS_TO_TICKS(400));
    //         digitalWrite(STEP_PIN, HIGH);
    //         vTaskDelay(pdMS_TO_TICKS(600)); // 这里的时序你自己测出完美的数值填进来
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(3000)); 

    //     // 4. 第一次长按确认
    //     digitalWrite(STEP_PIN, LOW);
    //     vTaskDelay(pdMS_TO_TICKS(4000));
    //     digitalWrite(STEP_PIN, HIGH);
    //     // 根据步数动态等待泵响完（每步大概1秒）
    //     vTaskDelay(pdMS_TO_TICKS(steps * 3000)); 

    //     // 5. 第二次长按执行
    //     digitalWrite(STEP_PIN, LOW);
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    //     digitalWrite(STEP_PIN, HIGH);

    //     Logger::info("[Task] 大剂量物理输入完毕！");

        
    //     // 线程执行完毕，必须自行销毁！
    //     // vTaskDelete(NULL);

    //     while(1) {
    //         delay(1000);
    //     }
    // }

    // bool hasExecuted = false;

    // void loop() {
    //     if (hasExecuted) {
    //         delay(1); // 必须留 delay(1) 或 yield()，防止触发看门狗
    //         return; 
    //     }

    //     // 刚开机，先静静等待 5 秒钟，让 ESP32 蓝牙广播稳定，也给泵一个稳定的供电初始电位
    //     delay(5000); 

    //     Serial.println("=========================================");
    //     Serial.println(">>> 触发测试：开始执行单次大剂量输入流程...");
    //     Serial.println("=========================================");

    //     // 1. 短按唤醒 / 进入声音大剂量界面
    //     digitalWrite(STEP_PIN, HIGH); 
    //     delay(140);                   
    //     digitalWrite(STEP_PIN, LOW);  
        
    //     Serial.println("步骤 1 完成：已发送唤醒脉冲，等待界面加载...");
    //     delay(3000); // 延长到 3 秒，确保泵的菜单完全加载完毕出来

    //     // 2. 长按触发输入模式 (必须大于 2 秒)
    //     digitalWrite(STEP_PIN, HIGH); 
    //     delay(2200);                  
    //     digitalWrite(STEP_PIN, LOW);  
        
    //     Serial.println("步骤 2 完成：已发送长按触发，等待音频就绪...");
    //     delay(2000); // 给泵 2 秒钟准备

    //     // 3. 循环输入步数 (放慢速度，确保消抖通过)
    //     Serial.println("步骤 3：开始按 200ms/250ms 的安全速度敲击 5 步...");
    //     for (int i = 1; i <= 5; i++) {
    //         digitalWrite(STEP_PIN, HIGH); 
    //         delay(200); // 稳稳按下 200ms
    //         digitalWrite(STEP_PIN, LOW);  
    //         delay(250); // 稳稳松开 250ms
    //     }
    //     delay(2000); 

    //     // 4. 第一次长按确认
    //     Serial.println("步骤 4：长按确认步数...");
    //     digitalWrite(STEP_PIN, HIGH); 
    //     delay(2500);                  
    //     digitalWrite(STEP_PIN, LOW);  
        
    //     // 释放后，死等泵把“滴滴滴”的声音播完（5步 * 500ms + 缓冲）
    //     delay(5 * 500 + 1000); 

    //     // 5. 第二次长按最终执行输注
    //     Serial.println("步骤 5：最终长按执行...");
    //     digitalWrite(STEP_PIN, HIGH); 
    //     delay(2500);                  
    //     digitalWrite(STEP_PIN, LOW);  

    //     Serial.println("=========================================");
    //     Serial.println(">>> [SUCCESS] 单次测试流程完全结束，锁定 loop！");
    //     Serial.println("=========================================");

    //     // 【核心修复 3】：锁死状态，防止进入下一次 loop 循环
    //     hasExecuted = true;
    // }

private:
    bool initialized;
    uint32_t lastUpdateTime;
    uint16_t updateIntervalMs;

    // 泵状态
    PatchState patchState;
    SimulatorState simulatorState;

    // 储药器和胰岛素
    double reservoir;
    double activeInsulin;

    // 电池
    double batteryVoltage;
    uint8_t batteryLevel;

    // 时间
    uint32_t patchStartTime;
    uint32_t totalElapsedTime;
    uint32_t patchId;

    // 大剂量
    BolusInfo* currentBolus;
    uint8_t bolusDeliveryProgress;
    uint8_t lastReportedBolusProgress;
    uint32_t lastBolusProgressReportTime;
    std::vector<BolusInfo> bolusHistory;

    uint8_t primeProgress;

    // 基础率
    std::vector<uint8_t> basalProfile;
    TempBasalInfo* tempBasal;
    double tempBasalRemaining;

    // 操作记录
    struct PumpRecord {
        uint8_t type;      // 1=bolus, 2=tempBasal, 3=suspend, 4=resume
        uint32_t timestamp;
        double amount;
        uint16_t duration;
    };
    std::vector<PumpRecord> recordBuffer;
    static constexpr size_t MAX_RECORDS = 50;

    // 连接状态
    bool isConnected;
    bool isSubscribed;
    bool isAuthenticated;
    std::vector<uint32_t> authenticatedClients;

    // 会话
    uint8_t sessionToken[4];
    int16_t pumpTimezone;
    bool timeSyncPending;

    // GATT Server
    GATTServer gattServer;
    ConnectionTracker connectionTracker;

    // 序列号和计时器
    uint8_t sequenceNumber;
    uint32_t pingCounter;
    uint32_t lastPingTime;
    uint16_t connectionTimeoutMs;
    uint32_t lastActivityTime;

    // 数据包缓冲
    std::vector<uint8_t> packetBuffer;
    uint8_t expectedPacketLen;
    uint8_t currentCmdType;
    uint8_t currentSeqNum;
    uint8_t currentPkgIndex;

    PatchState lastNotifiedState;
    bool patchStateDirty;
    bool pendingBleDisconnect;
    bool nvsClearPending;
    uint32_t advertisingResumeTime;
    uint32_t lastPrimeNotificationTime;

    double hourlyMaxInsulin;
    double dailyMaxInsulin;
    double hourlyDelivered;
    double dailyDelivered;
    double currentBasalRate;
    uint16_t basalSequence;
    uint8_t expirationTimer;
    uint8_t alarmSetting;
    uint8_t predictiveLowSuspend;
    uint8_t predictiveLowSuspendRange;

    // ========== 队列输注系统 ==========
    std::vector<InsulinAction> basalQueue;
    std::vector<InsulinAction> tempBasalQueue;
    std::vector<InsulinAction> bolusQueue;
    size_t basalQueueIdx;
    size_t tempBasalQueueIdx;
    size_t bolusQueueIdx;
    bool tempBasalActive;
    bool basalSuspended;
    uint32_t suspendResumeTimeMs;  // 0=不自动恢复, >0=自动恢复的绝对时间
    double stepCarryOver;  // 步进补偿余量, 独立跟踪, 不放入 bolusQueue
    uint32_t tempBasalStartMs;
    uint32_t lastDeliveryScanTime;
    bool everActivated;
    uint32_t lastHourResetSec;  // 上次小时计数器重置时间(秒), 持久化到NVS
    uint32_t lastDayResetSec;   // 上次日计数器重置时间(秒), 持久化到NVS

    uint32_t gpioDeliveryStartMs;   // 整次输注开始的绝对时间, 用于超时保护
    int gpioRemainingSteps;
    int gpioCurrentStep;
    int gpioTotalSteps;
    volatile bool isDeliveryTaskRunning;
    SemaphoreHandle_t xSemaphore;
    volatile bool pendingSubscribeNotify;  // 在BLE中断中设置, 主循环中消费

    const char* getStateName(PatchState s) {
        switch (s) {
            case PatchState::NONE: return "NONE";
            case PatchState::IDLE: return "IDLE";
            case PatchState::FILLED: return "FILLED";
            case PatchState::PRIMING: return "PRIMING";
            case PatchState::PRIMED: return "PRIMED";
            case PatchState::EJECTING: return "EJECTING";
            case PatchState::EJECTED: return "EJECTED";
            case PatchState::ACTIVE: return "ACTIVE";
            case PatchState::ACTIVE_ALT: return "ACTIVE_ALT";
            case PatchState::LOW_BG_SUSPENDED: return "LOW_BG_SUSPENDED";
            case PatchState::LOW_BG_SUSPENDED2: return "LOW_BG_SUSPENDED2";
            case PatchState::AUTO_SUSPENDED: return "AUTO_SUSPENDED";
            case PatchState::HOURLY_MAX_SUSPENDED: return "HOURLY_MAX_SUSPENDED";
            case PatchState::DAILY_MAX_SUSPENDED: return "DAILY_MAX_SUSPENDED";
            case PatchState::SUSPENDED: return "SUSPENDED";
            case PatchState::PAUSED: return "PAUSED";
            case PatchState::OCCLUSION: return "OCCLUSION";
            case PatchState::EXPIRED: return "EXPIRED";
            case PatchState::RESERVOIR_EMPTY: return "RESERVOIR_EMPTY";
            case PatchState::PATCH_FAULT: return "PATCH_FAULT";
            case PatchState::PATCH_FAULT2: return "PATCH_FAULT2";
            case PatchState::BASE_FAULT: return "BASE_FAULT";
            case PatchState::BATTERY_OUT: return "BATTERY_OUT";
            case PatchState::NO_CALIBRATION: return "NO_CALIBRATION";
            case PatchState::STOPPED: return "STOPPED";
            default: return "UNKNOWN";
        }
    }

    const char* getCommandName(uint8_t cmd) {
        switch (static_cast<CommandType>(cmd)) {
            case CommandType::SYNCHRONIZE: return "SYNCHRONIZE";
            case CommandType::SUBSCRIBE: return "SUBSCRIBE";
            case CommandType::AUTH_REQ: return "AUTH_REQ";
            case CommandType::GET_DEVICE_TYPE: return "GET_DEVICE_TYPE";
            case CommandType::SET_TIME: return "SET_TIME";
            case CommandType::GET_TIME: return "GET_TIME";
            case CommandType::SET_TIME_ZONE: return "SET_TIME_ZONE";
            case CommandType::PRIME: return "PRIME";
            case CommandType::ACTIVATE: return "ACTIVATE";
            case CommandType::SET_BOLUS: return "SET_BOLUS";
            case CommandType::CANCEL_BOLUS: return "CANCEL_BOLUS";
            case CommandType::SET_BASAL_PROFILE: return "SET_BASAL_PROFILE";
            case CommandType::SET_TEMP_BASAL: return "SET_TEMP_BASAL";
            case CommandType::CANCEL_TEMP_BASAL: return "CANCEL_TEMP_BASAL";
            case CommandType::SUSPEND_PUMP: return "SUSPEND_PUMP";
            case CommandType::RESUME_PUMP: return "RESUME_PUMP";
            case CommandType::POLL_PATCH: return "POLL_PATCH";
            case CommandType::STOP_PATCH: return "STOP_PATCH";
            case CommandType::READ_BOLUS_STATE: return "READ_BOLUS_STATE";
            case CommandType::SET_PATCH: return "SET_PATCH";
            case CommandType::SET_BOLUS_MOTOR: return "SET_BOLUS_MOTOR";
            case CommandType::GET_RECORD: return "GET_RECORD";
            case CommandType::CLEAR_ALARM: return "CLEAR_ALARM";
            default: return "UNKNOWN";
        }
    }

    void generateSessionToken() {
        for (int i = 0; i < 4; i++) {
            sessionToken[i] = random(256);
        }
    }

    void createDefaultBasalProfile() {
        double defaultRates[48] = {
            0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6,
            0.6, 0.6, 0.6, 0.6, 0.7, 0.7, 0.8, 0.9,
            1.0, 1.0, 0.9, 0.8, 0.8, 0.8, 0.8, 0.7,
            0.7, 0.7, 0.8, 0.9, 1.0, 0.9, 0.8, 0.7,
            0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6,
            0.6, 0.6, 0.6, 0.6, 0.7, 0.7, 0.8, 0.9
        };
        basalProfile.clear();
        uint8_t entryCount = 0;
        uint8_t tempEntries[48][3];
        double lastRate = defaultRates[0];
        uint16_t entryStart = 0;
        for (int seg = 1; seg < 48; seg++) {
            if (defaultRates[seg] != lastRate) {
                uint16_t rawRate = static_cast<uint16_t>(round(lastRate / 0.05));
                uint32_t raw24 = ((uint32_t)rawRate << 12) | (uint32_t)entryStart;
                tempEntries[entryCount][0] = raw24 & 0xFF;
                tempEntries[entryCount][1] = (raw24 >> 8) & 0xFF;
                tempEntries[entryCount][2] = (raw24 >> 16) & 0xFF;
                entryCount++;
                lastRate = defaultRates[seg];
                entryStart = seg * 30;
            }
        }
        uint16_t rawRate = static_cast<uint16_t>(round(lastRate / 0.05));
        uint32_t raw24 = ((uint32_t)rawRate << 12) | (uint32_t)entryStart;
        tempEntries[entryCount][0] = raw24 & 0xFF;
        tempEntries[entryCount][1] = (raw24 >> 8) & 0xFF;
        tempEntries[entryCount][2] = (raw24 >> 16) & 0xFF;
        entryCount++;
        basalProfile.push_back(entryCount);
        for (uint8_t i = 0; i < entryCount; i++) {
            basalProfile.push_back(tempEntries[i][0]);
            basalProfile.push_back(tempEntries[i][1]);
            basalProfile.push_back(tempEntries[i][2]);
        }
    }

    void update() {
        static uint32_t updateCallCount = 0;
        updateCallCount++;
        // Serial.print("[UPDATE] called #");
        // Serial.println(updateCallCount);

        static uint32_t elapsedAccumulator = 0;
        elapsedAccumulator += updateIntervalMs;
        if (elapsedAccumulator >= 1000) {
            totalElapsedTime += elapsedAccumulator / 1000;
            elapsedAccumulator %= 1000;
        }

        if (patchStateDirty) {
            patchStateDirty = false;
            Preferences statePrefs;
            statePrefs.begin("pump", false);
            statePrefs.putUChar("patchState", static_cast<uint8_t>(patchState));
            statePrefs.putUInt("patchStart", patchStartTime);
            statePrefs.putUInt("elapsedTime", totalElapsedTime);
            statePrefs.end();
        }

        if (pendingBleDisconnect) {
            pendingBleDisconnect = false;
            Logger::info("执行延迟BLE断开并重置为初始状态");
            if (everActivated) {
                patchState = PatchState::EXPIRED;
                Logger::info("patch曾激活过, 设置状态为EXPIRED而非FILLED");
            } else {
                patchState = PatchState::FILLED;
            }
            simulatorState = SimulatorState::INITIALIZING;
            gattServer.advertisingSuspended = true;
            // nRF52840: 通过 GATTServer 抽象层停广播 (ESP32 版直接调 BLEDevice::stopAdvertising)。
            gattServer.stopAdvertising();
            gattServer.disconnectAll();
            advertisingResumeTime = 0;
            Logger::info("BLE广播已永久停止, 需重启设备才能再次连接");
        }

        if (gattServer.advertisingSuspended && advertisingResumeTime > 0 && millis() > advertisingResumeTime) {
            advertisingResumeTime = 0;
            gattServer.advertisingSuspended = false;
            gattServer.startAdvertising();
            Logger::info("BLE广播已恢复, 设备可被发现");
        }

        if (nvsClearPending) {
            nvsClearPending = false;
            Preferences clearPrefs;
            clearPrefs.begin("pump", false);
            clearPrefs.clear();
            clearPrefs.end();
            Logger::info("NVS 持久化状态已清除");
        }

        static uint32_t lastElapsedTimeSave = 0;
        if (totalElapsedTime - lastElapsedTimeSave >= 60) {
            lastElapsedTimeSave = totalElapsedTime;
            Preferences elapsedPrefs;
            elapsedPrefs.begin("pump", false);
            elapsedPrefs.putUInt("elapsedTime", totalElapsedTime);
            elapsedPrefs.end();
        }

        // 自动恢复: suspend 到期后自动恢复
        if (basalSuspended && suspendResumeTimeMs > 0 && millis() >= suspendResumeTimeMs) {
            suspendResumeTimeMs = 0;
            basalSuspended = false;
            setPatchState(PatchState::ACTIVE);
            simulatorState = SimulatorState::RUNNING;
            addRecord(4, 0, 0);
            buildBasalQueue();
            Logger::info("暂停到期, 泵已自动恢复 -> ACTIVE");
        }

        processDeliveryQueues();
        updatePrimeProgress();
        resetHourlyDailyCounters();

        // 检查订阅后初始状态通知标志 (在 BLE 中断中设置, 避免 vector 堆分配在中断上下文)
        if (pendingSubscribeNotify) {
            pendingSubscribeNotify = false;
            // 不再主动发送状态通知 — Trio 在 SYNCHRONIZE 后已经知道状态,
            // 额外的通知会打乱 PRIME/ACTIVATE 流程。
        }

        // 检查状态变化, 在 update() 中发送通知 (避免在 handle 中同步发送导致 HVN 队列竞争)
        if (patchState != lastNotifiedState) {
            sendStateNotification();
            lastNotifiedState = patchState;
        }

        sendPingHeartbeat();
        checkConnectionTimeout();

        if (random(1000) == 0) {
            batteryVoltage = max(2.8, batteryVoltage - 0.001);
            batteryLevel = max(0, batteryLevel - 1);
        }

        if (isSubscribed && isConnected) {
            sendPeriodicNotification();
            sendPrimeProgressNotification();
        }
    }

    void addRecord(uint8_t type, double amount, uint16_t duration) {
        if (recordBuffer.size() >= MAX_RECORDS) {
            recordBuffer.erase(recordBuffer.begin());
        }
        recordBuffer.push_back({type, patchStartTime + totalElapsedTime, amount, duration});
    }

    void resetHourlyDailyCounters() {
        uint32_t now = millis() / 1000;

        if (now - lastHourResetSec >= 3600) {
            hourlyDelivered = 0.0;
            lastHourResetSec = now;
        }
        if (now - lastDayResetSec >= 86400) {
            dailyDelivered = 0.0;
            lastDayResetSec = now;
        }
    }

    void persistStats() {
        Preferences prefs;
        prefs.begin("pumpStats", false);
        prefs.putDouble("hourlyDel", hourlyDelivered);
        prefs.putDouble("dailyDel", dailyDelivered);
        prefs.putDouble("reservoir", reservoir);
        prefs.putDouble("stepCarry", stepCarryOver);
        prefs.putUInt("lastHourRst", lastHourResetSec);
        prefs.putUInt("lastDayRst", lastDayResetSec);
        prefs.end();
    }

    void updatePrimeProgress() {
        if (patchState == PatchState::PRIMING) {
            primeProgress += 40;
            Serial.print("[PRIME] progress=");
            Serial.println(primeProgress);
            if (primeProgress >= 240) {
                setPatchState(PatchState::PRIMED);
                primeProgress = 0;
                Logger::info("预充完成 -> PRIMED");
            }
        }
    }

    void setPatchState(PatchState newState) {
        if (patchState == newState) return;
        PatchState oldState = patchState;
        patchState = newState;
        Logger::info("状态变更: " + String(getStateName(oldState)) + " -> " + String(getStateName(newState)));

        patchStateDirty = true;

        sendStateNotification();
    }

    void checkAndSendStateNotification() {
        if (patchState != lastNotifiedState) {
            lastNotifiedState = patchState;
            sendStateNotification();
        }
    }

    void sendStateNotification() {
        if (!isSubscribed || !isConnected) {
            Serial.println("[STATE] skip: not subscribed or not connected");
            return;
        }
        std::vector<uint8_t> syncData = buildSynchronizeData();
        Serial.print("[STATE] sending notification, state=");
        Serial.print(getStateName(patchState));
        Serial.print(" dataLen=");
        Serial.println(syncData.size());
        sendNotificationPacket(syncData);
        lastNotifiedState = patchState;  // 标记已通知, 避免 update() 重复发送
    }

    void sendPingHeartbeat() {
        if (!isSubscribed || !isConnected) return;

        uint32_t currentTime = millis();
        if (currentTime - lastPingTime >= 5000) {
            lastPingTime = currentTime;
            pingCounter++;

            uint8_t pingData[7] = {0x07, 0x00, sequenceNumber, 0, 0, 0, 0};
            uint8_t crc = crc8Calculate(pingData, 6);
            pingData[6] = crc;

            gattServer.sendRawNotification(pingData, 7);
            // Logger::debug("发送 Ping 心跳 #" + String(pingCounter));
        }
    }

    void checkConnectionTimeout() {
        if (!isConnected || isSubscribed) return;

        uint32_t currentTime = millis();
        if (currentTime - lastActivityTime > connectionTimeoutMs) {
            Logger::warning("未认证连接超时,清理状态");
            connectionTracker.onDisconnect("认证超时");
            isConnected = false;
            authenticatedClients.clear();
        }
    }

    void sendPeriodicNotification() {
        if (currentBolus != nullptr) {
            // 大剂量期间每 2 秒发送一次状态通知, 让 Trio 更新进度
            static uint32_t lastBolusNotificationTime = 0;
            uint32_t now = millis();
            if (now - lastBolusNotificationTime >= 2000) {
                lastBolusNotificationTime = now;
                sendSynchronizeNotification();
                Logger::info("[BOLUS] 进度通知: 已输送 " + String(currentBolus->delivered) + " / " + String(currentBolus->amount) + "U");
            }
            return;
        }
        if (totalElapsedTime % 5 == 0) {
            sendSynchronizeNotification();
        }
    }

    void sendPrimeProgressNotification() {
        if (patchState != PatchState::PRIMING) return;
        uint32_t now = millis();
        if (now - lastPrimeNotificationTime >= 500) {
            lastPrimeNotificationTime = now;
            Serial.println("[PRIME] sending progress notification");
            sendSynchronizeNotification();
        }
    }

    void sendSynchronizeNotification() {
        std::vector<uint8_t> syncData = buildSynchronizeData();
        sendNotificationPacket(syncData);
    }

    std::vector<uint8_t> buildSynchronizeData() {
        std::vector<uint8_t> data;
        PatchState reportedState = patchState;
        if (everActivated && static_cast<uint8_t>(reportedState) < static_cast<uint8_t>(PatchState::PRIMING)) {
            reportedState = PatchState::EXPIRED;
        }
        data.push_back(static_cast<uint8_t>(reportedState));

        uint16_t fieldMask = 0;
        if (patchState == PatchState::SUSPENDED) {
            fieldMask |= MASK_SUSPEND;
        }
        if (currentBolus != nullptr || !bolusHistory.empty()) {
            fieldMask |= MASK_NORMAL_BOLUS;
        }
        fieldMask |= MASK_BASAL;
        if (patchState == PatchState::PRIMING) {
            fieldMask |= MASK_SETUP;
        }
        fieldMask |= MASK_RESERVOIR;
        fieldMask |= MASK_START_TIME;
        fieldMask |= MASK_BATTERY;
        fieldMask |= MASK_STORAGE;
        fieldMask |= MASK_ALARM;
        fieldMask |= MASK_AGE;
        fieldMask |= MASK_MAGNETO_PLACE;

        data.push_back(fieldMask & 0xFF);
        data.push_back((fieldMask >> 8) & 0xFF);

        if (fieldMask & MASK_SUSPEND) {
            uint32_t suspendTime = totalElapsedTime;
            data.push_back(suspendTime & 0xFF);
            data.push_back((suspendTime >> 8) & 0xFF);
            data.push_back((suspendTime >> 16) & 0xFF);
            data.push_back((suspendTime >> 24) & 0xFF);
        }

        if (fieldMask & MASK_NORMAL_BOLUS) {
            if (currentBolus) {
                uint8_t flags = currentBolus->type & 0x7F;
                if (currentBolus->delivered >= currentBolus->amount - 0.001) flags |= 0x80;
                data.push_back(flags);
                uint16_t deliveredRaw = static_cast<uint16_t>(round(currentBolus->delivered / 0.05));
                data.push_back(deliveredRaw & 0xFF);
                data.push_back((deliveredRaw >> 8) & 0xFF);
            } else {
                uint8_t completedFlags = bolusHistory.empty() ? 0 : (bolusHistory.back().type & 0x7F);
                completedFlags |= 0x80;
                data.push_back(completedFlags);
                uint16_t deliveredRaw = bolusHistory.empty() ? 0 : static_cast<uint16_t>(round(bolusHistory.back().delivered / 0.05));
                data.push_back(deliveredRaw & 0xFF);
                data.push_back((deliveredRaw >> 8) & 0xFF);
            }
        }

        if (fieldMask & MASK_BASAL) {
            BasalType basalType = BasalType::STANDARD;
            double basalRate = currentBasalRate;
            if (tempBasal) {
                basalType = BasalType::ABSOLUTE_TEMP;
                basalRate = tempBasal->rate;
            } else if (patchState == PatchState::SUSPENDED) {
                basalType = BasalType::SUSPEND_MANUAL;
                basalRate = 0;
            } else if (patchState == PatchState::STOPPED) {
                basalType = BasalType::STOP;
                basalRate = 0;
            } else if (patchState == PatchState::LOW_BG_SUSPENDED || patchState == PatchState::LOW_BG_SUSPENDED2) {
                basalType = BasalType::SUSPEND_LOW_GLUCOSE;
                basalRate = 0;
            } else if (patchState == PatchState::AUTO_SUSPENDED) {
                basalType = BasalType::SUSPEND_AUTO;
                basalRate = 0;
            } else if (patchState == PatchState::HOURLY_MAX_SUSPENDED) {
                basalType = BasalType::SUSPEND_MORE_THAN_MAX_PER_HOUR;
                basalRate = 0;
            } else if (patchState == PatchState::DAILY_MAX_SUSPENDED) {
                basalType = BasalType::SUSPEND_MORE_THAN_MAX_PER_DAY;
                basalRate = 0;
            }

            data.push_back(static_cast<uint8_t>(basalType));
            data.push_back(basalSequence & 0xFF);
            data.push_back((basalSequence >> 8) & 0xFF);
            data.push_back(patchId & 0xFF);
            data.push_back((patchId >> 8) & 0xFF);
            uint32_t startTime = patchStartTime;
            data.push_back(startTime & 0xFF);
            data.push_back((startTime >> 8) & 0xFF);
            data.push_back((startTime >> 16) & 0xFF);
            data.push_back((startTime >> 24) & 0xFF);

            uint16_t rateRaw = static_cast<uint16_t>(round(basalRate / 0.05));
            uint16_t deliveryRaw = 0;
            uint32_t rateDelivery = (static_cast<uint32_t>(deliveryRaw) << 12) | (rateRaw & 0x0FFF);
            data.push_back(rateDelivery & 0xFF);
            data.push_back((rateDelivery >> 8) & 0xFF);
            data.push_back((rateDelivery >> 16) & 0xFF);
        }

        if (fieldMask & MASK_SETUP) {
            data.push_back(primeProgress);
        }

        if (fieldMask & MASK_RESERVOIR) {
            uint16_t reservoirRaw = static_cast<uint16_t>(round(reservoir / 0.05));
            data.push_back(reservoirRaw & 0xFF);
            data.push_back((reservoirRaw >> 8) & 0xFF);
        }

        if (fieldMask & MASK_START_TIME) {
            data.push_back(patchStartTime & 0xFF);
            data.push_back((patchStartTime >> 8) & 0xFF);
            data.push_back((patchStartTime >> 16) & 0xFF);
            data.push_back((patchStartTime >> 24) & 0xFF);
        }

        if (fieldMask & MASK_BATTERY) {
            uint16_t voltageA = static_cast<uint16_t>(round(batteryVoltage * 512));
            uint16_t voltageB = static_cast<uint16_t>(round(batteryVoltage * 512));
            uint32_t packed = (static_cast<uint32_t>(voltageB) << 12) | (voltageA & 0x0FFF);
            data.push_back(packed & 0xFF);
            data.push_back((packed >> 8) & 0xFF);
            data.push_back((packed >> 16) & 0xFF);
        }

        if (fieldMask & MASK_STORAGE) {
            data.push_back(basalSequence & 0xFF);
            data.push_back((basalSequence >> 8) & 0xFF);
            data.push_back(patchId & 0xFF);
            data.push_back((patchId >> 8) & 0xFF);
        }

        if (fieldMask & MASK_ALARM) {
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
        }

        if (fieldMask & MASK_AGE) {
            data.push_back(totalElapsedTime & 0xFF);
            data.push_back((totalElapsedTime >> 8) & 0xFF);
            data.push_back((totalElapsedTime >> 16) & 0xFF);
            data.push_back((totalElapsedTime >> 24) & 0xFF);
        }

        if (fieldMask & MASK_MAGNETO_PLACE) {
            data.push_back(100);
            data.push_back(0);
        }

        return data;
    }

    // ========== 队列输注系统 ==========

    void buildBasalQueue() {
        basalQueue.clear();
        basalQueueIdx = 0;
        if (basalProfile.size() < 1) return;

        uint8_t entryCount = basalProfile[0];
        if (entryCount == 0 || basalProfile.size() < 1 + (size_t)entryCount * 3) return;

        struct BasalEntry {
            double rate;
            uint16_t startTimeMinutes;
        };
        BasalEntry entries[48];
        uint8_t actualCount = 0;

        for (uint8_t i = 0; i < entryCount; i++) {
            size_t offset = 1 + i * 3;
            uint32_t raw24 = (uint32_t)basalProfile[offset] |
                             ((uint32_t)basalProfile[offset + 1] << 8) |
                             ((uint32_t)basalProfile[offset + 2] << 16);
            uint16_t rateRaw = (raw24 >> 12) & 0xFFF;
            uint16_t timeRaw = raw24 & 0xFFF;
            double rate = rateRaw * 0.05;
            if (rate > 0) {
                entries[actualCount].rate = rate;
                entries[actualCount].startTimeMinutes = timeRaw;
                actualCount++;
            }
        }

        if (actualCount == 0) return;

        // 按 5 分钟间隔拆分, 24h = 288 个 5 分钟段
        const uint16_t INTERVAL_MIN = 5;
        const uint16_t TOTAL_SLOTS = 288;
        uint32_t baseTime = millis();

        for (uint16_t slot = 0; slot < TOTAL_SLOTS; slot++) {
            uint16_t slotStartMin = slot * INTERVAL_MIN;
            double rate = 0;
            for (int8_t e = (int8_t)actualCount - 1; e >= 0; e--) {
                if (entries[e].startTimeMinutes <= slotStartMin) {
                    rate = entries[e].rate;
                    break;
                }
            }

            if (rate <= 0) continue;

            InsulinAction action;
            action.executeTimeMs = baseTime + (uint32_t)slot * INTERVAL_MIN * 60000;
            action.stepAmount = rate * INTERVAL_MIN / 60.0; // rate × 5min / 60min
            basalQueue.push_back(action);
        }
        Logger::info("基础率队列已构建: " + String(basalQueue.size()) + " steps (5min间隔)");
    }

    void buildTempBasalQueue(double rate, uint16_t durationMinutes) {
        tempBasalQueue.clear();
        tempBasalQueueIdx = 0;

        // 按 5 分钟间隔拆分
        const uint16_t INTERVAL_MIN = 5;
        uint32_t numSlots = durationMinutes / INTERVAL_MIN;
        if (numSlots == 0) numSlots = 1;
        uint32_t baseTime = millis();

        for (uint32_t s = 0; s < numSlots; s++) {
            InsulinAction action;
            action.executeTimeMs = baseTime + s * INTERVAL_MIN * 60000;
            action.stepAmount = rate * INTERVAL_MIN / 60.0;
            tempBasalQueue.push_back(action);
        }
        Logger::info("临时基础率队列已构建: " + String(tempBasalQueue.size()) + " steps (5min间隔) @ " + String(rate) + "U/hr x " + String(durationMinutes) + "min");
    }

    // void startGpioDelivery(double stepU, int stepCount) {
    //     Logger::info("[DEBUG] startGpioDelivery: stepU=" + String(stepU) + "U, stepCount=" + String(stepCount));
    //     if (stepCount < 1) return;
        
    //     gpioDeliveryStartMs = millis();
    //     gpioRemainingSteps = stepCount;
    //     gpioCurrentStep = 0;
    //     gpioTotalSteps = stepCount;
    //     Logger::info("GPIO 输注启动: " + String(stepU) + "U, " + String(stepCount) + " steps");
    // }

    void startGpioDelivery(double stepU, int stepCount) {
        if (stepCount < 1) return;

        // 防重入: 信号量保护, 确保任何时候只有一个输注线程
        if (xSemaphoreTake(xSemaphore, 0) != pdTRUE) {
            Logger::warning("[GPIO] 无法获取信号量, 正在输注中，本次请求被忽略");
            return;
        }

        if (isDeliveryTaskRunning) {
            xSemaphoreGive(xSemaphore);
            Logger::warning("[GPIO] 上一笔输注线程尚未结束，忽略本次触发");
            return;
        }

        isDeliveryTaskRunning = true;
        gpioRemainingSteps = stepCount;

        // GPIO 任务优先级必须低于 BLE 事件处理任务, 否则会抢占 BLE 导致断连。
        // 用优先级 1 (低), 确保 Bluefruit 协议栈能正常处理连接事件。
        BaseType_t taskCreated = xTaskCreate(gpioDeliveryTask, "PumpDelivery", 4096, this, 1, NULL);
        
        if (taskCreated != pdPASS) {
            Logger::error("[GPIO] xTaskCreate 失败！无法创建输注线程");
            isDeliveryTaskRunning = false;
            xSemaphoreGive(xSemaphore);
            return;
        }
        
        Logger::info("GPIO 独立线程已派发: " + String(stepU) + "U, " + String(stepCount) + " steps");
        xSemaphoreGive(xSemaphore);
    }

    void resetGpioState() {
        gpioDeliveryStartMs = 0;
        gpioRemainingSteps = 0;
        gpioCurrentStep = 0;
        gpioTotalSteps = 0;
    }

    bool isGpioDeliveryComplete() const {
        return !isDeliveryTaskRunning;
    }

    void executeQueueAction(InsulinAction& action) {

        double stepU = action.stepAmount;
        int stepCount = static_cast<int>(round(stepU / STEP_SIZE));

        Logger::info("[DEBUG] executeQueueAction called, stepU=" + String(stepU) + "U, stepCount=" + String(stepCount));

        // 启动 GPIO 物理按键输注 (储药器已在 processDeliveryQueues section 3.3 扣除)
        startGpioDelivery(stepU, stepCount);
    }

    /**
     * 处理输注队列 - 核心输注调度逻辑
     *
     * 本函数负责将基础率、临时基础率和大剂量的胰岛素请求统一调度,
     * 并按步进电机最小步长 (STEP_SIZE) 进行重组后执行实际输注。
     *
     * 执行流程:
     *   1. 将到期的临时基础率动作移入 bolusQueue
     *   2. 将到期的基础率动作移入 bolusQueue
     *   3. 汇总 bolusQueue 中所有动作, 按 STEP_SIZE 取整后执行 GPIO 输注
     *   4. 处理大剂量进度追踪和完成通知
     *
     * 调用时机:
     *   - 正常情况下每 5 分钟扫描一次
     *   - 有活跃大剂量 (currentBolus != nullptr) 时立即执行
     */
    void processDeliveryQueues() {
        uint32_t now = millis();
        // Logger::info("[DEBUG] processDeliveryQueues called, patchState=" + String(getStateName(patchState)) + 
        //              ", currentBolus=" + String(currentBolus ? "yes" : "null") + 
        //              ", bolusQueue.size=" + String(bolusQueue.size()));

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            // Logger::info("[DEBUG] patchState not ACTIVE, skipping delivery");
            return;
        }

        // 5分钟扫描一次；有活跃大剂量时立即执行
        if (currentBolus == nullptr && now - lastDeliveryScanTime < 300000) {
            // Logger::info("[DEBUG] Skipping delivery: no active bolus and 5min not elapsed");
            return;
        }
        lastDeliveryScanTime = now;

        // GPIO 输注线程在运行中时，跳过队列处理（等待输注完成后由下次扫描执行）
        if (isDeliveryTaskRunning) {
            Logger::info("[DEBUG] GPIO 输注线程正在运行，跳过本次队列处理");
            return;
        }

        // ===== Step 1: 消费临时基础率队列 =====
        // 将已到期 (executeTimeMs <= now) 的临时基础率动作移入 bolusQueue,
        // 由后续统一执行。临时基础率结束时清理状态并发送通知。
        if (tempBasalActive && tempBasalQueueIdx < tempBasalQueue.size()) {
            while (tempBasalQueueIdx < tempBasalQueue.size() && now >= tempBasalQueue[tempBasalQueueIdx].executeTimeMs) {
                bolusQueue.push_back(tempBasalQueue[tempBasalQueueIdx]);
                tempBasalQueueIdx++;
                tempBasalRemaining = tempBasal->durationMinutes - (now - tempBasalStartMs) / 60000.0;
                if (tempBasalRemaining <= 0) {
                    Logger::info("临时基础率结束");
                    tempBasalActive = false;
                    tempBasalQueue.clear();
                    tempBasalQueueIdx = 0;
                    if (tempBasal) {
                        delete tempBasal;
                        tempBasal = nullptr;
                    }
                    tempBasalRemaining = 0;
                    sendStateNotification();
                }
            }
        }

        // ===== Step 2: 消费基础率队列 =====
        // 临时基础率优先级高于基础率, tempBasalActive 时不消费 basalQueue
        if (!basalSuspended && !tempBasalActive && basalQueueIdx < basalQueue.size()) {
            while (basalQueueIdx < basalQueue.size() && now >= basalQueue[basalQueueIdx].executeTimeMs) {
                bolusQueue.push_back(basalQueue[basalQueueIdx]);
                basalQueueIdx++;
            }
        }

        // ===== Step 3: 处理 bolusQueue - 区分 bolus 和 basal =====
        if (!bolusQueue.empty()) {
            // 3.1 汇总所有待输注量 (含 stepCarryOver)
            double sum = stepCarryOver;
            for (const auto& action : bolusQueue) {
                sum += action.stepAmount;
            }
            bolusQueue.clear();
            stepCarryOver = 0;
            Logger::info("[DEBUG] bolusQueue sum=" + String(sum) + "U (含carryOver)");

            // 3.2 步进补偿: 向下取整到 STEP_SIZE 边界 (只少打, 绝不多打)
            // 安全原则: 输注泵宁可少打可补, 不可超打。carryOver 恒 >= 0, 表示"欠"的胰岛素,
            // 下次累积到 >= STEP_SIZE 时补打。保持守恒: sum == deliverAmount + stepCarryOver。
            double deliverAmount = floor(sum / STEP_SIZE) * STEP_SIZE;
            if (deliverAmount < 0) deliverAmount = 0;  // 防御: sum 为负时不打
            stepCarryOver = sum - deliverAmount;        // 恒 >= 0, 范围 [0, STEP_SIZE)
            // 硬上限保护: carryOver 不允许超过 CARRYOVER_MAX。
            // 若 sum 本身超过 2U 却因故无法取整输注 (例如长期撞储药器/配额上限),
            // 累积的欠债超过 2U 时直接丢弃超额, 宁可少打也不在配额恢复时一次性补打出过量。
            if (stepCarryOver > CARRYOVER_MAX) {
                Logger::warning("[安全] stepCarryOver=" + String(stepCarryOver) + "U 超过上限 " + String(CARRYOVER_MAX) + "U, 丢弃超额");
                stepCarryOver = CARRYOVER_MAX;
            }
            Logger::info("[DEBUG] deliverAmount=" + String(deliverAmount) + "U, stepCarryOver=" + String(stepCarryOver) + "U (floor)");

            // 3.3 安全检查: 限制单次输注量不超过储药器余量和小时/日最大量
            // 关键: 截断后必须重新向下取整到 STEP_SIZE, 否则 deliverAmount 不是 0.5U 的倍数,
            // 会导致 executeQueueAction 的 stepCount=round(... ) 与账面不一致 (账面记 0.3U, GPIO 实打 0.5U)。
            // 被截断/取整掉的差额全部补回 stepCarryOver, 保持 sum == delivered + carryOver 不破。
            if (deliverAmount > 0.001) {
                double capped = deliverAmount;
                if (capped > reservoir) {
                    Logger::warning("[安全] 输注量 " + String(capped) + "U 超过储药器余量 " + String(reservoir) + "U, 截断");
                    capped = reservoir;
                }
                double dailyRemaining = dailyMaxInsulin - dailyDelivered;
                if (capped > dailyRemaining) {
                    Logger::warning("[安全] 输注量 " + String(capped) + "U 超过日剩余配额 " + String(dailyRemaining) + "U, 截断");
                    capped = dailyRemaining;
                }
                double hourlyRemaining = hourlyMaxInsulin - hourlyDelivered;
                if (capped > hourlyRemaining) {
                    Logger::warning("[安全] 输注量 " + String(capped) + "U 超过小时剩余配额 " + String(hourlyRemaining) + "U, 截断");
                    capped = hourlyRemaining;
                }

                // 截断后再向下取整到 STEP_SIZE, 保证 GPIO 步数与账面完全一致
                double finalDeliver = floor(capped / STEP_SIZE) * STEP_SIZE;
                if (finalDeliver < 0) finalDeliver = 0;
                // 截断 + 二次取整掉的部分补回 carryOver (下次补打), 不丢失
                stepCarryOver += (deliverAmount - finalDeliver);
                deliverAmount = finalDeliver;
                // 二次封顶: 截断回补可能使 carryOver 超过 CARRYOVER_MAX, 超额丢弃防止配额恢复时过量补打
                if (stepCarryOver > CARRYOVER_MAX) {
                    Logger::warning("[安全] 截断后 stepCarryOver=" + String(stepCarryOver) + "U 超过上限 " + String(CARRYOVER_MAX) + "U, 丢弃超额");
                    stepCarryOver = CARRYOVER_MAX;
                }

                if (deliverAmount > 0.001) {
                    reservoir = max(0.0, reservoir - deliverAmount);
                    activeInsulin += deliverAmount;
                    hourlyDelivered += deliverAmount;
                    dailyDelivered += deliverAmount;
                    Logger::info("输注记录, 储药器余量: " + String(reservoir) + "U");

                    if (currentBolus != nullptr) {
                        InsulinAction action;
                        action.stepAmount = deliverAmount;
                        executeQueueAction(action);
                    } else {
                        Logger::info("[BASAL] 基础率输注 " + String(deliverAmount) + "U (软件记录, 无GPIO)");
                    }
                } else {
                    Logger::info("[DEBUG] 截断+取整后本次不输注, 全部转入 carryOver=" + String(stepCarryOver) + "U");
                }
            }
            // 持久化统计数据, 防止重启丢失
            persistStats();
            // stepCarryOver 独立保存, 不放入 bolusQueue, 不会被 clear() 丢失
        }

        // 3.6 大剂量完成检查
        // handleSetBolusRequest 已返回成功给 Trio, 必须通知 Trio 完成
        // 否则进度条会一直卡住
        // carryOver 留在 bolusQueue 里等下次累积到 STEP_SIZE 再输注
        if (currentBolus && isGpioDeliveryComplete()) {
            Logger::info("大剂量完成: 通知Trio已完成 (carryOver留在队列累积)");
            currentBolus->delivered = currentBolus->amount;
            bolusHistory.push_back(*currentBolus);
            sendStateNotification();
            sendSynchronizeNotification();
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
            lastReportedBolusProgress = 0;
            // 不清 bolusQueue, carryOver 留着下次累积
            bolusQueueIdx = 0;
            resetGpioState();
        }
    }

    static void handleWriteRequestStatic(const uint8_t* data, size_t len) {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleWriteRequest(data, len);
        }
    }

    static void handleConnectStatic() {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleBleConnect();
        }
    }

    static void handleDisconnectStatic() {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleBleDisconnect();
        }
    }

    static void handleSubscribeStatic(bool subscribed) {
        extern M640GPumpSimulator* gSimulator;
        if (gSimulator) {
            gSimulator->handleSubscribe(subscribed);
        }
    }

    void handleWriteRequest(const uint8_t* data, size_t len) {
        lastActivityTime = millis();

        Logger::hexDump("RX", "<--", data, len);

        if (len < 4) {
            Logger::warning("数据长度太短: " + String(len));
            return;
        }

        uint8_t packetLen = data[0];
        uint8_t cmdType = data[1];
        uint8_t seqNum = data[2];
        uint8_t pkgIndex = data[3];

        if (cmdType == 0x00) {
            Logger::debug("忽略 Ping 消息");
            return;
        }

        Logger::info("收到命令: " + String(getCommandName(cmdType)) +
            " len=" + String(packetLen) + " seq=" + String(seqNum) + " pkg=" + String(pkgIndex));

        // 如果 packetBuffer 非空但收到的是新命令（pkgIndex == 0），说明上一个分包序列异常中断
        if (!packetBuffer.empty() && pkgIndex == 0) {
            Logger::warning("检测到新命令开始，清空上一个未完成的包缓冲区");
            packetBuffer.clear();
            expectedPacketLen = 0;
            currentCmdType = 0;
            currentSeqNum = 0;
            currentPkgIndex = 0;
        }

        if (packetBuffer.empty()) {
            // 判断是单包还是分包：
            // - 单包数据：pkgIndex == 0 且数据长度 >= packetLen + 1（有额外的 0 字节）
            // - 分包数据：pkgIndex == 0 但数据长度 < packetLen + 1，或者 pkgIndex > 0
            bool isSinglePacket = (pkgIndex == 0 && len >= packetLen + 1);
            
            if (isSinglePacket) {
                // 单包数据：复制整个数据（包括额外的 0 字节）
                size_t copyLen = min((size_t)len, (size_t)(packetLen + 1));
                packetBuffer.assign(data, data + copyLen);
                expectedPacketLen = packetLen;
                
                if (packetBuffer.size() >= (size_t)(packetLen + 1)) {
                    processCompleteCommand(packetBuffer.data(), packetLen + 1);
                    packetBuffer.clear();
                    expectedPacketLen = 0;
                    currentCmdType = 0;
                    currentSeqNum = 0;
                    currentPkgIndex = 0;
                }
            } else {
                // 分包数据的第一个包：复制 [header] + [content]，跳过 CRC
                // 数据格式：[header(4)] + [content(15)] + [CRC(1)]
                size_t copyLen = len - 1;  // 跳过 CRC
                packetBuffer.assign(data, data + copyLen);
                expectedPacketLen = packetLen;
                currentCmdType = cmdType;
                currentSeqNum = seqNum;
                currentPkgIndex = pkgIndex;
                
                Logger::debug("等待更多数据包... (" + String(packetBuffer.size()) + "/" + String(packetLen) + ")");
            }
        } else {
            if (cmdType != currentCmdType || pkgIndex != currentPkgIndex + 1) {
                Logger::error("包索引不匹配: 期望 pkg=" + String(currentPkgIndex + 1) + " 收到 pkg=" + String(pkgIndex));
                sendErrorResponse(static_cast<CommandType>(currentCmdType), BLEErrorCode::TIMEOUT);
                packetBuffer.clear();
                expectedPacketLen = 0;
                currentCmdType = 0;
                currentSeqNum = 0;
                currentPkgIndex = 0;
                return;
            }

            // 后续分包：复制 content 部分，跳过 header(4) 和 CRC(1)
            // 数据格式：[header(4)] + [content] + [CRC(1)]
            size_t contentLen = len - 5;  // 跳过 header(4) 和 CRC(1)
            
            // 检查是否是最后一个分包
            if (packetBuffer.size() + contentLen >= expectedPacketLen) {
                // 最后一个分包：调整复制长度，确保只复制到 expectedPacketLen
                contentLen = expectedPacketLen - packetBuffer.size();
            }
            
            packetBuffer.insert(packetBuffer.end(), data + 4, data + 4 + contentLen);
            currentPkgIndex = pkgIndex;

            Logger::debug("收到分包 " + String(pkgIndex) + " (" + String(packetBuffer.size()) + "/" + String(expectedPacketLen) + ")");

            if (packetBuffer.size() >= expectedPacketLen) {
                // 重要：Swift 计算 originalCRC 时 header 的 pkgIndex=0，
                // 但第一个分包 header 的 pkgIndex=1，需要改回 0 才能通过 CRC 校验
                packetBuffer[3] = 0;
                processCompleteCommand(packetBuffer.data(), expectedPacketLen);
                packetBuffer.clear();
                expectedPacketLen = 0;
                currentCmdType = 0;
                currentSeqNum = 0;
                currentPkgIndex = 0;
            }
        }
    }

    void handleSubscribe(bool subscribed) {
        isSubscribed = subscribed;
        if (subscribed) {
            Serial.println("");
            Serial.println("++++++++++++++++++++++++++++++++++++++++");
            Serial.println("+     NOTIFICATIONS SUBSCRIBED!        +");
            Serial.println("++++++++++++++++++++++++++++++++++++++++");
            Serial.println("");
            connectionTracker.onConnect();
            Logger::info("客户端已订阅通知");
            // 不主动发送状态通知 — Trio 期望先发 AUTH_REQ 认证,
            // 认证前收到通知会导致 Trio 跳过 PRIME 直接 ACTIVATE。
            // 状态通知会在 SYNCHRONIZE 流程中按需发送。
        } else {
            Serial.println("");
            Serial.println("----------------------------------------");
            Serial.println("-     NOTIFICATIONS UNSUBSCRIBED!      -");
            Serial.println("----------------------------------------");
            Serial.println("");
            connectionTracker.onDisconnect("客户端取消订阅");
            Logger::info("客户端已取消订阅");
        }
    }

    void handleBleConnect() {
        Serial.println("");
        Serial.println("****************************************");
        Serial.println("*     PUMP SIMULATOR: CLIENT CONNECTED     *");
        Serial.println("****************************************");
        Serial.println("");
        Logger::info("========== BLE 客户端已连接 ==========");

        // 不再检查 advertisingSuspended 拒绝连接:
        // 原逻辑每次断开都设 advertisingSuspended=true, iOS 用缓存 peripheral 重连时
        // 会被拒绝, 断开后又设 true, 形成死循环, 导致永远连不上。
        // advertisingSuspended 现在仅用于控制是否广播, 不阻止已建立的连接。

        isConnected = true;
        lastActivityTime = millis();
        connectionTracker.onConnect();
        // 不要在这里设置 isSubscribed = true
        // 等待客户端真正订阅通知特征后，handleSubscribe(true) 会被调用
    }

    void handleBleDisconnect() {
        Serial.println("");
        Serial.println("****************************************");
        Serial.println("*    PUMP SIMULATOR: CLIENT DISCONNECTED   *");
        Serial.println("****************************************");
        Serial.println("");
        Logger::info("========== BLE 客户端已断开 ==========");
        isConnected = false;
        isSubscribed = false;
        isAuthenticated = false;
        authenticatedClients.clear();
        packetBuffer.clear();
        expectedPacketLen = 0;
        currentCmdType = 0;
        connectionTracker.onDisconnect("BLE 断开");

        // 不再每次断开都暂停广播:
        // 原逻辑会导致 iOS 自动重连时被拒绝 (handleBleConnect 已移除拒绝逻辑),
        // 但暂停广播仍会导致 iOS 扫描找不到设备。
        // 广播暂停仅在 pendingBleDisconnect (主动停止 patch) 时设置, 那里是永久暂停。
        // 正常断开后立即恢复广播, 允许 iOS 重连。
        gattServer.advertisingSuspended = false;
        advertisingResumeTime = 0;
        gattServer.startAdvertising();
        Logger::info("BLE断开, 已恢复广播等待重连");
    }

    void processCompleteCommand(const uint8_t* data, uint8_t len) {
        if (len < 6) {
            Logger::error("完整命令数据长度不足: " + String(len));
            return;
        }

        // 尝试两种 CRC 校验方式：
        // 1. 单包数据格式：[header] + [content] + [CRC] + [0]，CRC 在 len-2 位置
        // 2. 分包数据格式：[header] + [content] + [CRC]，CRC 在 len-1 位置
        uint8_t expectedCrc = crc8Calculate(data, len - 2);
        if (data[len - 2] == expectedCrc) {
            // 单包格式 CRC 校验通过
        } else {
            // 尝试分包格式 CRC 校验
            expectedCrc = crc8Calculate(data, len - 1);
            if (data[len - 1] == expectedCrc) {
                // 分包格式 CRC 校验通过
            } else {
                Logger::error("CRC校验失败: 期望=0x" + String(expectedCrc, HEX) + " 收到=0x" + String(data[len - 2], HEX) + " (单包) 或 0x" + String(data[len - 1], HEX) + " (分包)");
                return;
            }
        }

        uint8_t cmdType = data[1];
        uint8_t seqNum = data[2];

        Logger::info("处理完整命令: " + String(getCommandName(cmdType)) + " (" + String(len) + " bytes)");

        // AUTH_REQ 不需要认证，其他命令需要
        if (cmdType != static_cast<uint8_t>(CommandType::AUTH_REQ) && !isAuthenticated) {
            Logger::warning("未认证客户端尝试执行命令: " + String(getCommandName(cmdType)));
            sendErrorResponse(static_cast<CommandType>(cmdType), BLEErrorCode::AUTH_REQUIRED);
            return;
        }

        switch (static_cast<CommandType>(cmdType)) {
            case CommandType::AUTH_REQ:
                handleAuthRequest(data, len, seqNum);
                break;
            case CommandType::SYNCHRONIZE:
                handleSynchronizeRequest(data, len, seqNum);
                break;
            case CommandType::SUBSCRIBE:
                handleSubscribeRequest(data, len, seqNum);
                break;
            case CommandType::GET_DEVICE_TYPE:
                handleGetDeviceTypeRequest(data, len, seqNum);
                break;
            case CommandType::GET_TIME:
                handleGetTimeRequest(data, len, seqNum);
                break;
            case CommandType::SET_TIME:
                handleSetTimeRequest(data, len, seqNum);
                break;
            case CommandType::SET_TIME_ZONE:
                handleSetTimeZoneRequest(data, len, seqNum);
                break;
            case CommandType::PRIME:
                handlePrimeRequest(data, len, seqNum);
                break;
            case CommandType::SET_BOLUS:
                handleSetBolusRequest(data, len, seqNum);
                break;
            case CommandType::CANCEL_BOLUS:
                handleCancelBolusRequest(data, len, seqNum);
                break;
            case CommandType::READ_BOLUS_STATE:
                handleReadBolusStateRequest(data, len, seqNum);
                break;
            case CommandType::SET_TEMP_BASAL:
                handleSetTempBasalRequest(data, len, seqNum);
                break;
            case CommandType::CANCEL_TEMP_BASAL:
                handleCancelTempBasalRequest(data, len, seqNum);
                break;
            case CommandType::SUSPEND_PUMP:
                handleSuspendRequest(data, len, seqNum);
                break;
            case CommandType::RESUME_PUMP:
                handleResumeRequest(data, len, seqNum);
                break;
            case CommandType::SET_BASAL_PROFILE:
                handleSetBasalProfileRequest(data, len, seqNum);
                break;
            case CommandType::CLEAR_ALARM:
                handleClearAlarmRequest(data, len, seqNum);
                break;
            case CommandType::ACTIVATE:
                handleActivateRequest(data, len, seqNum);
                break;
            case CommandType::STOP_PATCH:
                handleStopPatchRequest(data, len, seqNum);
                break;
            case CommandType::SET_PATCH:
                handleSetPatchRequest(data, len, seqNum);
                break;
            case CommandType::POLL_PATCH:
                handlePollPatchRequest(data, len, seqNum);
                break;
            case CommandType::GET_RECORD:
                handleGetRecordRequest(data, len, seqNum);
                break;
            case CommandType::SET_BOLUS_MOTOR:
                handleSetBolusMotorRequest(data, len, seqNum);
                break;
            default:
                Logger::warning("未知命令类型: " + String(cmdType));
                sendErrorResponse(static_cast<CommandType>(cmdType), BLEErrorCode::INVALID_PARAMETER);
        }
    }

    void handleAuthRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 认证请求 ===");

        if (len < 13) {
            Logger::error("认证请求数据长度不足: " + String(len));
            sendErrorResponse(CommandType::AUTH_REQ, BLEErrorCode::INVALID_PARAMETER);
            return;
        }

        uint8_t role = data[4];
        uint8_t clientToken[4] = {data[5], data[6], data[7], data[8]};
        uint8_t clientKey[4] = {data[9], data[10], data[11], data[12]};

        Logger::info("角色: " + String(role));
        Logger::info("客户端令牌: " + String(clientToken[0], HEX) + String(clientToken[1], HEX) + String(clientToken[2], HEX) + String(clientToken[3], HEX));

        uint8_t reversedSN[4] = {PUMP_SN[3], PUMP_SN[2], PUMP_SN[1], PUMP_SN[0]};
        uint8_t correctKey[4];
        Crypto::genKey(reversedSN, correctKey);

        Logger::info("期望密钥: " + String(correctKey[0], HEX) + String(correctKey[1], HEX) + String(correctKey[2], HEX) + String(correctKey[3], HEX));
        Logger::info("收到密钥: " + String(clientKey[0], HEX) + String(clientKey[1], HEX) + String(clientKey[2], HEX) + String(clientKey[3], HEX));

        if (memcmp(clientKey, correctKey, 4) != 0) {
            Logger::error("认证失败: 密钥不匹配");
            sendErrorResponse(CommandType::AUTH_REQ, BLEErrorCode::AUTH_FAILED);
            return;
        }

        Logger::info("认证成功!");

        uint8_t responseData[5] = {0x02, DEVICE_TYPE, 1, 0, 0};
        sendResponse(CommandType::AUTH_REQ, seqNum, responseData, 5);

        isConnected = true;
        isAuthenticated = true;
        uint32_t token = clientToken[0] | (clientToken[1] << 8) | (clientToken[2] << 16) | (clientToken[3] << 24);
        authenticatedClients.push_back(token);
    }

    void handleSynchronizeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 同步请求 ===");
        std::vector<uint8_t> syncData = buildSynchronizeData();
        Logger::info("返回同步数据: " + String(syncData.size()) + " bytes, 状态=" + String(getStateName(patchState)));
        sendResponse(CommandType::SYNCHRONIZE, seqNum, syncData.data(), syncData.size());
    }

    void handleSubscribeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 订阅请求 ===");
        if (len >= 6) {
            uint16_t subValue = data[4] | (data[5] << 8);
            Logger::info("订阅值: " + String(subValue));
        }
        isSubscribed = true;
        sendResponse(CommandType::SUBSCRIBE, seqNum, nullptr, 0);
        Logger::info("订阅成功, 开始发送通知");
        sendStateNotification();
    }

    void handleGetDeviceTypeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 获取设备类型 ===");
        uint8_t responseData[10] = {DEVICE_TYPE, 1, 0, 1, 0, 0, PUMP_SN[0], PUMP_SN[1], PUMP_SN[2], PUMP_SN[3]};
        sendResponse(CommandType::GET_DEVICE_TYPE, seqNum, responseData, 10);
    }

    void handleGetTimeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 获取时间 ===");
        uint32_t currentTime = patchStartTime + totalElapsedTime;
        uint8_t responseData[4] = {
            static_cast<uint8_t>(currentTime & 0xFF),
            static_cast<uint8_t>((currentTime >> 8) & 0xFF),
            static_cast<uint8_t>((currentTime >> 16) & 0xFF),
            static_cast<uint8_t>((currentTime >> 24) & 0xFF)
        };
        Logger::info("当前泵时间: " + String(currentTime));
        sendResponse(CommandType::GET_TIME, seqNum, responseData, 4);
    }

    void handleSetTimeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置时间 ===");
        if (len >= 9) {
            uint8_t prefix = data[4];
            uint32_t newTime = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
            patchStartTime = newTime;
            totalElapsedTime = 0;
            timeSyncPending = false;
            patchStateDirty = true;
            Logger::info("时间已同步: prefix=" + String(prefix) + " time=" + String(newTime));
        }
        sendResponse(CommandType::SET_TIME, seqNum, nullptr, 0);
    }

    void handleSetTimeZoneRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置时区 ===");
        if (len >= 10) {
            uint16_t tzOffsetRaw = static_cast<uint16_t>(data[4] | (data[5] << 8));
            int16_t tzOffset = static_cast<int16_t>(tzOffsetRaw);
            pumpTimezone = tzOffset;
            uint32_t timeVal = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
            patchStartTime = timeVal;
            totalElapsedTime = 0;
            patchStateDirty = true;
            Logger::info("时区偏移: " + String(tzOffset) + " 分钟, 时间: " + String(timeVal));
        }
        sendResponse(CommandType::SET_TIME_ZONE, seqNum, nullptr, 0);
    }

    void handlePrimeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 预充请求 ===");

        if (patchState == PatchState::PRIMING) {
            Logger::warning("已在预充中, 重置进度并继续");
            primeProgress = 0;
            sendResponse(CommandType::PRIME, seqNum, nullptr, 0);
            sendStateNotification();
            return;
        }

        if (patchState == PatchState::STOPPED || patchState == PatchState::EXPIRED ||
            patchState == PatchState::PATCH_FAULT || patchState == PatchState::PATCH_FAULT2) {
            Logger::warning("当前状态不允许预充: " + String(getStateName(patchState)));
            sendErrorResponse(CommandType::PRIME, BLEErrorCode::INVALID_STATE);
            return;
        }

        setPatchState(PatchState::PRIMING);
        primeProgress = 0;
        Logger::info("预充已开始 -> PRIMING");
        sendResponse(CommandType::PRIME, seqNum, nullptr, 0);
    }

    void handleSetBolusRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置大剂量 ===");

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            Logger::warning("泵未处于运行状态(" + String(getStateName(patchState)) + "),无法执行大剂量");
            sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::INVALID_STATE);
            return;
        }

        if (len >= 7) {
            uint8_t bolusType = data[4];
            uint16_t amountRaw = data[5] | (data[6] << 8);
            double amount = amountRaw * 0.05;

            Logger::info("大剂量类型: " + String(bolusType) + ", 剂量: " + String(amount) + "U");

            if (amountRaw == 0) {
                Logger::warning("大剂量金额为0,无效请求");
                sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::INVALID_PARAMETER);
                return;
            }

            if (currentBolus != nullptr) {
                // 如果当前 bolus 还有 carryOver 未输注, 允许新 bolus 叠加
                // 否则拒绝 (正在 GPIO 输注中)
                if (isDeliveryTaskRunning) {
                    Logger::warning("已有大剂量在GPIO输注中");
                    sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::PUMP_BUSY);
                    return;
                }
                // 叠加: 新量加入队列, 更新 currentBolus 的 amount
                Logger::info("叠加新大剂量: " + String(amount) + "U 到已有 " + String(currentBolus->amount) + "U");
                currentBolus->amount += amount;
                InsulinAction bolusAction;
                bolusAction.stepAmount = amount;
                bolusQueue.push_back(bolusAction);
                addRecord(1, amount, 0);
                Logger::info("大剂量已叠加到队列: " + String(amount) + "U, 总计: " + String(currentBolus->amount) + "U");
                sendResponse(CommandType::SET_BOLUS, seqNum, nullptr, 0);
                return;
            }

            if (amount > reservoir) {
                Logger::warning("储药器余量不足: 需要" + String(amount) + "U, 剩余" + String(reservoir) + "U");
                sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::RESERVOIR_EMPTY);
                return;
            }

            if (amount > MAX_BOLUS) {
                Logger::warning("超过最大大剂量限制: " + String(amount) + "U > " + String(MAX_BOLUS) + "U");
                sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::MAX_BOLUS_EXCEEDED);
                return;
            }

            if (amount + hourlyDelivered > hourlyMaxInsulin) {
                Logger::warning("超过每小时最大胰岛素限制: " + String(amount + hourlyDelivered) + "U > " + String(hourlyMaxInsulin) + "U");
                sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::HOURLY_MAX_EXCEEDED);
                return;
            }

            if (amount + dailyDelivered > dailyMaxInsulin) {
                Logger::warning("超过每日最大胰岛素限制: " + String(amount + dailyDelivered) + "U > " + String(dailyMaxInsulin) + "U");
                sendErrorResponse(CommandType::SET_BOLUS, BLEErrorCode::DAILY_MAX_EXCEEDED);
                return;
            }

            currentBolus = new BolusInfo{bolusType, amount, 0.0, millis() / 1000};
            bolusDeliveryProgress = 0;
            lastReportedBolusProgress = 0;
            lastBolusProgressReportTime = 0;
            addRecord(1, amount, 0);
            {
                InsulinAction bolusAction;
                bolusAction.stepAmount = amount;
                bolusQueue.push_back(bolusAction);
                Logger::info("大剂量已加入队列: " + String(amount) + "U");
            }
            Logger::info("大剂量已开始输送: " + String(amount) + "U");
        }
        sendResponse(CommandType::SET_BOLUS, seqNum, nullptr, 0);
    }

    void handleCancelBolusRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 取消大剂量 ===");
        if (len >= 5) {
            uint8_t bolusType = data[4];
            Logger::info("取消大剂量类型: " + String(bolusType));
        }
        if (currentBolus) {
            double undelivered = currentBolus->amount - currentBolus->delivered;
            Logger::info("大剂量已取消, 已输送: " + String(currentBolus->delivered) + "U, 未输送: " + String(undelivered) + "U");
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
            lastReportedBolusProgress = 0;
            // stepCarryOver 独立保存, bolusQueue 可安全清空
            bolusQueue.clear();
            bolusQueueIdx = 0;
            sendStateNotification();
        } else {
            Logger::info("没有进行中的大剂量");
        }
        sendResponse(CommandType::CANCEL_BOLUS, seqNum, nullptr, 0);
    }

    void handleReadBolusStateRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 读取大剂量状态 ===");
        uint8_t responseData[10];
        if (currentBolus) {
            responseData[0] = 1;
            responseData[1] = 0x01;
            uint16_t amountRaw = static_cast<uint16_t>(round(currentBolus->amount / 0.05));
            uint16_t deliveredRaw = static_cast<uint16_t>(round(currentBolus->delivered / 0.05));
            uint16_t remainingRaw = amountRaw - deliveredRaw;
            responseData[2] = deliveredRaw & 0xFF;
            responseData[3] = (deliveredRaw >> 8) & 0xFF;
            responseData[4] = remainingRaw & 0xFF;
            responseData[5] = (remainingRaw >> 8) & 0xFF;
            responseData[6] = amountRaw & 0xFF;
            responseData[7] = (amountRaw >> 8) & 0xFF;
            responseData[8] = 0;
            responseData[9] = 0;
            Logger::info("大剂量进行中: " + String(currentBolus->amount) + "U, 已输送: " + String(currentBolus->delivered) + "U");
        } else {
            memset(responseData, 0, 10);
            Logger::info("无进行中的大剂量");
        }
        sendResponse(CommandType::READ_BOLUS_STATE, seqNum, responseData, 10);
    }

    void handleSetTempBasalRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置临时基础率 ===");

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            Logger::warning("泵未处于运行状态,无法设置临时基础率");
            sendErrorResponse(CommandType::SET_TEMP_BASAL, BLEErrorCode::INVALID_STATE);
            return;
        }

        if (len >= 9) {
            uint8_t basalType = data[4];
            uint16_t rateRaw = data[5] | (data[6] << 8);
            uint16_t durationRaw = data[7] | (data[8] << 8);
            double rate = rateRaw * 0.05;
            Logger::info("类型: " + String(basalType) + ", 速率: " + String(rate) + "U/hr, 时长: " + String(durationRaw) + "分钟");

            if (rate > MAX_BASAL_RATE) {
                Logger::warning("超过最大基础率限制: " + String(rate) + "U/hr > " + String(MAX_BASAL_RATE) + "U/hr");
                sendErrorResponse(CommandType::SET_TEMP_BASAL, BLEErrorCode::MAX_BASAL_EXCEEDED);
                return;
            }

            if (reservoir <= 0) {
                Logger::warning("储药器已空,无法设置临时基础率");
                sendErrorResponse(CommandType::SET_TEMP_BASAL, BLEErrorCode::RESERVOIR_EMPTY);
                return;
            }

            double estimatedDelivery = rate * durationRaw / 60.0;
            if (estimatedDelivery + hourlyDelivered > hourlyMaxInsulin) {
                Logger::warning("临时基础率可能超过每小时最大胰岛素限制");
                sendErrorResponse(CommandType::SET_TEMP_BASAL, BLEErrorCode::HOURLY_MAX_EXCEEDED);
                return;
            }

            if (estimatedDelivery + dailyDelivered > dailyMaxInsulin) {
                Logger::warning("临时基础率可能超过每日最大胰岛素限制");
                sendErrorResponse(CommandType::SET_TEMP_BASAL, BLEErrorCode::DAILY_MAX_EXCEEDED);
                return;
            }

            if (tempBasal) delete tempBasal;
            tempBasal = new TempBasalInfo{basalType, rate, durationRaw, millis() / 1000};
            tempBasalRemaining = durationRaw;
            basalSequence++;
            addRecord(2, rate, durationRaw);
            tempBasalActive = true;
            tempBasalStartMs = millis();
            buildTempBasalQueue(rate, durationRaw);
        }

        uint8_t responseData[11];
        responseData[0] = tempBasal ? static_cast<uint8_t>(BasalType::ABSOLUTE_TEMP) : static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = tempBasal ? static_cast<uint16_t>(round(tempBasal->rate / 0.05)) : static_cast<uint16_t>(round(currentBasalRate / 0.05));
        responseData[1] = basalValue & 0xFF;
        responseData[2] = (basalValue >> 8) & 0xFF;
        responseData[3] = basalSequence & 0xFF;
        responseData[4] = (basalSequence >> 8) & 0xFF;
        responseData[5] = patchId & 0xFF;
        responseData[6] = (patchId >> 8) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[7] = startTime & 0xFF;
        responseData[8] = (startTime >> 8) & 0xFF;
        responseData[9] = (startTime >> 16) & 0xFF;
        responseData[10] = (startTime >> 24) & 0xFF;

        sendResponse(CommandType::SET_TEMP_BASAL, seqNum, responseData, 11);
        sendStateNotification();
    }

    void handleCancelTempBasalRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 取消临时基础率 ===");
        if (tempBasal) {
            Logger::info("取消临时基础率: " + String(tempBasal->rate) + "U/hr");
            delete tempBasal;
            tempBasal = nullptr;
            tempBasalRemaining = 0;
            tempBasalActive = false;
            tempBasalQueue.clear();
            tempBasalQueueIdx = 0;
            basalSequence++;
            buildBasalQueue();
        } else {
            Logger::info("无进行中的临时基础率");
        }

        uint8_t responseData[11];
        responseData[0] = static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = static_cast<uint16_t>(round(currentBasalRate / 0.05));
        responseData[1] = basalValue & 0xFF;
        responseData[2] = (basalValue >> 8) & 0xFF;
        responseData[3] = basalSequence & 0xFF;
        responseData[4] = (basalSequence >> 8) & 0xFF;
        responseData[5] = patchId & 0xFF;
        responseData[6] = (patchId >> 8) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[7] = startTime & 0xFF;
        responseData[8] = (startTime >> 8) & 0xFF;
        responseData[9] = (startTime >> 16) & 0xFF;
        responseData[10] = (startTime >> 24) & 0xFF;

        sendResponse(CommandType::CANCEL_TEMP_BASAL, seqNum, responseData, 11);
        sendStateNotification();
    }

    void handleSuspendRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 暂停泵 ===");

        if (patchState != PatchState::ACTIVE && patchState != PatchState::ACTIVE_ALT) {
            Logger::warning("泵未处于运行状态,无法暂停: " + String(getStateName(patchState)));
            sendErrorResponse(CommandType::SUSPEND_PUMP, BLEErrorCode::INVALID_STATE);
            return;
        }

        if (len >= 6) {
            uint8_t cause = data[4];
            uint8_t duration = data[5];
            Logger::info("暂停原因: " + String(cause) + ", 时长: " + String(duration) + "分钟");
            if (duration > 0) {
                suspendResumeTimeMs = millis() + (uint32_t)duration * 60000;
                Logger::info("将在 " + String(duration) + " 分钟后自动恢复");
            } else {
                suspendResumeTimeMs = 0;
            }
        }

        setPatchState(PatchState::SUSPENDED);
        simulatorState = SimulatorState::SUSPENDED;
        addRecord(3, 0, 0);
        basalSuspended = true;

        if (currentBolus) {
            Logger::info("暂停时取消大剂量, 已输送: " + String(currentBolus->delivered) + "U");
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
            lastReportedBolusProgress = 0;
            // stepCarryOver 独立保存, bolusQueue 可安全清空
            bolusQueue.clear();
            bolusQueueIdx = 0;
        }

        if (tempBasal) {
            Logger::info("暂停时暂停临时基础率(保留记录以便恢复后继续)");
            delete tempBasal;
            tempBasal = nullptr;
            tempBasalRemaining = 0;
            tempBasalActive = false;
            tempBasalQueue.clear();
            tempBasalQueueIdx = 0;
        }

        sendResponse(CommandType::SUSPEND_PUMP, seqNum, nullptr, 0);
    }

    void handleResumeRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 恢复泵 ===");

        bool canResume = (patchState == PatchState::SUSPENDED ||
                          patchState == PatchState::LOW_BG_SUSPENDED ||
                          patchState == PatchState::LOW_BG_SUSPENDED2 ||
                          patchState == PatchState::AUTO_SUSPENDED ||
                          patchState == PatchState::HOURLY_MAX_SUSPENDED ||
                          patchState == PatchState::DAILY_MAX_SUSPENDED ||
                          patchState == PatchState::PAUSED);

        if (!canResume) {
            Logger::warning("泵未处于暂停状态,无法恢复: " + String(getStateName(patchState)));
            sendErrorResponse(CommandType::RESUME_PUMP, BLEErrorCode::INVALID_STATE);
            return;
        }

        setPatchState(PatchState::ACTIVE);
        simulatorState = SimulatorState::RUNNING;
        addRecord(4, 0, 0);
        basalSuspended = false;
        suspendResumeTimeMs = 0;
        buildBasalQueue();
        Logger::info("泵已恢复 -> ACTIVE");
        sendResponse(CommandType::RESUME_PUMP, seqNum, nullptr, 0);
    }

    void handleSetBasalProfileRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置基础率配置文件 ===");
        if (len >= 5) {
            uint8_t profileType = data[4];
            Logger::info("配置文件类型: " + String(profileType));
            if (len > 5) {
                size_t profileLen = len - 5;
                basalProfile.assign(data + 5, data + 5 + profileLen);
                Logger::info("配置文件数据: " + String(profileLen) + " bytes");

                if (profileLen >= 4) {
                    uint8_t entryCount = data[5];
                    Logger::info("条目数量: " + String(entryCount));

                    if (entryCount > 0 && profileLen >= 1 + (size_t)entryCount * 3) {
                        uint32_t absoluteSecs = patchStartTime + totalElapsedTime;
                        uint32_t nowMinutes = (absoluteSecs % 86400) / 60;

                        double activeRate = 0;
                        uint16_t activeStart = 0;

                        for (uint8_t i = 0; i < entryCount; i++) {
                            size_t entryOffset = 6 + i * 3;
                            uint32_t raw24 = (uint32_t)data[entryOffset] |
                                             ((uint32_t)data[entryOffset + 1] << 8) |
                                             ((uint32_t)data[entryOffset + 2] << 16);
                            uint16_t rateRaw = (raw24 >> 12) & 0xFFF;
                            uint16_t timeRaw = raw24 & 0xFFF;
                            double rate = rateRaw * 0.05;
                            Logger::info("  条目[" + String(i) + "]: " + String(rate) + "U/hr, 开始时间: " + String(timeRaw) + "分钟");

                            if (timeRaw <= nowMinutes) {
                                activeRate = rate;
                                activeStart = timeRaw;
                            }
                        }

                        if (activeRate > 0) {
                            currentBasalRate = activeRate;
                            Logger::info("当前活跃基础率: " + String(currentBasalRate) + "U/hr, 开始时间: " + String(activeStart) + "分钟 (当前时间: " + String(nowMinutes) + "分钟)");
                        } else {
                            currentBasalRate = 0;
                            Logger::info("未找到当前时间匹配的基础率条目");
                        }
                    }
                }
            }
            basalSequence++;
            if (!tempBasalActive && !basalSuspended) {
                buildBasalQueue();
            }
        }
        uint8_t responseData[11];
        responseData[0] = static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = static_cast<uint16_t>(round(currentBasalRate / 0.05));
        responseData[1] = basalValue & 0xFF;
        responseData[2] = (basalValue >> 8) & 0xFF;
        responseData[3] = basalSequence & 0xFF;
        responseData[4] = (basalSequence >> 8) & 0xFF;
        responseData[5] = patchId & 0xFF;
        responseData[6] = (patchId >> 8) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[7] = startTime & 0xFF;
        responseData[8] = (startTime >> 8) & 0xFF;
        responseData[9] = (startTime >> 16) & 0xFF;
        responseData[10] = (startTime >> 24) & 0xFF;
        sendResponse(CommandType::SET_BASAL_PROFILE, seqNum, responseData, 11);
    }

    void handleClearAlarmRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 清除警报 ===");
        if (len >= 6) {
            uint16_t alertType = data[4] | (data[5] << 8);
            Logger::info("警报类型: " + String(alertType));
        }
        sendResponse(CommandType::CLEAR_ALARM, seqNum, nullptr, 0);
    }

    void handleActivateRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 激活 Patch ===");

        if (patchState == PatchState::ACTIVE || patchState == PatchState::ACTIVE_ALT) {
            Logger::info("Patch 已处于激活状态,返回当前状态");
        } else if (patchState != PatchState::PRIMED) {
            Logger::warning("Patch 未完成预充,无法激活: " + String(getStateName(patchState)));
            sendErrorResponse(CommandType::ACTIVATE, BLEErrorCode::INVALID_STATE);
            return;
        }

        if (len >= 15) {
            uint8_t autoSuspendEnable = data[4];
            uint8_t autoSuspendTime = data[5];
            expirationTimer = data[6];
            alarmSetting = data[7];
            uint8_t lowSuspend = data[8];
            predictiveLowSuspend = data[9];
            predictiveLowSuspendRange = data[10];
            uint16_t hourlyMax = data[11] | (data[12] << 8);
            uint16_t dailyMax = data[13] | (data[14] << 8);
            hourlyMaxInsulin = hourlyMax * 0.05;
            dailyMaxInsulin = dailyMax * 0.05;
            Logger::info("激活参数: expirationTimer=" + String(expirationTimer) +
                " alarmSetting=" + String(alarmSetting) +
                " hourlyMax=" + String(hourlyMaxInsulin) + "U" +
                " dailyMax=" + String(dailyMaxInsulin) + "U");

            if (len >= 17) {
                uint16_t currentTDDRaw = data[15] | (data[16] << 8);
                Logger::info("当前TDD: " + String(currentTDDRaw * 0.05) + "U");
            }

            if (len > 18) {
                size_t profileLen = len - 18;
                basalProfile.assign(data + 18, data + 18 + profileLen);
                Logger::info("基础率配置文件: " + String(profileLen) + " bytes");
                if (profileLen >= 2) {
                    uint16_t firstRateRaw = data[18] | (data[19] << 8);
                    currentBasalRate = firstRateRaw * 0.05;
                    Logger::info("当前基础率: " + String(currentBasalRate) + "U/hr");
                }
            }
        }

        // 直接设置状态, 不在此处发通知 — setPatchState 会调 sendStateNotification,
        // 紧接着 sendResponse 的 notify 会因 HVN 队列满而失败, 导致 Trio 收不到激活响应。
        patchState = PatchState::ACTIVE;
        simulatorState = SimulatorState::RUNNING;
        everActivated = true;
        reservoir = MAX_RESERVOIR;
        totalElapsedTime = 0;
        hourlyDelivered = 0;
        dailyDelivered = 0;
        lastHourResetSec = millis() / 1000;
        lastDayResetSec = millis() / 1000;
        basalSequence = 0;
        basalSuspended = false;
        tempBasalActive = false;
        basalQueue.clear();
        basalQueueIdx = 0;
        tempBasalQueue.clear();
        tempBasalQueueIdx = 0;
        // prime: 储药器换新, 但 stepCarryOver 刻意保留 (这是上一个 patch 欠的胰岛素, 不应凭空消失)
        bolusQueue.clear();
        bolusQueueIdx = 0;
        buildBasalQueue();

        patchStateDirty = true;

        Logger::info("Patch 已激活 -> ACTIVE");

        Preferences metaPrefs;
        metaPrefs.begin("pumpMeta", false);
        metaPrefs.putUChar("activated", 1);
        metaPrefs.end();
        Logger::info("激活标记已持久化到pumpMeta");

        uint8_t responseData[25];
        memset(responseData, 0, 25);
        responseData[0] = patchId & 0xFF;
        responseData[1] = (patchId >> 8) & 0xFF;
        responseData[2] = (patchId >> 16) & 0xFF;
        responseData[3] = (patchId >> 24) & 0xFF;
        uint32_t startTime = patchStartTime;
        responseData[4] = startTime & 0xFF;
        responseData[5] = (startTime >> 8) & 0xFF;
        responseData[6] = (startTime >> 16) & 0xFF;
        responseData[7] = (startTime >> 24) & 0xFF;
        responseData[8] = static_cast<uint8_t>(BasalType::STANDARD);
        uint16_t basalValue = static_cast<uint16_t>(round(currentBasalRate / 0.05));
        responseData[9] = basalValue & 0xFF;
        responseData[10] = (basalValue >> 8) & 0xFF;
        responseData[11] = basalSequence & 0xFF;
        responseData[12] = (basalSequence >> 8) & 0xFF;
        responseData[13] = patchId & 0xFF;
        responseData[14] = (patchId >> 8) & 0xFF;
        responseData[15] = startTime & 0xFF;
        responseData[16] = (startTime >> 8) & 0xFF;
        responseData[17] = (startTime >> 16) & 0xFF;
        responseData[18] = (startTime >> 24) & 0xFF;
        // [19-20] basalPatchId (重复 patchId)
        responseData[19] = patchId & 0xFF;
        responseData[20] = (patchId >> 8) & 0xFF;
        // [21-24] basalStartTime (重复 startTime)
        responseData[21] = startTime & 0xFF;
        responseData[22] = (startTime >> 8) & 0xFF;
        responseData[23] = (startTime >> 16) & 0xFF;
        responseData[24] = (startTime >> 24) & 0xFF;
        sendResponse(CommandType::ACTIVATE, seqNum, responseData, 25);
    }

    void handleStopPatchRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 停止 Patch ===");
        patchState = PatchState::STOPPED;
        patchStateDirty = false;
        simulatorState = SimulatorState::EJECTING;
        patchStartTime = 0;
        totalElapsedTime = 0;
        reservoir = MAX_RESERVOIR;
        activeInsulin = 0;
        hourlyDelivered = 0;
        dailyDelivered = 0;
        lastHourResetSec = millis() / 1000;
        lastDayResetSec = millis() / 1000;
        basalSequence = 0;
        primeProgress = 0;
        lastNotifiedState = PatchState::NONE;

        Preferences prefs;
        prefs.begin("pump", false);
        prefs.remove("patchStart");
        prefs.remove("patchState");
        prefs.remove("elapsedTime");
        prefs.end();

        if (currentBolus) {
            delete currentBolus;
            currentBolus = nullptr;
            bolusDeliveryProgress = 0;
            lastReportedBolusProgress = 0;
            lastBolusProgressReportTime = 0;
        }
        if (tempBasal) {
            delete tempBasal;
            tempBasal = nullptr;
            tempBasalRemaining = 0;
        }
        basalQueue.clear();
        basalQueueIdx = 0;
        tempBasalQueue.clear();
        tempBasalQueueIdx = 0;
        // stop patch: stepCarryOver 刻意保留 (这是本贴欠的胰岛素统计, 不应凭空消失, 保留用于对账)
        bolusQueue.clear();
        bolusQueueIdx = 0;
        basalSuspended = false;
        tempBasalActive = false;

        uint8_t responseData[4] = {
            basalSequence & 0xFF,
            static_cast<uint8_t>((basalSequence >> 8) & 0xFF),
            static_cast<uint8_t>(patchId & 0xFF),
            static_cast<uint8_t>((patchId >> 8) & 0xFF)
        };
        sendResponse(CommandType::STOP_PATCH, seqNum, responseData, 4);

        pendingBleDisconnect = true;
        Logger::info("Patch 已停止, 将在主循环中断开BLE并重置状态");
    }

    void handleSetPatchRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置 Patch 参数 ===");
        if (len >= 5) {
            alarmSetting = data[4];
            Logger::info("警报设置: " + String(alarmSetting));
        }
        if (len >= 7) {
            uint16_t hourlyMax = data[5] | (data[6] << 8);
            hourlyMaxInsulin = hourlyMax * 0.05;
            Logger::info("每小时最大: " + String(hourlyMaxInsulin) + "U");
        }
        if (len >= 9) {
            uint16_t dailyMax = data[7] | (data[8] << 8);
            dailyMaxInsulin = dailyMax * 0.05;
            Logger::info("每日最大: " + String(dailyMaxInsulin) + "U");
        }
        if (len >= 10) {
            expirationTimer = data[9];
            Logger::info("过期计时器: " + String(expirationTimer));
        }
        if (len >= 14) {
            uint8_t autoSuspendEnable = data[10];
            uint8_t autoSuspendTime = data[11];
            uint8_t lowSuspend = data[12];
            predictiveLowSuspend = data[13];
            Logger::info("自动暂停: enable=" + String(autoSuspendEnable) +
                " time=" + String(autoSuspendTime) +
                " lowSuspend=" + String(lowSuspend) +
                " predictiveLowSuspend=" + String(predictiveLowSuspend));
        }
        if (len >= 15) {
            predictiveLowSuspendRange = data[14];
            Logger::info("预测低血糖暂停范围: " + String(predictiveLowSuspendRange));
        }
        sendResponse(CommandType::SET_PATCH, seqNum, nullptr, 0);
    }

    void handlePollPatchRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 轮询 Patch 状态 ===");
        std::vector<uint8_t> syncData = buildSynchronizeData();
        sendResponse(CommandType::POLL_PATCH, seqNum, syncData.data(), syncData.size());
    }

    void handleGetRecordRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 获取记录 ===");
        uint8_t requestType = 0;
        uint8_t recordIndex = 0;
        if (len >= 6) {
            requestType = data[4];
            recordIndex = data[5];
            Logger::info("记录类型: " + String(requestType) + ", 索引: " + String(recordIndex));
        }

        uint8_t responseData[20];
        memset(responseData, 0, 20);

        if (recordIndex < recordBuffer.size()) {
            const PumpRecord& rec = recordBuffer[recordBuffer.size() - 1 - recordIndex];
            responseData[0] = rec.type;
            responseData[1] = 0x00;
            responseData[2] = rec.timestamp & 0xFF;
            responseData[3] = (rec.timestamp >> 8) & 0xFF;
            responseData[4] = (rec.timestamp >> 16) & 0xFF;
            responseData[5] = (rec.timestamp >> 24) & 0xFF;
            uint16_t amountRaw = static_cast<uint16_t>(round(rec.amount / 0.05));
            responseData[6] = amountRaw & 0xFF;
            responseData[7] = (amountRaw >> 8) & 0xFF;
            responseData[8] = rec.duration & 0xFF;
            responseData[9] = (rec.duration >> 8) & 0xFF;
            Logger::info("返回记录[" + String(recordIndex) + "]: type=" + String(rec.type) +
                " amount=" + String(rec.amount) + " duration=" + String(rec.duration));
        } else {
            responseData[0] = 0x00;
            Logger::info("无更多记录 (索引=" + String(recordIndex) + ", 总数=" + String(recordBuffer.size()) + ")");
        }

        sendResponse(CommandType::GET_RECORD, seqNum, responseData, 20);
    }

    void handleSetBolusMotorRequest(const uint8_t* data, uint8_t len, uint8_t seqNum) {
        Logger::info("=== 设置大剂量电机 ===");
        if (len >= 7) {
            uint16_t motorSteps = data[4] | (data[5] << 8);
            uint8_t direction = data[6];
            Logger::info("电机步数: " + String(motorSteps) + ", 方向: " + String(direction));
        }
        sendResponse(CommandType::SET_BOLUS_MOTOR, seqNum, nullptr, 0);
    }

    void sendResponse(CommandType cmdType, uint8_t seqNum, const uint8_t* data, uint8_t dataLen) {
        uint8_t totalContentLen = 2 + dataLen;
        uint8_t totalPacketLen = totalContentLen + 6;

        uint8_t packet[256];
        packet[0] = static_cast<uint8_t>(totalContentLen + 5);
        packet[1] = static_cast<uint8_t>(cmdType);
        packet[2] = seqNum;
        packet[3] = 0;
        packet[4] = 0;
        packet[5] = 0;
        if (dataLen > 0) {
            memcpy(packet + 6, data, dataLen);
        }
        packet[totalContentLen + 4] = crc8Calculate(packet, totalContentLen + 4);
        packet[totalContentLen + 5] = 0;

        Logger::hexDump("TX", "-->", packet, totalPacketLen);
        gattServer.sendResponse(packet, totalPacketLen);
    }

    void sendErrorResponse(CommandType cmdType, uint16_t errorCode) {
        Logger::warning("发送错误响应: cmd=" + String(getCommandName(static_cast<uint8_t>(cmdType))) +
            " code=0x" + String(errorCode, HEX) + " (" + String(BLEErrorCode::toString(errorCode)) + ")");

        uint8_t header[4] = {
            7,
            static_cast<uint8_t>(cmdType),
            static_cast<uint8_t>(sequenceNumber),
            0
        };
        uint8_t packet[8];
        memcpy(packet, header, 4);
        packet[4] = static_cast<uint8_t>(errorCode & 0xFF);
        packet[5] = static_cast<uint8_t>((errorCode >> 8) & 0xFF);
        uint8_t crc = crc8Calculate(packet, 6);
        packet[6] = crc;
        packet[7] = 0;

        Logger::hexDump("TX", "-->", packet, 8);
        gattServer.sendResponse(packet, 8);
    }

    void sendNotificationPacket(const std::vector<uint8_t>& syncData) {
        if (syncData.empty()) return;

        // Swift NotificationPacket 期望的格式:
        //   [0] = state
        //   [1..2] = fieldMask
        //   [3..] = syncData
        // Swift handleHeartbeat 会追加 0x00 作为假 CRC
        // 所以这里只发送原始 syncData，不含包头和CRC
        gattServer.sendRawNotification(syncData.data(), syncData.size());
    }

    void sendNotificationPacketWithHeader(const std::vector<uint8_t>& syncData, uint8_t seqNum) {
        if (syncData.empty()) return;

        // 构建完整的通知数据包（含包头和CRC），用于需要标准包格式的场景
        uint8_t header[4] = {
            static_cast<uint8_t>(syncData.size() + 5),
            static_cast<uint8_t>(CommandType::SYNCHRONIZE),
            seqNum,
            0
        };

        uint8_t packet[256];
        memcpy(packet, header, 4);
        packet[4] = 0;
        packet[5] = 0;
        memcpy(packet + 6, syncData.data(), syncData.size());

        uint8_t crc = crc8Calculate(packet, syncData.size() + 6);
        packet[syncData.size() + 6] = crc;
        packet[syncData.size() + 7] = 0;

        gattServer.sendRawNotification(packet, syncData.size() + 8);
    }
};


M640GPumpSimulator* gSimulator = nullptr;

// 5000 wait
// 200 1000 第一步：唤醒屏幕 (短按)
// 2000 2000 第二步：触发进入声响大剂量模式 (长按)
// 400 600 loop
// 3000 输入完毕后，等一下再确认
// 4000 3000*steps 第四步：第一次长按确认（泵会回放步数声音）
// 2000 第五步：第二次长按确认（真正开始推药）

// 独立线程里的同步输注任务
void gpioDeliveryTask(void *parameter) {
    M640GPumpSimulator* simulator = static_cast<M640GPumpSimulator*>(parameter);

    // 上锁读取步数
    xSemaphoreTake(simulator->xSemaphore, portMAX_DELAY);
    int steps = simulator->gpioRemainingSteps;
    xSemaphoreGive(simulator->xSemaphore);

    if (steps <= 0) {
        Logger::warning("[Task] 步数为0，跳过输注");
        goto finish;
    }

    Logger::info("[Task] 独立输注线程启动, steps=" + String(steps));

// ==================== 修改后的 TS5A3166 声音大剂量物理敲击时序 ====================
    // steps = 10; // 举例：输入 10 步

    // 1. 唤醒屏幕 / 进入界面 (第一次短按)
    Serial.println(">>> 1. 短按唤醒/进入声音大剂量...");
    // digitalWrite(STEP_PIN, HIGH); // 高电平：按下
    // delay(140);                   // TS5A3166 纯净信号，140ms 稳定骗过消抖
    // digitalWrite(STEP_PIN, LOW);  // 低电平：松开
    // delay(1000);                  // 等待屏幕或界面完全加载

    // 2. 触发进入模式 (长按 2 秒)
    Serial.println(">>> 2. 长按触发模式...");
    digitalWrite(STEP_PIN, HIGH); // 高电平：按下
    safeDelay(2000);              // 持续稳定按下 2 秒
    digitalWrite(STEP_PIN, LOW);  // 低电平：松开
    safeDelay(1500);              // 等待泵发出准备就绪的提示音

    // 3. 循环输入步数 (核心修改：拉长按下与松开时间)
    Serial.println(">>> 3. 开始循环敲击步数...");
    for (int i = 1; i <= steps; i++) {
        digitalWrite(STEP_PIN, HIGH); // 高电平：按下
        safeDelay(500);               // 按下维持 500ms
        digitalWrite(STEP_PIN, LOW);  // 低电平：松开
        safeDelay(500);               // 松开维持 500ms

        // 每完成一步, 更新 currentBolus->delivered 进度, 让 iOS 看到进度变化
        // 否则进度一直卡在 0.00, iOS 会超时断开连接
        xSemaphoreTake(simulator->xSemaphore, portMAX_DELAY);
        if (simulator->currentBolus != nullptr) {
            double newDelivered = i * STEP_SIZE;
            if (newDelivered > simulator->currentBolus->amount) {
                newDelivered = simulator->currentBolus->amount;
            }
            simulator->currentBolus->delivered = newDelivered;
        }
        xSemaphoreGive(simulator->xSemaphore);
    }
    safeDelay(2000); // 步数输完后，稳一稳，准备确认

    // 4. 第一次长按确认 (修正：电平和延时重新对齐)
    Serial.println(">>> 4. 第一次长按确认输入的步数...");
    digitalWrite(STEP_PIN, HIGH); // 高电平：按下
    safeDelay(2500);              // 长按 2.5 秒触发确认
    digitalWrite(STEP_PIN, LOW);  // 低电平：松开
    
    // 【关键等待】松开后，泵会“滴滴滴”把刚才输入的步数回放一遍，必须等它完全响完！
    safeDelay(steps * 500);

    // 5. 第二次长按执行输注 (修正：电平和延时重新对齐)
    Serial.println(">>> 5. 第二次长按最终执行输注...");
    digitalWrite(STEP_PIN, HIGH); // 高电平：按下
    safeDelay(2500);              // 长按 2.5 秒触发输注
    digitalWrite(STEP_PIN, LOW);  // 低电平：松开（彻底完成）
    
    Serial.println(">>> [SUCCESS] 声音大剂量物理时序模拟完毕！");

finish:
    // 确保 GPIO 安全: LOW = 断开, 恢复高驱动输出模式
    digitalWrite(STEP_PIN, LOW);
    pinMode(STEP_PIN, OUTPUT_H0H1);

    // 上锁标记任务结束
    xSemaphoreTake(simulator->xSemaphore, portMAX_DELAY);
    simulator->isDeliveryTaskRunning = false;
    xSemaphoreGive(simulator->xSemaphore);

    vTaskDelete(NULL);
}


#undef CHECK_TIMEOUT

} // namespace M640GKit

#endif // M640G_PUMP_SIMULATOR_H
