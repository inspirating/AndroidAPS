/*
================================================================================
杂项数据包 (C++ 版本)
================================================================================

对应 Python: packets/misc_packet.py
================================================================================
*/

#ifndef M640G_MISC_PACKET_H
#define M640G_MISC_PACKET_H

#include "base_packet.h"
#include <cstdint>

namespace M640GKit {

class PollPatchPacket : public BasePacket {
public:
    PollPatchPacket() {
        commandType = static_cast<uint8_t>(CommandType::POLL_PATCH);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class GetDeviceTypePacket : public BasePacket {
public:
    GetDeviceTypePacket() {
        commandType = static_cast<uint8_t>(CommandType::GET_DEVICE_TYPE);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class GetRecordPacket : public BasePacket {
public:
    uint8_t recordType;
    uint8_t recordIndex;

    GetRecordPacket(uint8_t type = 0, uint8_t index = 0)
        : recordType(type), recordIndex(index) {
        commandType = static_cast<uint8_t>(CommandType::GET_RECORD);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {recordType, recordIndex};
    }
};

} // namespace M640GKit

#endif // M640G_MISC_PACKET_H
