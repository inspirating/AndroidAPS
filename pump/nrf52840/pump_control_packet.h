/*
================================================================================
泵控制数据包 (C++ 版本)
================================================================================

对应 Python: packets/pump_control_packet.py
================================================================================
*/

#ifndef M640G_PUMP_CONTROL_PACKET_H
#define M640G_PUMP_CONTROL_PACKET_H

#include "base_packet.h"
#include <cstdint>
#include <cmath>

namespace M640GKit {

class SuspendPumpPacket : public BasePacket {
public:
    uint8_t duration = 0;

    SuspendPumpPacket() {
        commandType = static_cast<uint8_t>(CommandType::SUSPEND_PUMP);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {3, duration};
    }
};

class ResumePumpPacket : public BasePacket {
public:
    ResumePumpPacket() {
        commandType = static_cast<uint8_t>(CommandType::RESUME_PUMP);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class StopPatchPacket : public BasePacket {
public:
    StopPatchPacket() {
        commandType = static_cast<uint8_t>(CommandType::STOP_PATCH);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class ActivatePacket : public BasePacket {
public:
    uint8_t autoSuspendEnable = 0;
    uint8_t autoSuspendTime = 12;
    uint8_t expirationTimer = 1;
    AlarmSettings alarmSetting = AlarmSettings::BEEP_ONLY;
    uint8_t lowSuspend = 0;
    uint8_t predictiveLowSuspend = 0;
    uint8_t predictiveLowSuspendRange = 30;
    uint16_t hourlyMaxInsulinRaw;
    uint16_t dailyMaxInsulinRaw;
    uint16_t currentTDDRaw;
    std::vector<uint8_t> basalProfile;

    ActivatePacket() {
        commandType = static_cast<uint8_t>(CommandType::ACTIVATE);
    }

    void setHourlyMaxInsulin(double value) {
        hourlyMaxInsulinRaw = static_cast<uint16_t>(round(value / 0.05));
    }

    void setDailyMaxInsulin(double value) {
        dailyMaxInsulinRaw = static_cast<uint16_t>(round(value / 0.05));
    }

    void setCurrentTDD(double value) {
        currentTDDRaw = static_cast<uint16_t>(round(value / 0.05));
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(autoSuspendEnable);
        data.push_back(autoSuspendTime);
        data.push_back(expirationTimer);
        data.push_back(static_cast<uint8_t>(alarmSetting));
        data.push_back(lowSuspend);
        data.push_back(predictiveLowSuspend);
        data.push_back(predictiveLowSuspendRange);
        data.push_back(hourlyMaxInsulinRaw & 0xFF);
        data.push_back((hourlyMaxInsulinRaw >> 8) & 0xFF);
        data.push_back(dailyMaxInsulinRaw & 0xFF);
        data.push_back((dailyMaxInsulinRaw >> 8) & 0xFF);
        data.push_back(currentTDDRaw & 0xFF);
        data.push_back((currentTDDRaw >> 8) & 0xFF);
        data.push_back(1);
        data.insert(data.end(), basalProfile.begin(), basalProfile.end());
        return data;
    }
};

class SetPatchPacket : public BasePacket {
public:
    AlarmSettings alarmSettings = AlarmSettings::BEEP_ONLY;
    uint16_t hourlyMaxInsulinRaw;
    uint16_t dailyMaxInsulinRaw;
    uint8_t expirationTimer = 1;
    uint8_t autoSuspendEnable = 0;
    uint8_t autoSuspendTime = 12;
    uint8_t lowSuspend = 0;
    uint8_t predictiveLowSuspend = 0;
    uint8_t predictiveLowSuspendRange = 30;

    SetPatchPacket() {
        commandType = static_cast<uint8_t>(CommandType::SET_PATCH);
    }

    void setHourlyMaxInsulin(double value) {
        hourlyMaxInsulinRaw = static_cast<uint16_t>(round(value / 0.05));
    }

    void setDailyMaxInsulin(double value) {
        dailyMaxInsulinRaw = static_cast<uint16_t>(round(value / 0.05));
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(alarmSettings));
        data.push_back(hourlyMaxInsulinRaw & 0xFF);
        data.push_back((hourlyMaxInsulinRaw >> 8) & 0xFF);
        data.push_back(dailyMaxInsulinRaw & 0xFF);
        data.push_back((dailyMaxInsulinRaw >> 8) & 0xFF);
        data.push_back(expirationTimer);
        data.push_back(autoSuspendEnable);
        data.push_back(autoSuspendTime);
        data.push_back(lowSuspend);
        data.push_back(predictiveLowSuspend);
        data.push_back(predictiveLowSuspendRange);
        return data;
    }
};

class PrimePacket : public BasePacket {
public:
    PrimePacket() {
        commandType = static_cast<uint8_t>(CommandType::PRIME);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

} // namespace M640GKit

#endif // M640G_PUMP_CONTROL_PACKET_H
