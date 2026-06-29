# M640GKit 泵模拟器 — nRF52840 (PCA10056) 移植版

本目录是把 `ESP32/` 版本的胰岛素泵模拟器移植到 **Nordic nRF52840 DK (PCA10056)** 的产物,使用 Adafruit nRF52 板卡包 + 纯 Arduino IDE sketch。

## 1. 移植策略:平台适配层

**业务逻辑零改动复用** —— `pump_simulator.h` 的状态机、队列调度、carryOver 2U 安全上限、GPIO 敲击时序、时间模型全部保持原样。只在 4 个硬件耦合点做了平台替换:

| 耦合点 | ESP32 | nRF52840 (本目录) |
|--------|-------|-------------------|
| BLE 栈 | `<BLEDevice.h>` (Arduino-ESP32 BLE) | `<bluefruit.h>` (Adafruit Bluefruit) → 重写 `gatt_server.h` |
| 持久化 | `Preferences` (NVS) | `preferences_nrf52.h`:LittleFS(InternalFS) 包装类,同名同 API |
| FreeRTOS | `<freertos/semphr.h>` | 路径 + 栈大小 8192→4096,API 不变 |
| MAC 地址 | `esp_efuse_mac` 软件设置 | 删除——nRF52840 MAC 由 SoftDevice 器件 ID 决定 |
| GPIO 引脚 | STEP_PIN=7 (GPIO7) | STEP_PIN=17 (P0.17, Nice!Nano D2) |
| LED | 高电平点亮 | 低电平点亮(nRF52840 DK 板载 LED active-low) |

## 2. 文件清单

| 文件 | 来源 | 说明 |
|------|------|------|
| `nrf52840.ino` | 改自 `ESP32.ino` | 入口,删 esp_reset/ESP.getFreeHeap,加 InternalFS 初始化 |
| `pump_simulator.h` | 改自 ESP32 版 | 3 处改动:Preferences include / FreeRTOS include / STEP_PIN / 栈大小 |
| `gatt_server.h` | **重写** | Adafruit Bluefruit,公共 API 与 ESP32 版完全一致 |
| `preferences_nrf52.h` | **新建** | LittleFS 实现的 Preferences 兼容类 |
| `crypto.h` `crc8.h` `enums.h` | 逐字复制 | 纯软件加密/校验/常量 |
| `*_packet.h` (9 个) | 逐字复制 | 纯协议层字节操作 |
| `connection_tracker.h` | 逐字复制 | 纯 millis 连接跟踪 |
| `test_*.h` (3 个) | 逐字复制 | 测试代码 |

## 3. 硬件接线

### TS5A3166 模拟开关 / 三极管继电器 → Nice!Nano (nRF52840)
```
TS5A3166 / 三极管基极    Nice!Nano
IN (控制) / Base   →  P0.17  (STEP_PIN = 17, D2 排针)
VCC / 继电器线圈    →  外部电源
GND / Emitter      →  GND
NO/COM   →  并联到 M640G 泵按键两端
```
- TS5A3166 是**高电平导通**(IN=HIGH 时按下泵按键), 三极管继电器同理。
- **P0.17 选择原因**: Nice!Nano 的 D2 排针引脚, 方便接线, 无硬件冲突。
- 若你接其他引脚: 改 `pump_simulator.h` 第 147 行 `static constexpr int STEP_PIN = 17;` 即可。
- **重要**: Arduino IDE 选板必须选 **"PCA10056 nRF52840 DK"** (1:1 引脚映射, P0.x = x)。
  不要选 "Feather nRF52840 Express" — 其 g_ADigitalPinMap 非 1:1, 会导致 STEP_PIN=17 实际操作 P0.28 而非 P0.17!

### nRF52840 DK 供电
- USB 接 micro-USB(板载 SEGGER J-Link),或外部 3.3V。

## 4. 开发环境搭建

### 4.1 安装 Adafruit nRF52 板卡包
1. Arduino IDE → 文件 → 首选项 → 附加开发板管理器网址,加:
   ```
   https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
   ```
2. 工具 → 开发板 → 开发板管理器 → 搜索 `Adafruit nRF52` → 安装(≥1.x)。

### 4.2 板卡配置
- 开发板:**nRF52840 DK (PCA10056)**
- SoftDevice:**S140**
- 优化:-Os
- 串口:选对应 COM 口(USB CDC)

### 4.3 库依赖
**无需额外库**——Adafruit nRF52 板卡包自带:
- `bluefruit.h`(BLE)
- `InternalFS` / `Adafruit_LittleFS`(持久化)
- FreeRTOS(任务/信号量)
- crypto/crc 都是手写软件,已在目录内。

## 5. 编译与烧录

1. Arduino IDE 打开 `nrf52840.ino`(会自动连同同目录 .h 一起编译)。
2. 选板 + COM 口。
3. 点上传(↓)。PCA10056 经 J-Link 烧录。

## 6. 与 ESP32 版的关键差异(必读)

### 6.1 MAC 地址变了 → Trio 要重新配对一次
- ESP32 版用 `esp_base_mac_addr_set` 设固定 MAC;nRF52840 的 MAC 来自 SoftDevice 器件 ID,**不可软件改**。
- 后果:Trio 之前缓存的 peripheral UUID 失效,**首次需重新扫描配对一次**。之后跨重启稳定(nRF52840 MAC 比 ESP32 更可靠)。

### 6.2 LED 极性相反
- ESP32:HIGH=亮。nRF52840 DK:LOW=亮(板载 LED active-low)。
- `nrf52840.ino` 的 loop() 已处理,无需手动改。

### 6.3 Flash 磨损
- `persistStats()` 每次注射后写 ~48 字节到 LittleFS。正常使用(几十次/天)无忧。
- 若注射密度极高(>100次/天),长期运行需评估 flash 寿命。可后续优化为 dirty-flag 延迟批量写。

### 6.4 FreeRTOS 栈
- GPIO 任务栈 8192→4096(nRF52840 RAM 更紧张)。任务体只有 digitalWrite+delay,4096 充裕。

## 7. 分阶段验证清单

### 阶段 1 — 编译通过
- [ ] 全部 .h/.ino 无编译错误
- [ ] FreeRTOS include 路径正确(`<freertos/FreeRTOS.h>` + `<freertos/semphr.h>`)

### 阶段 2 — 上电 + BLE 广播
- [ ] 串口(115200)看到 `[GATT] GATT Server is now running!` 和 `[ADV] BLE advertising is now active!`
- [ ] 用 **nRF Connect** 或 **LightBlue** 扫描,看到设备名 `MT`
- [ ] 厂商数据 = `59 6A 65 D1 79 98 01 01`(与 ESP32 版一字不差)
- [ ] LED 心跳:广播时快闪,连接后慢闪

### 阶段 3 — Trio 配对
- [ ] Trio 首次配对成功(因 MAC 变了,**必须重新配对一次**)
- [ ] 串口看到 `[BLE] CLIENT CONNECTED!`

### 阶段 4 — 持久化恢复
- [ ] set time / activate / prime 流程走通
- [ ] 断电重启,串口看到 reservoir / stepCarryOver / hourlyDelivered / dailyDelivered 被正确恢复
- [ ] 验证 stepCarryOver 被钳到 [0, 2.0]

### 阶段 5 — GPIO 敲击(接泵)
- [ ] set bolus 1U,串口看到 `[DEBUG] deliverAmount=1.00U, stepCarryOver=0.00U`
- [ ] STEP_PIN 实际驱动 TS5A3166 完成泵的声音大剂量序列(进入菜单→输入步数→确认)
- [ ] 多次 set bolus,验证 carryOver 累积与 2U 封顶日志(`[安全] ...丢弃超额`)

### 阶段 6 — 一致性对账
- [ ] 跑一天,对比:
  - `Σ(日志 deliverAmount)`
  - 储药器下降量
  - GPIO 物理步数 × 0.5
  - 三者应**完全一致**(carryOver 2U 上限逻辑保证)

## 8. 故障排查

| 现象 | 可能原因 | 处理 |
|------|---------|------|
| 编译报 `freertos/semphr.h` 找不到 | 板卡包未选 Adafruit nRF52 | 确认开发板选了 PCA10056 |
| `[PREFS] ERROR 写入失败` | InternalFS 未初始化 | 确认 `setup()` 里有 `InternalFS.begin()` |
| Trio 连不上 | MAC 变了 | Trio 删除旧设备重新配对 |
| 泵不响应按键 | STEP_PIN 引脚错 / TS5A3166 接线 | 确认 P1.01 实际接线 |
| LED 不闪 | LED_BUILTIN 定义 / 极性 | 确认 Adafruit 核心版本,LED active-low |

## 9. 与 ESP32 版对照

代码 diff 时,业务逻辑(pump_simulator.h 的状态机/队列/时间模型)应**零差异**。只有这几处平台相关改动:
- `pump_simulator.h`:Preferences include(L26-29)/ FreeRTOS include(L42-45)/ STEP_PIN(L144)/ 栈大小(L1286)
- `gatt_server.h`:整文件重写(Adafruit Bluefruit)
- `preferences_nrf52.h`:新建
- `nrf52840.ino`:改自 `ESP32.ino`
