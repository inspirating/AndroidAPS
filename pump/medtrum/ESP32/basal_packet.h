/*
================================================================================
基础率数据包 (C++ 版本)
================================================================================

对应 Python: packets/basal_packet.py
================================================================================
*/

#ifndef M640G_BASAL_PACKET_H
#define M640G_BASAL_PACKET_H

#include "base_packet.h"
#include <cstdint>

namespace M640GKit {

class SetBasalProfilePacket : public BasePacket {
public:
    uint8_t basalType = 1;
    std::vector<uint8_t> profileData;

    SetBasalProfilePacket() {
        commandType = static_cast<uint8_t>(CommandType::SET_BASAL_PROFILE);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(basalType);
        data.insert(data.end(), profileData.begin(), profileData.end());
        return data;
    }
};

class SetTempBasalPacket : public BasePacket {
public:
    uint8_t basalType;
    uint16_t rateRaw;      // 实际速率 = rateRaw * 0.05
    uint16_t duration;     // 分钟

    SetTempBasalPacket(uint8_t type = 0, uint16_t rate = 0, uint16_t dur = 0)
        : basalType(type), rateRaw(rate), duration(dur) {
        commandType = static_cast<uint8_t>(CommandType::SET_TEMP_BASAL);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(basalType);
        data.push_back(rateRaw & 0xFF);
        data.push_back((rateRaw >> 8) & 0xFF);
        data.push_back(duration & 0xFF);
        data.push_back((duration >> 8) & 0xFF);
        return data;
    }
};

class CancelTempBasalPacket : public BasePacket {
public:
    CancelTempBasalPacket() {
        commandType = static_cast<uint8_t>(CommandType::CANCEL_TEMP_BASAL);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

} // namespace M640GKit

#endif // M640G_BASAL_PACKET_H
