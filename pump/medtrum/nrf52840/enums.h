/*
================================================================================
枚举类型定义 (C++ 版本)
================================================================================

包含所有 BLE 通信相关的枚举类型:
- BLE 状态
- 命令类型
- Patch 状态
- 基础率类型
- 警报设置
- 同步响应掩码
- 连接错误类型

对应 Python: enums.py
================================================================================
*/

#ifndef M640G_ENUMS_H
#define M640G_ENUMS_H

#include <cstdint>
#include <cstring>

namespace M640GKit {

// =============================================================================
// BLE UUID 定义
// =============================================================================

// GATT 服务和特征 UUID (字符串形式, 用于 Arduino BLE 库)
constexpr const char* SERVICE_UUID = "669A9001-0008-968F-E311-6050405558B3";
constexpr const char* READ_UUID = "669a9120-0008-968f-e311-6050405558b3";
constexpr const char* WRITE_UUID = "669a9101-0008-968f-e311-6050405558B3";

// =============================================================================
// BLE 状态机
// =============================================================================

enum class BLEState : uint8_t {
    IDLE = 0,              // 空闲, 未连接
    SCANNING,              // 扫描中
    CONNECTING,            // 连接中
    CONNECTED,             // 已连接
    AUTHENTICATING,        // 认证中
    AUTHENTICATED,         // 已认证
    DISCONNECTED           // 断开连接
};

// =============================================================================
// 命令类型枚举
// =============================================================================

enum class CommandType : uint8_t {
    SYNCHRONIZE = 3,          // 同步泵状态 (轮询)
    SUBSCRIBE = 4,            // 订阅通知
    AUTH_REQ = 5,             // 认证请求
    GET_DEVICE_TYPE = 6,      // 获取设备类型
    SET_TIME = 10,            // 设置泵时间
    GET_TIME = 11,            // 获取泵时间
    SET_TIME_ZONE = 12,       // 设置时区
    PRIME = 16,               // 灌注操作
    ACTIVATE = 18,            // 激活 Patch
    SET_BOLUS = 19,           // 设置大剂量
    CANCEL_BOLUS = 20,        // 取消大剂量
    SET_BASAL_PROFILE = 21,   // 设置基础率配置文件
    SET_TEMP_BASAL = 24,      // 设置临时基础率
    CANCEL_TEMP_BASAL = 25,   // 取消临时基础率
    SUSPEND_PUMP = 28,        // 暂停泵
    RESUME_PUMP = 29,         // 恢复泵
    POLL_PATCH = 30,          // 轮询 Patch 状态
    STOP_PATCH = 31,          // 停止 Patch
    READ_BOLUS_STATE = 34,    // 读取大剂量状态
    SET_PATCH = 35,           // 设置 Patch
    SET_BOLUS_MOTOR = 36,     // 设置大剂量电机
    GET_RECORD = 99,          // 获取记录
    CLEAR_ALARM = 115         // 清除警报
};

// =============================================================================
// Patch 状态枚举
// =============================================================================

enum class PatchState : uint8_t {
    NONE = 0,                 // 无状态/未知
    IDLE = 1,                 // 空闲
    FILLED = 2,               // 已填充胰岛素
    PRIMING = 3,              // 灌注中
    PRIMED = 4,               // 已完成灌注
    EJECTING = 5,             // 弹出中
    EJECTED = 6,              // 已弹出
    ACTIVE = 32,              // 运行中 (正常工作中)
    ACTIVE_ALT = 33,          // 运行中 (备用)
    LOW_BG_SUSPENDED = 64,    // 低血糖自动暂停
    LOW_BG_SUSPENDED2 = 65,   // 低血糖自动暂停2
    AUTO_SUSPENDED = 66,      // 自动暂停
    HOURLY_MAX_SUSPENDED = 67, // 每小时最大暂停
    DAILY_MAX_SUSPENDED = 68,  // 每日最大暂停
    SUSPENDED = 69,           // 手动暂停
    PAUSED = 70,              // 暂停
    OCCLUSION = 96,           // 堵管报警
    EXPIRED = 97,             // Patch 已过期
    RESERVOIR_EMPTY = 98,     // 储药器空
    PATCH_FAULT = 99,         // Patch 故障
    PATCH_FAULT2 = 100,       // Patch 故障2
    BASE_FAULT = 101,         // 基础率故障
    BATTERY_OUT = 102,        // 电池耗尽
    NO_CALIBRATION = 103,     // 无校准
    STOPPED = 128             // 已停止
};

// =============================================================================
// 基础率类型枚举
// =============================================================================

enum class BasalType : uint8_t {
    NONE = 0,
    STANDARD = 1,
    EXERCISE = 2,
    HOLIDAY = 3,
    PROGRAM_A = 4,
    PROGRAM_B = 5,
    ABSOLUTE_TEMP = 6,       // 绝对值临时基础率
    RELATIVE_TEMP = 7,       // 相对值临时基础率
    PROGRAM_C = 8,
    PROGRAM_D = 9,
    SICK = 10,
    AUTO = 11,
    NEW = 12,
    SUSPEND_LOW_GLUCOSE = 13,
    SUSPEND_PREDICT_LOW_GLUCOSE = 14,
    SUSPEND_AUTO = 15,
    SUSPEND_MORE_THAN_MAX_PER_HOUR = 16,
    SUSPEND_MORE_THAN_MAX_PER_DAY = 17,
    SUSPEND_MANUAL = 18,
    SUSPEND_KEY_LOST = 19,
    STOP_OCCLUSION = 20,
    STOP_EXPIRED = 21,
    STOP_EMPTY = 22,
    STOP_PATCH_FAULT = 23,
    STOP_PATCH_FAULT2 = 24,
    STOP_BASE_FAULT = 25,
    STOP_DISCARD = 26,
    STOP_BATTERY_EMPTY = 27,
    STOP = 28,
    PAUSE_INTERRUPT = 29,
    PRIME = 30,
    AUTO_MODE_START = 31,
    AUTO_MODE_EXIT = 32,
    AUTO_MODE_TARGET_100 = 33,
    AUTO_MODE_TARGET_110 = 34,
    AUTO_MODE_TARGET_120 = 35,
    AUTO_MODE_BREAKFAST = 36,
    AUTO_MODE_LUNCH = 37,
    AUTO_MODE_DINNER = 38,
    AUTO_MODE_SNACK = 39,
    AUTO_MODE_EXERCISE_START = 40,
    AUTO_MODE_EXERCISE_EXIT = 41
};

// =============================================================================
// 警报设置枚举
// =============================================================================

enum class AlarmSettings : uint8_t {
    LIGHT_VIBRATE_BEEP = 0,   // 灯光+震动+蜂鸣
    LIGHT_VIBRATE = 1,        // 灯光+震动
    LIGHT_BEEP = 2,           // 灯光+蜂鸣
    LIGHT_ONLY = 3,           // 仅灯光
    VIBRATE_BEEP = 4,         // 震动+蜂鸣
    VIBRATE_ONLY = 5,         // 仅震动
    BEEP_ONLY = 6,            // 仅蜂鸣
    NONE = 7                  // 静音
};

// =============================================================================
// 警报类型枚举
// =============================================================================

enum class AlertType : uint8_t {
    HOURLY = 4,   // 每小时警报
    DAILY = 5     // 每日警报
};

// =============================================================================
// 连接错误类型
// =============================================================================

namespace ConnectError {
    constexpr const char* NO_SERIAL_NUMBER = "noSerialNumberAvailable";
    constexpr const char* INVALID_BLUETOOTH_STATE = "invalidBluetoothState";
    constexpr const char* FAILED_TO_CONNECT = "failedToConnectToDevice";
    constexpr const char* FAILED_TO_FIND_DEVICE = "failedToFindDevice";
    constexpr const char* NO_MANAGER = "noManager";
    constexpr const char* NO_WRITE_CHARACTERISTIC = "noWriteCharacteristic";
    constexpr const char* ALREADY_RUNNING = "alreadyRunning";
    constexpr const char* NO_DATA = "noData";
    constexpr const char* TIMEOUT = "timeout";
    constexpr const char* FAILED_DISCOVER_SERVICES = "failedToDiscoverServices";
    constexpr const char* FAILED_DISCOVER_CHARACTERISTICS = "failedToDiscoverCharacteristics";
}

// =============================================================================
// 同步响应掩码
// =============================================================================

constexpr uint16_t MASK_SUSPEND = 0x0001;
constexpr uint16_t MASK_NORMAL_BOLUS = 0x0002;
constexpr uint16_t MASK_EXTENDED_BOLUS = 0x0004;
constexpr uint16_t MASK_BASAL = 0x0008;
constexpr uint16_t MASK_SETUP = 0x0010;
constexpr uint16_t MASK_RESERVOIR = 0x0020;
constexpr uint16_t MASK_START_TIME = 0x0040;
constexpr uint16_t MASK_BATTERY = 0x0080;
constexpr uint16_t MASK_STORAGE = 0x0100;
constexpr uint16_t MASK_ALARM = 0x0200;
constexpr uint16_t MASK_AGE = 0x0400;
constexpr uint16_t MASK_MAGNETO_PLACE = 0x0800;
constexpr uint16_t MASK_UNUSED_CGM = 0x1000;
constexpr uint16_t MASK_UNUSED_COMMAND_CONFIRM = 0x2000;
constexpr uint16_t MASK_UNUSED_AUTO_STATUS = 0x4000;
constexpr uint16_t MASK_UNUSED_LEGACY = 0x8000;

// =============================================================================
// BLE 错误码定义
// =============================================================================

namespace BLEErrorCode {
    constexpr uint16_t SUCCESS = 0x0000;

    // 通用错误
    constexpr uint16_t UNKNOWN_ERROR = 0x0100;
    constexpr uint16_t INVALID_PARAMETER = 0x0101;
    constexpr uint16_t TIMEOUT = 0x0102;
    constexpr uint16_t NOT_CONNECTED = 0x0103;
    constexpr uint16_t ALREADY_CONNECTED = 0x0104;

    // 认证错误
    constexpr uint16_t AUTH_REQUIRED = 0x0200;
    constexpr uint16_t AUTH_FAILED = 0x0201;
    constexpr uint16_t INVALID_KEY = 0x0202;
    constexpr uint16_t INVALID_TOKEN = 0x0203;

    // 泵错误
    constexpr uint16_t PUMP_BUSY = 0x0300;
    constexpr uint16_t PUMP_SUSPENDED = 0x0301;
    constexpr uint16_t RESERVOIR_LOW = 0x0302;
    constexpr uint16_t RESERVOIR_EMPTY = 0x0303;
    constexpr uint16_t BATTERY_LOW = 0x0304;
    constexpr uint16_t BATTERY_EMPTY = 0x0305;
    constexpr uint16_t OCCLUSION = 0x0306;
    constexpr uint16_t PATCH_EXPIRED = 0x0307;
    constexpr uint16_t MAX_BOLUS_EXCEEDED = 0x0308;
    constexpr uint16_t MAX_BASAL_EXCEEDED = 0x0309;
    constexpr uint16_t HOURLY_MAX_EXCEEDED = 0x030A;
    constexpr uint16_t DAILY_MAX_EXCEEDED = 0x030B;

    // 状态错误
    constexpr uint16_t INVALID_STATE = 0x0400;
    constexpr uint16_t NOT_INITIALIZED = 0x0401;
    constexpr uint16_t ALREADY_INITIALIZED = 0x0402;

    inline const char* toString(uint16_t errorCode) {
        switch (errorCode) {
            case SUCCESS: return "成功";
            case UNKNOWN_ERROR: return "未知错误";
            case INVALID_PARAMETER: return "参数无效";
            case TIMEOUT: return "超时";
            case NOT_CONNECTED: return "未连接";
            case ALREADY_CONNECTED: return "已连接";
            case AUTH_REQUIRED: return "需要认证";
            case AUTH_FAILED: return "认证失败";
            case INVALID_KEY: return "密钥无效";
            case INVALID_TOKEN: return "令牌无效";
            case PUMP_BUSY: return "泵忙";
            case PUMP_SUSPENDED: return "泵已暂停";
            case RESERVOIR_LOW: return "储药器低";
            case RESERVOIR_EMPTY: return "储药器空";
            case BATTERY_LOW: return "电池低";
            case BATTERY_EMPTY: return "电池空";
            case OCCLUSION: return "堵管";
            case PATCH_EXPIRED: return "Patch已过期";
            case MAX_BOLUS_EXCEEDED: return "超过最大大剂量";
            case MAX_BASAL_EXCEEDED: return "超过最大基础率";
            case HOURLY_MAX_EXCEEDED: return "超过每小时最大量";
            case DAILY_MAX_EXCEEDED: return "超过每日最大量";
            case INVALID_STATE: return "状态无效";
            case NOT_INITIALIZED: return "未初始化";
            case ALREADY_INITIALIZED: return "已初始化";
            default: return "未知错误";
        }
    }
}

} // namespace M640GKit

#endif // M640G_ENUMS_H
