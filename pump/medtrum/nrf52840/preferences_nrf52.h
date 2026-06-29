/*
================================================================================
Preferences 兼容包装层 (nRF52840 / Adafruit nRF52)
================================================================================

目的: 让 ESP32 版的 pump_simulator.h 在 nRF52840 上零改动复用。

ESP32 用 Preferences(NVS) 持久化, 调用模式为:
    Preferences prefs;
    prefs.begin("namespace", false);
    prefs.putDouble("key", value);
    double v = prefs.getDouble("key", default);
    prefs.clear();
    prefs.remove("key");
    prefs.end();

本包装类提供同名同签名的 Preferences 类, 背后用 Adafruit nRF52 内置的
InternalFS (LittleFS)。每个命名空间对应一个文件:
    "pump"      -> /prefs_pump
    "pumpStats" -> /prefs_pumpStats
    "pumpMeta"  -> /prefs_pumpMeta

文件内容是该命名空间用到的全部键的固定结构体 (见下方 PumpNS / PumpStatsNS /
PumpMetaNS)。每次 put* 整体覆盖写文件; get* 读整个文件再取对应字段。

调用点 (pump_simulator.h 共 11 处) 不需要任何修改:
  - getUInt/getUChar/getDouble/putUInt/putUChar/putDouble
  - begin(name, readOnly) / end() / clear() / remove(key)

注意: clear() 删整个命名空间文件 (等价 NVS clear); remove(key) 把对应字段
清零后整体写回 (NVS 的 remove 是删键, 这里语义上等价——未激活字段读出来是 0/默认)。

闪存寿命: persistStats() 每次注射后调用, 写一个 ~48 字节文件。LittleFS 在写
新版本时会触发页擦除, 频繁注射(>100次/天)长期运行需评估 flash 磨损。
================================================================================
*/

#ifndef M640G_PREFERENCES_NRF52_H
#define M640G_PREFERENCES_NRF52_H

#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// ========== 三个命名空间的固定结构体 ==========

// 命名空间 "pump": 补丁生命周期
struct PumpNS {
    uint32_t patchStart;    // getUInt("patchStart") / putUInt
    uint8_t  patchState;    // getUChar("patchState") / putUChar
    uint32_t elapsedTime;   // getUInt("elapsedTime") / putUInt
};

// 命名空间 "pumpStats": 注射统计 (断电后必须恢复)
struct PumpStatsNS {
    double   hourlyDel;     // getDouble("hourlyDel")
    double   dailyDel;      // getDouble("dailyDel")
    double   reservoir;     // getDouble("reservoir")
    double   stepCarry;     // getDouble("stepCarry")
    uint32_t lastHourRst;   // getUInt("lastHourRst")
    uint32_t lastDayRst;    // getUInt("lastDayRst")
};

// 命名空间 "pumpMeta": 一次性激活标记
struct PumpMetaNS {
    uint8_t  activated;     // getUChar("activated") / putUChar
};

// ========== Preferences 包装类 (API 与 ESP32 完全一致) ==========

class Preferences {
public:
    Preferences() : ns(nullptr), readOnly(false), nsType(NS_UNKNOWN) {}

    // 开始一个命名空间。命名空间名映射到文件 /prefs_<name>。
    // 实际 pump_simulator.h 只用到三个: "pump" / "pumpStats" / "pumpMeta"。
    bool begin(const char* name, bool readOnly = false) {
        this->ns = name;
        this->readOnly = readOnly;

        // InternalFS 必须先 init (由 .ino 调用 InternalFS.begin()), 这里不重复 init。
        // 确定命名空间类型
        if (strcmp(name, "pump") == 0) {
            nsType = NS_PUMP;
            strncpy(filePath, "/prefs_pump", sizeof(filePath));
        } else if (strcmp(name, "pumpStats") == 0) {
            nsType = NS_PUMPSTATS;
            strncpy(filePath, "/prefs_pumpStats", sizeof(filePath));
        } else if (strcmp(name, "pumpMeta") == 0) {
            nsType = NS_PUMPMETA;
            strncpy(filePath, "/prefs_pumpMeta", sizeof(filePath));
        } else {
            Serial.print("[PREFS] ERROR 未知命名空间: ");
            Serial.println(name);
            nsType = NS_UNKNOWN;
            return false;
        }
        return true;
    }

    void end() {
        ns = nullptr;
        nsType = NS_UNKNOWN;
    }

    // ---- get 系: 读整个文件, 取对应字段 ----
    double getDouble(const char* key, double defaultValue) const {
        uint8_t buf[NS_MAX_SIZE];
        if (!readFile(buf)) return defaultValue;
        switch (nsType) {
            case NS_PUMPSTATS: {
                PumpStatsNS* s = reinterpret_cast<PumpStatsNS*>(buf);
                if (strcmp(key, "hourlyDel") == 0) return s->hourlyDel;
                if (strcmp(key, "dailyDel")  == 0) return s->dailyDel;
                if (strcmp(key, "reservoir") == 0) return s->reservoir;
                if (strcmp(key, "stepCarry") == 0) return s->stepCarry;
                break;
            }
            default: break;
        }
        return defaultValue;
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue) const {
        uint8_t buf[NS_MAX_SIZE];
        if (!readFile(buf)) return defaultValue;
        switch (nsType) {
            case NS_PUMP: {
                PumpNS* s = reinterpret_cast<PumpNS*>(buf);
                if (strcmp(key, "patchStart")  == 0) return s->patchStart;
                if (strcmp(key, "elapsedTime") == 0) return s->elapsedTime;
                break;
            }
            case NS_PUMPSTATS: {
                PumpStatsNS* s = reinterpret_cast<PumpStatsNS*>(buf);
                if (strcmp(key, "lastHourRst") == 0) return s->lastHourRst;
                if (strcmp(key, "lastDayRst")  == 0) return s->lastDayRst;
                break;
            }
            default: break;
        }
        return defaultValue;
    }

    uint8_t getUChar(const char* key, uint8_t defaultValue) const {
        uint8_t buf[NS_MAX_SIZE];
        if (!readFile(buf)) return defaultValue;
        switch (nsType) {
            case NS_PUMP: {
                PumpNS* s = reinterpret_cast<PumpNS*>(buf);
                if (strcmp(key, "patchState") == 0) return s->patchState;
                break;
            }
            case NS_PUMPMETA: {
                PumpMetaNS* s = reinterpret_cast<PumpMetaNS*>(buf);
                if (strcmp(key, "activated") == 0) return s->activated;
                break;
            }
            default: break;
        }
        return defaultValue;
    }

    // ---- put 系: 读现有内容(或全零), 改对应字段, 整体写回 ----
    size_t putDouble(const char* key, double value) {
        if (readOnly) return 0;
        uint8_t buf[NS_MAX_SIZE] = {0};
        readFile(buf);  // 读现有值(不存在则全零)
        switch (nsType) {
            case NS_PUMPSTATS: {
                PumpStatsNS* s = reinterpret_cast<PumpStatsNS*>(buf);
                if (strcmp(key, "hourlyDel") == 0) s->hourlyDel = value;
                else if (strcmp(key, "dailyDel")  == 0) s->dailyDel  = value;
                else if (strcmp(key, "reservoir") == 0) s->reservoir = value;
                else if (strcmp(key, "stepCarry") == 0) s->stepCarry = value;
                else return 0;
                break;
            }
            default: return 0;
        }
        return writeFile(buf, nsStructSize()) ? sizeof(double) : 0;
    }

    size_t putUInt(const char* key, uint32_t value) {
        if (readOnly) return 0;
        uint8_t buf[NS_MAX_SIZE] = {0};
        readFile(buf);
        switch (nsType) {
            case NS_PUMP: {
                PumpNS* s = reinterpret_cast<PumpNS*>(buf);
                if (strcmp(key, "patchStart")  == 0) s->patchStart  = value;
                else if (strcmp(key, "elapsedTime") == 0) s->elapsedTime = value;
                else return 0;
                break;
            }
            case NS_PUMPSTATS: {
                PumpStatsNS* s = reinterpret_cast<PumpStatsNS*>(buf);
                if (strcmp(key, "lastHourRst") == 0) s->lastHourRst = value;
                else if (strcmp(key, "lastDayRst")  == 0) s->lastDayRst  = value;
                else return 0;
                break;
            }
            default: return 0;
        }
        return writeFile(buf, nsStructSize()) ? sizeof(uint32_t) : 0;
    }

    size_t putUChar(const char* key, uint8_t value) {
        if (readOnly) return 0;
        uint8_t buf[NS_MAX_SIZE] = {0};
        readFile(buf);
        switch (nsType) {
            case NS_PUMP: {
                PumpNS* s = reinterpret_cast<PumpNS*>(buf);
                if (strcmp(key, "patchState") == 0) s->patchState = value;
                else return 0;
                break;
            }
            case NS_PUMPMETA: {
                PumpMetaNS* s = reinterpret_cast<PumpMetaNS*>(buf);
                if (strcmp(key, "activated") == 0) s->activated = value;
                else return 0;
                break;
            }
            default: return 0;
        }
        return writeFile(buf, nsStructSize()) ? sizeof(uint8_t) : 0;
    }

    // 删除整个命名空间文件 (等价 NVS clear())
    void clear() {
        if (readOnly) return;
        // 禁用 LittleFS 写入, 不做任何操作
    }

    // 删单个键: 语义上 NVS remove(key) 让该键读默认值。
    // 这里把该字段清零(== 默认值场景)后整体写回。注意: 对 pumpStats 等若文件
    // 本就存在, 清零不会让 getXxx 走默认分支(因为 readFile 仍成功)——所以我们对
    // pump 命名空间(唯一用到 remove 的地方)改为删整个文件, 更贴合原语义。
    bool remove(const char* key) {
        if (readOnly) return false;
        (void)key;  // pump_simulator.h 只对 "pump" 命名空间调 remove, 删整个文件即可
        clear();
        return true;
    }

private:
    enum NsType { NS_UNKNOWN, NS_PUMP, NS_PUMPSTATS, NS_PUMPMETA };
    static constexpr size_t NS_MAX_SIZE = 64;  // 足够装下最大的结构体(PumpStatsNS)

    const char* ns;
    bool readOnly;
    NsType nsType;
    char filePath[32];

    size_t nsStructSize() const {
        switch (nsType) {
            case NS_PUMP:      return sizeof(PumpNS);
            case NS_PUMPSTATS: return sizeof(PumpStatsNS);
            case NS_PUMPMETA:  return sizeof(PumpMetaNS);
            default:           return 0;
        }
    }

    // 读文件到 buf (只读 nsStructSize() 字节)。返回 false 表示文件不存在/读失败。
    // 持久化已禁用, 始终返回 false 让调用方使用默认值。
    bool readFile(uint8_t* buf) const {
        (void)buf;
        return false;  // 文件不存在, 走默认值
    }

    // 把 buf 的 sz 字节整体覆盖写回文件。
    // !! nRF52840 LittleFS 在运行时写入会触发断言崩溃 (pcache->block == 0xffffffff)。
    // !! 模拟器不需要断电持久化, 禁用写入, 每次重启从 FILLED 状态开始。
    bool writeFile(const uint8_t* buf, size_t sz) {
        (void)buf; (void)sz;
        return true;  // 假装写入成功, 实际不写 flash
    }
};

#endif // M640G_PREFERENCES_NRF52_H
