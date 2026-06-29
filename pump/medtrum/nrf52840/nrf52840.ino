/*
================================================================================
M640GKit 泵模拟器 - nRF52840 (Nice!Nano) Arduino 入口文件
================================================================================

该程序模拟 M640G 胰岛素泵, 作为 BLE GATT Server 运行,
供 iOS Loop app / Trio 或其他 BLE 客户端连接和通信。

移植自 ESP32/ESP32.ino, 业务逻辑零改动 (见 pump_simulator.h)。
与 ESP32 版的差异 (仅入口文件层面):
  - 删除 esp_reset_reason() / ESP.getFreeHeap(): ESP32 专属 API
  - 新增 InternalFS.begin(): Adafruit nRF52 的 LittleFS 持久化初始化
    (替代 ESP32 的 NVS/Preferences)
  - LED 极性: Nice!Nano 板载 LED 是低电平点亮 (ESP32 多为高电平)
  - STEP_PIN: 默认用 P0.17 (Nice!Nano 引脚号 17, D2 排针)

硬件要求:
  - Nice!Nano v2 (nRF52840, Pro Micro 兼容引脚)
  - Arduino IDE + Adafruit nRF52 板卡包 (Board Manager 安装)
  -// 选板: "PCA10056 nRF52840 DK" (引脚 1:1 映射, P0.x = x, P1.x = 32+x)
// 不要选 "Feather nRF52840 Express" — Feather 变体的 g_ADigitalPinMap 不是 1:1,
// 会导致 STEP_PIN=17 实际操作 P0.28 而非 P0.17, 三极管继电器电路无法驱动。

使用方法:
  1. Arduino IDE -> 工具 -> 开发板 -> Adafruit nRF52 -> 选对应板
  2. 工具 -> SoftDevice -> S140
  3. 工具 -> 串口 -> 选对应 COM 口
  4. 双击 RST 进入 UF2 bootloader 后上传, 或直接上传
  5. 串口监视器 (115200 baud)

对应 ESP32: ESP32.ino
================================================================================
*/

#include "pump_simulator.h"

// 使用 M640GKit 命名空间
using namespace M640GKit;

// Adafruit nRF52 用 -fno-exceptions 编译, 标准库 <vector> 引用了
// std::__throw_length_error 但没有实现, 链接时报 undefined reference。
// 提供空实现: 实际触发时挂起 (嵌入式无异常支持, 不应到达此处)。
namespace std {
    void __throw_length_error(const char*) { while (1) delay(1); }
}

// ========== 引脚定义 ==========
// STEP_PIN: 控制 TS5A3166 模拟开关, 高电平导通 = 按下泵按键。
// 实际常量定义在 pump_simulator.h 内 (static constexpr int STEP_PIN = 17),
// 指向 Nice!Nano 的 P0.17 (D2 排针引脚)。
// 如你接在其他引脚, 改 pump_simulator.h 的 STEP_PIN 即可。
// (Adafruit nRF52 引脚号映射: P0.x = x; P1.x = 32 + x)

// LED 引脚: Nice!Nano 板载蓝色 LED 在 P0.15, Adafruit nRF52 核心已定义为 LED_BUILTIN。
// 板载 LED 为低电平点亮 (active-low), 与 ESP32 相反。
#define LED_BUILTIN 15  // Nice!Nano: LED = P0.15

// 全局模拟器实例
M640GPumpSimulator pumpSimulator;

void setup() {
    // 初始化串口 (PCA10056 默认走 USB CDC 或 UART0, 取决于板卡包配置)
    Serial.begin(115200);

    // 等待 USB CDC 枚举完成, 否则 setup 日志会丢失
    uint32_t usbWaitStart = millis();
    while (!Serial && (millis() - usbWaitStart < 5000)) {
        delay(10);
    }

    Serial.println("nRF52840 starting...");
    Serial.println("Version: 1.0.0-nrf52840");
    Serial.flush();

    // ESP32 版有 esp_reset_reason(); nRF52840 可读 NRF_POWER->RESETREAS。
    // 这里打印复位原因寄存器 (位掩码), 便于诊断。
    Serial.print("Reset reason (NRF_POWER->RESETREAS raw): 0x");
    Serial.println(NRF_POWER->RESETREAS, HEX);
    // 清除复位原因标志 (nRF52 的特性: 该寄存器需手动清, 否则跨复位保留)
    NRF_POWER->RESETREAS = 1;  // 写任意值清除

    Serial.println("\n========================================");
    Serial.println("  M640GKit 泵模拟器 (Nice!Nano nRF52840)");
    Serial.println("========================================");
    Serial.println("正在初始化...");
    Serial.flush();

    // ---------- 持久化已禁用 ----------
    // nRF52840 LittleFS 在运行时写 flash 会触发断言崩溃 (pcache->block == 0xffffffff)。
    // preferences_nrf52.h 的 writeFile/readFile 已改为空操作, 不需要 InternalFS。
    // 跳过 InternalFS.begin() 避免挂载损坏的文件系统导致崩溃重启。
    Serial.println("Persistence disabled (LittleFS bypassed)");

    // ---------- 初始化 LED (Nice!Nano 板载 LED 为低电平点亮) ----------
    Serial.print("Initializing LED on pin ");
    Serial.println(LED_BUILTIN);
    // 板载 LED 低电平点亮: 初始设 HIGH = 灭灯
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // HIGH = 灭 (active-low)

    // ---------- 初始化 STEP_PIN (TS5A3166 控制, 高电平导通) ----------
    // STEP_PIN 定义在 pump_simulator.h (= 17 = P0.17 = D2 on Nice!Nano)。
    // D2 是 Nice!Nano 边缘排针上的引脚, 方便接线。
    // 必须先 pinMode(OUTPUT) 再 digitalWrite(LOW),
    // 确保 pinMode 切换瞬间输出寄存器默认 LOW, 开关不会误触发。
    // OUTPUT_H0H1 = 高驱动强度 (~5mA), 足以驱动三极管基极饱和导通。
    // 默认 OUTPUT (S0S1 ~0.5mA) 只能驱动 TS5A3166 CMOS 输入, 无法驱动三极管+继电器。
    pinMode(STEP_PIN, OUTPUT_H0H1);
    digitalWrite(STEP_PIN, LOW);  // TS5A3166 / 三极管: LOW = 断开
    Serial.print("STEP_PIN (P0.17=");
    Serial.print(STEP_PIN);
    Serial.println(") initialized as OUTPUT_H0H1 (high-drive), active-high, default LOW)");

    // 设置全局实例指针 (用于回调)
    gSimulator = &pumpSimulator;

    // 初始化模拟器 (内部会调 gattServer.start() 启动 BLE)
    Serial.println("Starting pump simulator setup...");
    pumpSimulator.setup();

    Serial.println("初始化完成!");
    Serial.println("LED will blink fast when advertising, slow when connected");

    // 打印内存信息 (替代 ESP32 的 ESP.getFreeHeap)
    // Adafruit nRF52 的 FreeRTOS 未导出 xPortGetFreeHeapSize, 此处仅打印提示
    Serial.println("Free heap: see FreeRTOS stats (xPortGetFreeHeapSize unavailable on nRF52)");

    Serial.println("========================================\n");
    Serial.flush();
}

void loop() {
    // 运行模拟器主循环
    pumpSimulator.loop();
    delay(5); // <--- 【关键】给系统底层调度器让出 5ms

    // LED 指示: 未连接时快闪, 已连接时常灭
    static uint32_t lastLedToggle = 0;
    static bool ledOn = false;
    uint32_t now = millis();

    if (pumpSimulator.getIsConnected()) {
        // 已连接: LED 常灭
        if (ledOn) {
            ledOn = false;
            digitalWrite(LED_BUILTIN, HIGH);  // HIGH = 灭
        }
    } else {
        // 未连接: 快闪 (亮200ms, 灭200ms)
        uint32_t interval = ledOn ? 200 : 200;
        if (now - lastLedToggle >= interval) {
            lastLedToggle = now;
            ledOn = !ledOn;
            digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);  // LOW=亮, HIGH=灭
        }
    }
}
