# Medtrum Nano 蓝牙通信协议文档

## 目录

1. [概述](#1-概述)
2. [BLE 服务与特征值](#2-ble-服务与特征值)
3. [通用数据格式](#3-通用数据格式)
4. [数据包结构](#4-数据包结构)
5. [连接流程](#5-连接流程)
6. [命令详解](#6-命令详解)
   - [6.1 Synchronize (同步)](#61-synchronize-同步-commandtype--3)
   - [6.2 Subscribe (订阅通知)](#62-subscribe-订阅通知-commandtype--4)
   - [6.3 Authorize (授权)](#63-authorize-授权-commandtype--5)
   - [6.4 Set Time (设置时间)](#64-set-time-设置时间-commandtype--10)
   - [6.5 Get Time (获取时间)](#65-get-time-获取时间-commandtype--11)
   - [6.6 Set Time Zone (设置时区)](#66-set-time-zone-设置时区-commandtype--12)
   - [6.7 Prime (灌注)](#67-prime-灌注-commandtype--16)
   - [6.8 Activate (激活)](#68-activate-激活-commandtype--18)
   - [6.9 Set Bolus (设置大剂量)](#69-set-bolus-设置大剂量-commandtype--19)
   - [6.10 Cancel Bolus (取消大剂量)](#610-cancel-bolus-取消大剂量-commandtype--20)
   - [6.11 Set Basal Profile (设置基础率配置)](#611-set-basal-profile-设置基础率配置-commandtype--21)
   - [6.12 Set Temp Basal (设置临时基础率)](#612-set-temp-basal-设置临时基础率-commandtype--24)
   - [6.13 Cancel Temp Basal (取消临时基础率)](#613-cancel-temp-basal-取消临时基础率-commandtype--25)
   - [6.14 Suspend Pump (暂停输注)](#614-suspend-pump-暂停输注-commandtype--28)
   - [6.15 Resume Pump (恢复输注)](#615-resume-pump-恢复输注-commandtype--29)
   - [6.16 Stop Patch (停用贴片)](#616-stop-patch-停用贴片-commandtype--31)
   - [6.17 Set Patch (设置贴片参数)](#617-set-patch-设置贴片参数-commandtype--35)
   - [6.18 Clear Alarm (清除警报)](#618-clear-alarm-清除警报-commandtype--115)
   - [6.19 Notification (通知/心跳)](#619-notification-通知心跳)
7. [枚举定义](#7-枚举定义)
8. [加密与校验](#8-加密与校验)

---

## 1. 概述

Medtrum Nano 胰岛素泵通过 BLE (Bluetooth Low Energy) 与手机通信。本文档详细描述了所有蓝牙通信的请求和响应数据包结构。

**时间基准**: Medtrum 使用自定义时间基准，以 2014-01-01 00:00:00 UTC 为起点（Unix 时间戳偏移量 1,388,534,400 秒）。

**胰岛素单位**: 所有胰岛素剂量值以 0.05U 为最小单位存储。传输时使用整数表示，实际值 = 整数值 × 0.05。

---

## 2. BLE 服务与特征值

| 名称 | UUID |
|------|------|
| **Service UUID** | `669A9001-0008-968F-E311-6050405558B3` |
| **Read/Notify UUID** | `669a9120-0008-968f-e311-6050405558b3` |
| **Write UUID** | `669a9101-0008-968f-e311-6050405558b3` |

- **Write 特征值**: 用于发送命令（带响应写入 `withResponse`）
- **Read/Notify 特征值**: 用于接收响应和通知（支持 Notify/Indicate）

**扫描过滤**: 广播名称（Local Name）为 `"MT"`，厂商数据（Manufacturer Data）包含设备信息。

---

## 3. 通用数据格式

### 3.1 整数编码（Little-Endian）

所有多字节整数使用 **Little-Endian**（小端）字节序。即低位字节在前。

例如：`0x1234` 编码为 `[0x34, 0x12]`

### 3.2 时间编码

Medtrum 时间 = Unix 时间戳 - 1,388,534,400（即从 2014-01-01 开始的秒数），编码为 4 字节 Little-Endian。

### 3.3 胰岛素剂量编码

胰岛素剂量值除以 0.05 后取整，编码为整数传输。

| 实际值 | 编码值 |
|--------|--------|
| 0.05 U | 1 |
| 1.0 U | 20 |
| 10.0 U | 200 |

### 3.4 CRC8 校验

每个数据包末尾包含 1 字节 CRC8 校验值，使用自定义查找表算法。

---

## 4. 数据包结构

### 4.1 请求数据包格式

```
[Header: 4 bytes] + [Content: variable] + [CRC8: 1 byte] + [Padding: 1 byte (0x00)]
```

**Header 格式**:

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 数据大小（dataSize）= Content 长度 + 5 |
| 1 | 1 | 命令类型（CommandType） |
| 2 | 1 | 序列号（SequenceNumber） |
| 3 | 1 | 包索引（pkgIndex），0 表示唯一包或首包 |

**多包传输**: 当 Content 长度超过 15 字节时，数据包会被拆分为多个 BLE 包发送。每个包的 Header 中 `pkgIndex` 从 1 开始递增，最后一个包的 `pkgIndex` 标记为最终包。

### 4.2 响应数据包格式

```
[Header: 4 bytes] + [ResponseCode: 2 bytes] + [Payload: variable] + [CRC8: 1 byte]
```

**Header 格式**:

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 数据大小（dataSize） |
| 1 | 1 | 命令类型（必须与请求一致） |
| 2 | 1 | 序列号 |
| 3 | 1 | 包索引 |

**ResponseCode**（偏移 4-5）:

| 值 | 描述 |
|----|------|
| 0 | 成功 |
| 7 | 授权无效（Session Token 错误） |
| 8 | 状态无效（贴片不在正确状态） |
| 16384 | 需要跳过的消息 |

### 4.3 通知/心跳数据包

贴片会定期发送通知（Notification），格式与响应类似但无末尾 CRC8 字节。通知使用 `CommandType.SYNCHRONIZE`（0x03）命令类型，包含贴片状态更新。

---

## 5. 连接流程

```
手机 -> 贴片: Authorize (授权)
贴片 -> 手机: AuthorizeResponse (设备类型、软件版本)
手机 -> 贴片: Synchronize (同步)
贴片 -> 手机: SynchronizePacketResponse (贴片完整状态)
手机 -> 贴片: Subscribe (订阅通知)
贴片 -> 手机: SubscribePacketResponse (确认)
```

连接建立后，贴片会定期发送通知（心跳），手机每 3 分钟执行一次完整同步。

---

## 6. 命令详解

### 6.1 Synchronize (同步) - CommandType = 3

用于获取贴片的完整状态信息。在连接建立后和定期同步时使用。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 0 | 空 | 无请求内容 |

**请求字节**: 空 Data

#### Response: SynchronizePacketResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6 | 1 | state | [PatchState](#71-patchstate) 枚举值 |
| 7-8 | 2 | fieldMask | 位掩码，指示后续包含哪些数据字段 |

**fieldMask 位定义**:

| 位 | 值 | 掩码名 | 描述 |
|----|-----|--------|------|
| 0 | 0x0001 | MASK_SUSPEND | 暂停时间 |
| 1 | 0x0002 | MASK_NORMAL_BOLUS | 普通大剂量 |
| 2 | 0x0004 | MASK_EXTENDED_BOLUS | 扩展大剂量 |
| 3 | 0x0008 | MASK_BASAL | 基础率 |
| 4 | 0x0010 | MASK_SETUP | 设置进度（灌注进度） |
| 5 | 0x0020 | MASK_RESERVOIR | 储药器余量 |
| 6 | 0x0040 | MASK_START_TIME | 启动时间 |
| 7 | 0x0080 | MASK_BATTERY | 电池电压 |
| 8 | 0x0100 | MASK_STORAGE | 存储（序列号/贴片ID） |
| 9 | 0x0200 | MASK_ALARM | 警报状态 |
| 10 | 0x0400 | MASK_AGE | 贴片使用时长 |
| 11 | 0x0800 | MASK_MAGNETO_PLACE | 磁力值 |
| 12 | 0x1000 | MASK_UNUSED_CGM | 未使用（CGM相关，5字节） |
| 13 | 0x2000 | MASK_UNUSED_COMMAND_CONFIRM | 未使用（命令确认，2字节） |
| 14 | 0x4000 | MASK_UNUSED_AUTO_STATUS | 未使用（自动状态，2字节） |
| 15 | 0x8000 | MASK_UNUSED_LEGACY | 未使用（遗留，2字节） |

根据 fieldMask 中设置的位，后续数据按顺序排列。每个掩码对应的数据解析如下：

**MASK_SUSPEND (0x0001)** - 4 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 4 | 暂停时间（Medtrum 时间戳） |

**MASK_NORMAL_BOLUS (0x0002)** - 3 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 类型（bit 0-6）和完成标志（bit 7） |
| 1-2 | 2 | 已输注量（× 0.05 U） |

**MASK_EXTENDED_BOLUS (0x0004)** - 3 字节（被忽略）

**MASK_BASAL (0x0008)** - 12 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 基础率类型（[BasalType](#72-basaltype)） |
| 1-2 | 2 | 序列号 |
| 3-4 | 2 | 贴片ID |
| 5-8 | 4 | 开始时间（Medtrum 时间戳） |
| 9-11 | 3 | 高12位 = 已输注量（× 0.05），低12位 = 速率（× 0.05 U/h） |

**MASK_SETUP (0x0010)** - 1 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 灌注进度 |

**MASK_RESERVOIR (0x0020)** - 2 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-1 | 2 | 储药器余量（× 0.05 U） |

**MASK_START_TIME (0x0040)** - 4 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-3 | 4 | 启动时间（Medtrum 时间戳） |

**MASK_BATTERY (0x0080)** - 3 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-2 | 3 | 低12位 = 电压A / 512，高12位 = 电压B / 512 |

**MASK_STORAGE (0x0100)** - 4 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-1 | 2 | 序列号 |
| 2-3 | 2 | 贴片ID |

**MASK_ALARM (0x0200)** - 4 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-1 | 2 | 警报标志位（[AlarmState](#73-alarmstate)） |
| 2-3 | 2 | 未使用参数 |

**MASK_AGE (0x0400)** - 4 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-3 | 4 | 贴片使用时长（秒） |

**MASK_MAGNETO_PLACE (0x0800)** - 2 字节

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-1 | 2 | 磁力值 |

#### Response 结构体定义

```swift
struct SynchronizePacketResponse {
    let state: PatchState           // 贴片状态
    var suspendTime: Date?          // 暂停时间
    var bolus: BolusData?           // 大剂量数据
    var basal: BasalData?           // 基础率数据
    var primeProgress: UInt8?       // 灌注进度
    var reservoir: Double?          // 储药器余量 (U)
    var startTime: Date?            // 启动时间
    var battery: BatteryData?       // 电池数据
    var storage: StorageData?       // 存储数据
    var activeAlarms: [AlarmState]  // 活跃警报列表
    var patchAge: UInt64?           // 贴片使用时长 (秒)
    var magnetoPlacement: Double?   // 磁力值
}

struct BolusData {
    let type: UInt8                 // 大剂量类型
    let completed: Bool             // 是否完成
    let delivered: Double           // 已输注量 (U)
}

struct BasalData {
    let type: BasalType             // 基础率类型
    let sequence: Double            // 序列号
    let patchId: Double             // 贴片ID
    let startTime: Date             // 开始时间
    let rate: Double                // 速率 (U/h)
    let delivery: Double            // 已输注量 (U)
}

struct BatteryData {
    let voltageA: Double            // 电压A
    let voltageB: Double            // 电压B
}

struct StorageData {
    let sequence: Double            // 序列号
    let patchId: Double             // 贴片ID
}
```

---

### 6.2 Subscribe (订阅通知) - CommandType = 4

用于订阅贴片的通知。连接流程的最后一步。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 2 | 0xFF 0x0F | 订阅掩码（4095 = 0x0FFF） |

**请求字节**: `[0xFF, 0x0F]`

#### Response: SubscribePacketResponse

空响应结构体，无额外数据。

---

### 6.3 Authorize (授权) - CommandType = 5

用于与贴片建立安全连接，交换会话令牌和加密密钥。

#### Request

| 字段 | 大小 | 描述 |
|------|------|------|
| role | 1 | 角色（固定值 2） |
| sessionToken | 4 | 会话令牌（随机生成） |
| key | 4 | 加密密钥（由泵序列号通过 Crypto.genKey 生成） |

**请求字节**: `[role(1)] + [sessionToken(4)] + [key(4)]`

**密钥生成算法**:
```
key = simpleCrypt(randomGen(randomGen(MEDTRUM_CIPHER ^ pumpSN)))
其中 MEDTRUM_CIPHER = 1,344,751,489
pumpSN 需要反转字节序后使用
```

#### Response: AuthorizeResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 7 | 1 | deviceType | 设备类型 |
| 8 | 1 | swVersionMajor | 软件版本主号 |
| 9 | 1 | swVersionMinor | 软件版本次号 |
| 10 | 1 | swVersionPatch | 软件版本补丁号 |

```swift
struct AuthorizeResponse {
    let deviceType: UInt8           // 设备类型
    let swVersion: String           // 软件版本 (格式: "主.次.补丁")
}
```

---

### 6.4 Set Time (设置时间) - CommandType = 10

设置贴片的当前时间。

#### Request

| 字段 | 大小 | 描述 |
|------|------|------|
| type | 1 | 类型（固定值 2） |
| time | 4 | Medtrum 时间戳 |

**请求字节**: `[0x02] + [time(4)]`

#### Response: SetTimePacketResponse

空响应结构体。

---

### 6.5 Get Time (获取时间) - CommandType = 11

获取贴片的当前时间。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 0 | 空 | 无请求内容 |

**请求字节**: 空 Data

#### Response: GetTimePacketResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6-9 | 4 | time | Medtrum 时间戳 |

```swift
struct GetTimePacketResponse {
    let time: Date                  // 贴片当前时间
}
```

---

### 6.6 Set Time Zone (设置时区) - CommandType = 12

设置贴片的时区偏移量。

#### Request

| 字段 | 大小 | 描述 |
|------|------|------|
| offset | 2 | 时区偏移量（分钟），Little-Endian。负值加 65536 |
| time | 4 | Medtrum 时间戳 |

**时区偏移处理**:
- 如果偏移 > 12 小时（720 分钟），减去 24 小时
- 如果偏移 < 0，加上 65536

**请求字节**: `[offset(2)] + [time(4)]`

#### Response: SetTimeZonePacketResponse

空响应结构体。

---

### 6.7 Prime (灌注) - CommandType = 16

启动贴片灌注流程。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 0 | 空 | 无请求内容 |

**请求字节**: 空 Data

#### Response: PrimePacketResponse

空响应结构体。

---

### 6.8 Activate (激活) - CommandType = 18

激活贴片，设置运行参数和基础率配置。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | autoSuspendEnable | 自动暂停启用（固定 0） |
| 1 | 1 | autoSuspendTime | 自动暂停时间（固定 12） |
| 2 | 1 | expirationTimer | 到期定时器（0=延长模式, 1=默认模式） |
| 3 | 1 | alarmSetting | [AlarmSettings](#74-alarmsettings) 枚举值 |
| 4 | 1 | lowSuspend | 低血糖暂停（固定 0） |
| 5 | 1 | predictiveLowSuspend | 预测低血糖暂停（固定 0） |
| 6 | 1 | predictiveLowSuspendRange | 预测低血糖范围（固定 30） |
| 7-8 | 2 | hourlyMaxInsulin | 每小时最大胰岛素量（÷ 0.05） |
| 9-10 | 2 | dailyMaxInsulin | 每天最大胰岛素量（÷ 0.05） |
| 11-12 | 2 | currentTDD | 今日总剂量（÷ 0.05） |
| 13 | 1 | 固定值 1 | 始终为 1 |
| 14+ | 可变 | basalProfile | 基础率配置数据 |

**基础率配置数据格式**:

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 条目数量（N） |
| 1-3 | 3×N | 每个条目：高12位=速率（÷0.05），低12位=开始时间（分钟） |

**请求字节**: `[7字节固定参数] + [hourlyMaxInsulin(2)] + [dailyMaxInsulin(2)] + [currentTDD(2)] + [0x01] + [basalProfile]`

#### Response: ActivatePacketResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6-9 | 4 | patchId | 贴片ID |
| 10-13 | 4 | time | 激活时间（Medtrum 时间戳） |
| 14 | 1 | basalType | 基础率类型（[BasalType](#72-basaltype)） |
| 15-16 | 2 | basalValue | 基础率值（× 0.05 U/h） |
| 17-18 | 2 | basalSequence | 基础率序列号 |
| 19-20 | 2 | basalPatchId | 基础率贴片ID |
| 21-24 | 4 | basalStartTime | 基础率开始时间（Medtrum 时间戳） |

```swift
struct ActivatePacketResponse {
    let patchId: Data               // 贴片ID (4字节)
    let time: Date                  // 激活时间
    let basalType: BasalType        // 基础率类型
    let basalValue: Double          // 基础率值 (U/h)
    let basalSequence: Double       // 基础率序列号
    let basalPatchId: Double        // 基础率贴片ID
    let basalStartTime: Date        // 基础率开始时间
}
```

---

### 6.9 Set Bolus (设置大剂量) - CommandType = 19

设置大剂量输注。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | bolusType | 大剂量类型（1=普通, 2=扩展, 3=组合） |
| 1-2 | 2 | bolusAmount | 大剂量量（÷ 0.05 U） |
| 3 | 1 | 固定值 0 | 保留 |

**请求字节**: `[bolusType(1)] + [bolusAmount(2)] + [0x00]`

#### Response: SetBolusResponse

空响应结构体。

---

### 6.10 Cancel Bolus (取消大剂量) - CommandType = 20

取消正在进行的大剂量输注。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | bolusType | 大剂量类型（1=普通） |

**请求字节**: `[0x01]`

#### Response: CancelBolusPacketResponse

空响应结构体。

---

### 6.11 Set Basal Profile (设置基础率配置) - CommandType = 21

设置基础率配置文件。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | basalType | 基础率类型（固定 1 = 标准） |
| 1+ | 可变 | basalProfile | 基础率配置数据 |

**基础率配置数据格式**:

| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 条目数量（N） |
| 1-3 | 3×N | 每个条目：高12位=速率（÷0.05），低12位=开始时间（分钟） |

**请求字节**: `[0x01] + [basalProfile]`

#### Response: SetBasalProfilePacketResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6 | 1 | basalType | 基础率类型（[BasalType](#72-basaltype)） |
| 7-8 | 2 | basalValue | 基础率值（× 0.05 U/h） |
| 9-10 | 2 | basalSequence | 基础率序列号 |
| 11-12 | 2 | basalPatchId | 基础率贴片ID |
| 13-16 | 4 | basalStartTime | 基础率开始时间（Medtrum 时间戳） |

```swift
struct SetBasalProfilePacketResponse {
    let basalType: BasalType        // 基础率类型
    let basalValue: Double          // 基础率值 (U/h)
    let basalSequence: Double       // 基础率序列号
    let basalPatchId: Double        // 基础率贴片ID
    let basalStartTime: Date        // 基础率开始时间
}
```

---

### 6.12 Set Temp Basal (设置临时基础率) - CommandType = 24

设置临时基础率。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | type | 类型（固定 6 = 临时基础率） |
| 1-2 | 2 | rate | 临时基础率值（÷ 0.05 U/h） |
| 3-4 | 2 | duration | 持续时间（分钟） |

**请求字节**: `[0x06] + [rate(2)] + [duration(2)]`

#### Response: SetTempBasalResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6 | 1 | basalType | 基础率类型（[BasalType](#72-basaltype)） |
| 7-8 | 2 | basalValue | 基础率值（× 0.05 U/h） |
| 9-10 | 2 | basalSequence | 基础率序列号 |
| 11-12 | 2 | basalPatchId | 基础率贴片ID |
| 13-16 | 4 | basalStartTime | 基础率开始时间（Medtrum 时间戳） |

```swift
struct SetTempBasalResponse {
    let basalType: BasalType        // 基础率类型
    let basalValue: Double          // 基础率值 (U/h)
    let basalSequence: Double       // 基础率序列号
    let basalPatchId: Double        // 基础率贴片ID
    let basalStartTime: Date        // 基础率开始时间
}
```

---

### 6.13 Cancel Temp Basal (取消临时基础率) - CommandType = 25

取消正在运行的临时基础率。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 0 | 空 | 无请求内容 |

**请求字节**: 空 Data

#### Response: CancelTempBasalPacketResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6 | 1 | basalType | 基础率类型（[BasalType](#72-basaltype)） |
| 7-8 | 2 | basalValue | 基础率值（× 0.05 U/h） |
| 9-10 | 2 | basalSequence | 基础率序列号 |
| 11-12 | 2 | basalPatchId | 基础率贴片ID |
| 13-16 | 4 | basalStartTime | 基础率开始时间（Medtrum 时间戳） |

```swift
struct CancelTempBasalPacketResponse {
    let basalType: BasalType        // 基础率类型
    let basalValue: Double          // 基础率值 (U/h)
    let basalSequence: Double       // 基础率序列号
    let basalPatchId: Double        // 基础率贴片ID
    let basalStartTime: Date        // 基础率开始时间
}
```

---

### 6.14 Suspend Pump (暂停输注) - CommandType = 28

暂停胰岛素输注。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | cause | 原因（固定值 3） |
| 1 | 1 | duration | 暂停持续时间（分钟） |

**请求字节**: `[0x03] + [duration(1)]`

#### Response: SuspendPumpResponse

空响应结构体。

---

### 6.15 Resume Pump (恢复输注) - CommandType = 29

恢复暂停的胰岛素输注。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 0 | 空 | 无请求内容 |

**请求字节**: 空 Data

#### Response: ResumePumpPacketResponse

空响应结构体。

---

### 6.16 Stop Patch (停用贴片) - CommandType = 31

停用当前贴片。

#### Request

| 字段 | 大小 | 值 | 描述 |
|------|------|-----|------|
| Content | 0 | 空 | 无请求内容 |

**请求字节**: 空 Data

#### Response: StopPatchResponse

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 6-7 | 2 | sequence | 序列号 |
| 8-9 | 2 | patchId | 贴片ID |

```swift
struct StopPatchResponse {
    let sequence: Double            // 序列号
    let patchId: Double             // 贴片ID
}
```

---

### 6.17 Set Patch (设置贴片参数) - CommandType = 35

更新贴片的运行参数（警报设置、胰岛素限制等）。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | alarmSetting | [AlarmSettings](#74-alarmsettings) 枚举值 |
| 1-2 | 2 | hourlyMaxInsulin | 每小时最大胰岛素量（÷ 0.05） |
| 3-4 | 2 | dailyMaxInsulin | 每天最大胰岛素量（÷ 0.05） |
| 5 | 1 | expirationTimer | 到期定时器（0=延长, 1=默认） |
| 6 | 1 | autoSuspendEnable | 自动暂停启用（固定 0） |
| 7 | 1 | autoSuspendTime | 自动暂停时间（固定 12） |
| 8 | 1 | lowSuspend | 低血糖暂停（固定 0） |
| 9 | 1 | predictiveLowSuspend | 预测低血糖暂停（固定 0） |
| 10 | 1 | predictiveLowSuspendRange | 预测低血糖范围（固定 30） |

**请求字节**: `[alarmSetting(1)] + [hourlyMaxInsulin(2)] + [dailyMaxInsulin(2)] + [expirationTimer(1)] + [autoSuspendEnable(1)] + [autoSuspendTime(1)] + [lowSuspend(1)] + [predictiveLowSuspend(1)] + [predictiveLowSuspendRange(1)]`

#### Response: SetPatchResponse

空响应结构体。

---

### 6.18 Clear Alarm (清除警报) - CommandType = 115

清除贴片的警报状态（每小时最大量/每天最大量警报）。

#### Request

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0-1 | 2 | alertType | 警报类型（4=每小时最大, 5=每天最大） |

**请求字节**: `[alertType(2)]`

#### Response: ClearAlertResponse

空响应结构体。

---

### 6.19 Notification (通知/心跳)

贴片定期发送的通知消息，使用 `CommandType.SYNCHRONIZE`（0x03）。通知数据包末尾没有 CRC8 字节（代码中通过追加 0x00 来适配解析逻辑）。

#### 数据格式

与 [Synchronize 响应](#61-synchronize-同步---commandtype--3) 格式相同，但偏移不同：

| 偏移 | 大小 | 字段 | 描述 |
|------|------|------|------|
| 0 | 1 | state | [PatchState](#71-patchstate) 枚举值 |
| 1-2 | 2 | fieldMask | 位掩码 |
| 3+ | 可变 | syncData | 根据 fieldMask 解析的数据 |

#### 处理逻辑

手机收到通知后：
1. 如果距离上次同步 < 2.5 分钟，仅解析通知数据更新状态
2. 如果正在大剂量输注中，仅解析通知数据更新状态
3. 否则，执行完整同步（发送 Synchronize 请求 + GetTime 请求）

---

## 7. 枚举定义

### 7.1 PatchState

贴片运行状态枚举。

| 值 | 名称 | 描述 |
|----|------|------|
| 0 | none | 无状态 |
| 1 | idle | 空闲 |
| 2 | filled | 已填充 |
| 3 | priming | 灌注中 |
| 4 | primed | 已灌注 |
| 5 | ejecting | 弹出中 |
| 6 | ejected | 已弹出 |
| 32 | active | 运行中 |
| 33 | active_alt | 运行中（备用） |
| 64 | lowBgSuspended | 低血糖暂停 |
| 65 | lowBgSuspended2 | 低血糖暂停2 |
| 66 | autoSuspended | 自动暂停 |
| 67 | hourlyMaxSuspended | 每小时最大量暂停 |
| 68 | dailyMaxSuspended | 每天最大量暂停 |
| 69 | suspended | 已暂停 |
| 70 | paused | 已暂停（手动） |
| 96 | occlusion | 堵管 |
| 97 | expired | 已过期 |
| 98 | reservoirEmpty | 储药器空 |
| 99 | patchFault | 贴片故障 |
| 100 | patchFaultd2 | 贴片故障2 |
| 101 | baseFault | 底座故障 |
| 102 | batteryOut | 电池耗尽 |
| 103 | noCalibration | 未校准 |
| 128 | stopped | 已停止 |

### 7.2 BasalType

基础率类型枚举。

| 名称 | 描述 |
|------|------|
| NONE | 无 |
| STANDARD | 标准 |
| EXERCISE | 运动 |
| HOLIDAY | 假日 |
| PROGRAM_A | 方案A |
| PROGRAM_B | 方案B |
| ABSOLUTE_TEMP | 绝对临时基础率 |
| RELATIVE_TEMP | 相对临时基础率 |
| PROGRAM_C | 方案C |
| PROGRAM_D | 方案D |
| SICK | 生病 |
| AUTO | 自动 |
| NEW | 新 |
| SUSPEND_LOW_GLUCOSE | 低血糖暂停 |
| SUSPEND_PREDICT_LOW_GLUCOSE | 预测低血糖暂停 |
| SUSPEND_AUTO | 自动暂停 |
| SUSPEND_MORE_THAN_MAX_PER_HOUR | 超过每小时最大量暂停 |
| SUSPEND_MORE_THAN_MAX_PER_DAY | 超过每天最大量暂停 |
| SUSPEND_MANUAL | 手动暂停 |
| SUSPEND_KEY_LOST | 信号丢失暂停 |
| STOP_OCCLUSION | 堵管停止 |
| STOP_EXPIRED | 过期停止 |
| STOP_EMPTY | 储药器空停止 |
| STOP_PATCH_FAULT | 贴片故障停止 |
| STOP_PATCH_FAULT2 | 贴片故障停止2 |
| STOP_BASE_FAULT | 底座故障停止 |
| STOP_DISCARD | 丢弃停止 |
| STOP_BATTERY_EMPTY | 电池耗尽停止 |
| STOP | 停止 |
| PAUSE_INTERRUPT | 暂停中断 |
| PRIME | 灌注 |
| AUTO_MODE_START | 自动模式开始 |
| AUTO_MODE_EXIT | 自动模式退出 |
| AUTO_MODE_TARGET_100 | 自动模式目标100 |
| AUTO_MODE_TARGET_110 | 自动模式目标110 |
| AUTO_MODE_TARGET_120 | 自动模式目标120 |
| AUTO_MODE_BREAKFAST | 自动模式早餐 |
| AUTO_MODE_LUNCH | 自动模式午餐 |
| AUTO_MODE_DINNER | 自动模式晚餐 |
| AUTO_MODE_SNACK | 自动模式加餐 |
| AUTO_MODE_EXERCISE_START | 自动模式运动开始 |
| AUTO_MODE_EXERCISE_EXIT | 自动模式运动结束 |

### 7.3 AlarmState

警报状态枚举。

| 值 | 名称 | 描述 |
|----|------|------|
| 0 | None | 无警报 |
| 1 | PumpLowBattery | 电池电量低 |
| 2 | PumpLowReservoir | 储药器余量低 |
| 4 | PumpExpiresSoon | 即将过期 |
| - | LowBgSuspended | 低血糖暂停（来自状态） |
| - | LowBgSuspended2 | 低血糖暂停2（来自状态） |
| - | AutoSuspended | 自动暂停（来自状态） |
| - | HourlyMaxSuspended | 每小时最大量暂停（来自状态） |
| - | DailyMaxSuspended | 每天最大量暂停（来自状态） |
| - | Suspended | 已暂停（来自状态） |
| - | Paused | 已暂停（来自状态） |
| - | Occlusion | 堵管（来自状态） |
| - | Expired | 已过期（来自状态） |
| - | ReservoirEmpty | 储药器空（来自状态） |
| - | PatchFault | 贴片故障（来自状态） |
| - | PatchFault2 | 贴片故障2（来自状态） |
| - | BaseFault | 底座故障（来自状态） |
| - | BatteryOut | 电池耗尽（来自状态） |
| - | NoCalibration | 未校准（来自状态） |

### 7.4 AlarmSettings

警报提醒方式枚举。

| 值 | 名称 | 描述 |
|----|------|------|
| 0 | LightVibrateAndBeep | 灯光+震动+声音 |
| 1 | LightAndVibrate | 灯光+震动 |
| 2 | LightAndBeep | 灯光+声音 |
| 3 | LightOnly | 仅灯光 |
| 4 | VibrateAndBeep | 震动+声音 |
| 5 | VibrateOnly | 仅震动 |
| 6 | BeepOnly | 仅声音 |
| 7 | None | 无提醒 |

### 7.5 AlertType (清除警报用)

| 值 | 名称 | 描述 |
|----|------|------|
| 4 | hourly | 每小时最大量警报 |
| 5 | daily | 每天最大量警报 |

---

## 8. 加密与校验

### 8.1 CRC8 校验

使用自定义查找表算法计算 1 字节 CRC8 校验值。每个数据包（包括请求和响应）末尾都包含 CRC8 校验字节。

**注意**: 通知（Notification）数据包末尾没有 CRC8 字节，代码中通过追加 `0x00` 来适配解析逻辑。

### 8.2 会话密钥生成

授权过程中使用的加密密钥由泵序列号生成：

```
MEDTRUM_CIPHER = 1,344,751,489
sn = pumpSN.reversed().toInt64()
key = simpleCrypt(randomGen(randomGen(MEDTRUM_CIPHER ^ sn)))
```

其中 `randomGen` 使用线性同余生成器（LCG），`simpleCrypt` 使用基于 Rijndael S-Box 的 32 轮 Feistel 网络结构。

### 8.3 会话令牌

会话令牌（Session Token）为 4 字节随机数，使用 `SecRandomCopyBytes` 生成。在贴片停用后重新激活时需要刷新。

### 8.4 序列号管理

每个命令的序列号从 0 开始递增，达到 254 后回绕到 0。响应中的序列号应与请求一致。

---

## 附录：命令速查表

| 命令 | 类型值 | 请求数据大小 | 响应最小大小 | 请求内容 | 响应内容 |
|------|--------|-------------|-------------|---------|---------|
| Synchronize | 3 | 0 | 9 | 空 | PatchState + FieldMask + 变长数据 |
| Subscribe | 4 | 2 | 0 | `[0xFF, 0x0F]` | 空 |
| Authorize | 5 | 9 | 10 | role(1) + sessionToken(4) + key(4) | deviceType(1) + swVersion(3) |
| Set Time | 10 | 5 | 0 | type(1) + time(4) | 空 |
| Get Time | 11 | 0 | 10 | 空 | time(4) |
| Set Time Zone | 12 | 6 | 0 | offset(2) + time(4) | 空 |
| Prime | 16 | 0 | 0 | 空 | 空 |
| Activate | 18 | 14+basal | 25 | 7参数(7) + hourlyMax(2) + dailyMax(2) + tdd(2) + 0x01(1) + basalProfile | patchId(4) + time(4) + basalType(1) + basalValue(2) + basalSequence(2) + basalPatchId(2) + basalStartTime(4) |
| Set Bolus | 19 | 4 | 0 | bolusType(1) + amount(2) + 0x00(1) | 空 |
| Cancel Bolus | 20 | 1 | 0 | bolusType(1) | 空 |
| Set Basal Profile | 21 | 1+basal | 17 | basalType(1) + basalProfile | basalType(1) + basalValue(2) + basalSequence(2) + basalPatchId(2) + basalStartTime(4) |
| Set Temp Basal | 24 | 5 | 17 | type(1) + rate(2) + duration(2) | basalType(1) + basalValue(2) + basalSequence(2) + basalPatchId(2) + basalStartTime(4) |
| Cancel Temp Basal | 25 | 0 | 17 | 空 | basalType(1) + basalValue(2) + basalSequence(2) + basalPatchId(2) + basalStartTime(4) |
| Suspend Pump | 28 | 2 | 0 | cause(1) + duration(1) | 空 |
| Resume Pump | 29 | 0 | 0 | 空 | 空 |
| Stop Patch | 31 | 0 | 10 | 空 | sequence(2) + patchId(2) |
| Set Patch | 35 | 11 | 0 | alarmSetting(1) + hourlyMax(2) + dailyMax(2) + expirationTimer(1) + 5固定值(5) | 空 |
| Clear Alarm | 115 | 2 | 2 | alertType(2) | 空 |

> **注意**: 响应最小大小（mimimumDataSize）是指 `totalData` 的最小长度，不包括 6 字节的 Header+ResponseCode。实际响应数据从偏移 6 开始。