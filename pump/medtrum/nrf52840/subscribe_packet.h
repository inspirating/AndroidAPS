/*
================================================================================
订阅数据包 (C++ 版本)
================================================================================

对应 Python: packets/subscribe_packet.py
================================================================================
*/

#ifndef M640G_SUBSCRIBE_PACKET_H
#define M640G_SUBSCRIBE_PACKET_H

#include "base_packet.h"

namespace M640GKit {

class SubscribePacket : public BasePacket {
public:
    SubscribePacket() {
        commandType = static_cast<uint8_t>(CommandType::SUBSCRIBE);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {0xFF, 0x0F};
    }
};

} // namespace M640GKit

#endif // M640G_SUBSCRIBE_PACKET_H
