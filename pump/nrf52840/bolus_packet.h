/*
================================================================================
大剂量数据包 (C++ 版本)
================================================================================

对应 Python: packets/bolus_packet.py
================================================================================
*/

#ifndef M640G_BOLUS_PACKET_H
#define M640G_BOLUS_PACKET_H

#include "base_packet.h"
#include <cstdint>

namespace M640GKit {

class SetBolusPacket : public BasePacket {
public:
    uint8_t bolusType;
    uint16_t amountRaw;  // 实际剂量 = amountRaw * 0.05

    SetBolusPacket(uint8_t type = 0, uint16_t amount = 0)
        : bolusType(type), amountRaw(amount) {
        commandType = static_cast<uint8_t>(CommandType::SET_BOLUS);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(bolusType);
        data.push_back(amountRaw & 0xFF);
        data.push_back((amountRaw >> 8) & 0xFF);
        data.push_back(0);
        return data;
    }
};

class CancelBolusPacket : public BasePacket {
public:
    uint8_t bolusType = 1;

    CancelBolusPacket() {
        commandType = static_cast<uint8_t>(CommandType::CANCEL_BOLUS);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {bolusType};
    }
};

class ReadBolusStatePacket : public BasePacket {
public:
    uint8_t bolusId;

    ReadBolusStatePacket(uint8_t id = 0) : bolusId(id) {
        commandType = static_cast<uint8_t>(CommandType::READ_BOLUS_STATE);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {bolusId};
    }

    struct Response {
        uint8_t bolusId;
        bool isActive;
        double delivered;
        double remaining;
        double programmed;
        uint16_t duration;
    };

    Response parseResponse() const {
        Response resp{};
        if (totalData.size() >= 8) {
            resp.bolusId = totalData[6];
            resp.isActive = totalData[7] != 0;
        }
        if (totalData.size() >= 12) {
            uint16_t deliveredRaw = totalData[8] | (totalData[9] << 8);
            uint16_t remainingRaw = totalData[10] | (totalData[11] << 8);
            resp.delivered = deliveredRaw * 0.05;
            resp.remaining = remainingRaw * 0.05;
        }
        if (totalData.size() >= 16) {
            uint16_t programmedRaw = totalData[12] | (totalData[13] << 8);
            resp.programmed = programmedRaw * 0.05;
            resp.duration = totalData[14] | (totalData[15] << 8);
        }
        return resp;
    }
};

class SetBolusMotorPacket : public BasePacket {
public:
    uint16_t motorSteps;
    uint8_t direction;

    SetBolusMotorPacket(uint16_t steps = 0, uint8_t dir = 0)
        : motorSteps(steps), direction(dir) {
        commandType = static_cast<uint8_t>(CommandType::SET_BOLUS_MOTOR);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        std::vector<uint8_t> data;
        data.push_back(motorSteps & 0xFF);
        data.push_back((motorSteps >> 8) & 0xFF);
        data.push_back(direction);
        return data;
    }
};

} // namespace M640GKit

#endif // M640G_BOLUS_PACKET_H
