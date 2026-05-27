/*
================================================================================
时间和警报数据包 (C++ 版本)
================================================================================

对应 Python: packets/time_packet.py
================================================================================
*/

#ifndef M640G_TIME_PACKET_H
#define M640G_TIME_PACKET_H

#include "base_packet.h"
#include <cstdint>

namespace M640GKit {

// M640GKit 使用自定义的时间基准: 2014年1月1日 00:00:00 UTC
static constexpr uint32_t BASE_UNIX = 1388534400;

inline uint32_t m640gkitSeconds() {
    // 在 Arduino ESP32 中, 使用 time(nullptr) 获取当前 Unix 时间
    // 返回从 2014-01-01 开始的秒数
    time_t now = time(nullptr);
    if (now < (time_t)BASE_UNIX) {
        return 0;
    }
    return (uint32_t)(now - BASE_UNIX);
}

inline void dateFromM640gkitSeconds(uint32_t seconds, int& year, int& month, int& day,
                                     int& hour, int& minute, int& second) {
    time_t unixTime = (time_t)seconds + BASE_UNIX;
    struct tm* tmInfo = gmtime(&unixTime);
    if (tmInfo) {
        year = tmInfo->tm_year + 1900;
        month = tmInfo->tm_mon + 1;
        day = tmInfo->tm_mday;
        hour = tmInfo->tm_hour;
        minute = tmInfo->tm_min;
        second = tmInfo->tm_sec;
    } else {
        year = 2014; month = 1; day = 1;
        hour = 0; minute = 0; second = 0;
    }
}

class GetTimePacket : public BasePacket {
public:
    GetTimePacket() {
        commandType = static_cast<uint8_t>(CommandType::GET_TIME);
        minimumDataSize = 10;
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }

    uint32_t parseResponse() const {
        if (totalData.size() >= 10) {
            return totalData[6] | (totalData[7] << 8) |
                   (totalData[8] << 16) | (totalData[9] << 24);
        }
        return 0;
    }
};

class SetTimePacket : public BasePacket {
public:
    uint32_t dateSeconds;

    SetTimePacket(uint32_t seconds = 0) : dateSeconds(seconds) {
        commandType = static_cast<uint8_t>(CommandType::SET_TIME);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(2);
        data.push_back(dateSeconds & 0xFF);
        data.push_back((dateSeconds >> 8) & 0xFF);
        data.push_back((dateSeconds >> 16) & 0xFF);
        data.push_back((dateSeconds >> 24) & 0xFF);
        return data;
    }
};

class SetTimeZonePacket : public BasePacket {
public:
    int16_t timezoneOffset;
    uint32_t dateSeconds;

    SetTimeZonePacket(int16_t offset = 0, uint32_t seconds = 0)
        : timezoneOffset(offset), dateSeconds(seconds) {
        commandType = static_cast<uint8_t>(CommandType::SET_TIME_ZONE);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(timezoneOffset & 0xFF);
        data.push_back((timezoneOffset >> 8) & 0xFF);
        data.push_back(dateSeconds & 0xFF);
        data.push_back((dateSeconds >> 8) & 0xFF);
        data.push_back((dateSeconds >> 16) & 0xFF);
        data.push_back((dateSeconds >> 24) & 0xFF);
        return data;
    }
};

class ClearAlertPacket : public BasePacket {
public:
    uint16_t alertType = 4;

    ClearAlertPacket() {
        commandType = static_cast<uint8_t>(CommandType::CLEAR_ALARM);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {
            static_cast<uint8_t>(alertType & 0xFF),
            static_cast<uint8_t>((alertType >> 8) & 0xFF)
        };
    }
};

} // namespace M640GKit

#endif // M640G_TIME_PACKET_H
