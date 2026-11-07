/*
================================================================================
同步数据包 (C++ 版本)
================================================================================

对应 Python: packets/synchronize_packet.py
================================================================================
*/

#ifndef M640G_SYNCHRONIZE_PACKET_H
#define M640G_SYNCHRONIZE_PACKET_H

#include "base_packet.h"
#include <cstdint>
#include <vector>

namespace M640GKit {

class SynchronizePacket : public BasePacket {
public:
    SynchronizePacket() {
        commandType = static_cast<uint8_t>(CommandType::SYNCHRONIZE);
        minimumDataSize = 3;
    }

    std::vector<uint8_t> getRequestBytes() const override {
        return {};
    }
};

class SynchronizeResponseParser {
public:
    static constexpr uint16_t MASK_SUSPEND = 0x0001;
    static constexpr uint16_t MASK_NORMAL_BOLUS = 0x0002;
    static constexpr uint16_t MASK_EXTENDED_BOLUS = 0x0004;
    static constexpr uint16_t MASK_BASAL = 0x0008;
    static constexpr uint16_t MASK_SETUP = 0x0010;
    static constexpr uint16_t MASK_RESERVOIR = 0x0020;
    static constexpr uint16_t MASK_START_TIME = 0x0040;
    static constexpr uint16_t MASK_BATTERY = 0x0080;
    static constexpr uint16_t MASK_STORAGE = 0x0100;
    static constexpr uint16_t MASK_ALARM = 0x0200;
    static constexpr uint16_t MASK_AGE = 0x0400;
    static constexpr uint16_t MASK_MAGNETO_PLACE = 0x0800;

    struct BolusData {
        uint8_t type;
        bool completed;
        double delivered;
    };

    struct BasalData {
        BasalType type;
        uint16_t sequence;
        uint16_t patchId;
        uint32_t startTime;
        double rate;
        double delivery;
    };

    struct BatteryData {
        double voltageA;
        double voltageB;
    };

    struct SynchronizeResponse {
        PatchState state;
        uint32_t suspendTime;
        BolusData bolus;
        BasalData basal;
        uint8_t primeProgress;
        double reservoir;
        uint32_t startTime;
        BatteryData battery;
        uint16_t storageSequence;
        uint16_t storagePatchId;
        std::vector<uint16_t> activeAlarms;
        uint32_t patchAge;
        uint16_t magnetoPlacement;
    };

    static SynchronizeResponse parse(const uint8_t* data, size_t len) {
        SynchronizeResponse resp{};
        if (len < 3) return resp;

        resp.state = static_cast<PatchState>(data[0]);
        uint16_t fieldMask = data[1] | (data[2] << 8);
        size_t offset = 3;

        if (fieldMask & MASK_SUSPEND) {
            if (offset + 4 <= len) {
                resp.suspendTime = data[offset] | (data[offset + 1] << 8) |
                                   (data[offset + 2] << 16) | (data[offset + 3] << 24);
                offset += 4;
            }
        }

        if (fieldMask & MASK_NORMAL_BOLUS) {
            if (offset + 3 <= len) {
                resp.bolus.type = data[offset] & 0x7F;
                resp.bolus.completed = (data[offset] & 0x80) != 0;
                uint16_t deliveredRaw = data[offset + 1] | (data[offset + 2] << 8);
                resp.bolus.delivered = deliveredRaw * 0.05;
                offset += 3;
            }
        }

        if (fieldMask & MASK_EXTENDED_BOLUS) {
            offset += 3;
        }

        if (fieldMask & MASK_BASAL) {
            if (offset + 12 <= len) {
                resp.basal.type = static_cast<BasalType>(data[offset]);
                resp.basal.sequence = data[offset + 1] | (data[offset + 2] << 8);
                resp.basal.patchId = data[offset + 3] | (data[offset + 4] << 8);
                resp.basal.startTime = data[offset + 5] | (data[offset + 6] << 8) |
                                       (data[offset + 7] << 16) | (data[offset + 8] << 24);
                uint32_t rateDelivery = data[offset + 9] | (data[offset + 10] << 8) | (data[offset + 11] << 16);
                uint16_t delivery = rateDelivery >> 12;
                uint16_t rate = rateDelivery & 0x0FFF;
                resp.basal.rate = rate * 0.05;
                resp.basal.delivery = delivery * 0.05;
                offset += 12;
            }
        }

        if (fieldMask & MASK_SETUP) {
            if (offset + 1 <= len) {
                resp.primeProgress = data[offset];
                offset += 1;
            }
        }

        if (fieldMask & MASK_RESERVOIR) {
            if (offset + 2 <= len) {
                uint16_t reservoirRaw = data[offset] | (data[offset + 1] << 8);
                resp.reservoir = reservoirRaw * 0.05;
                offset += 2;
            }
        }

        if (fieldMask & MASK_START_TIME) {
            if (offset + 4 <= len) {
                resp.startTime = data[offset] | (data[offset + 1] << 8) |
                                 (data[offset + 2] << 16) | (data[offset + 3] << 24);
                offset += 4;
            }
        }

        if (fieldMask & MASK_BATTERY) {
            if (offset + 3 <= len) {
                uint32_t value = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16);
                resp.battery.voltageA = (value & 0x0FFF) / 512.0;
                resp.battery.voltageB = (value >> 12) / 512.0;
                offset += 3;
            }
        }

        if (fieldMask & MASK_STORAGE) {
            if (offset + 4 <= len) {
                resp.storageSequence = data[offset] | (data[offset + 1] << 8);
                resp.storagePatchId = data[offset + 2] | (data[offset + 3] << 8);
                offset += 4;
            }
        }

        if (fieldMask & MASK_ALARM) {
            if (offset + 4 <= len) {
                uint16_t flags = data[offset] | (data[offset + 1] << 8);
                if (flags != 0) {
                    for (int i = 0; i < 3; i++) {
                        if (flags & (1 << i)) {
                            resp.activeAlarms.push_back(1 << i);
                        }
                    }
                }
                offset += 4;
            }
        }

        if (fieldMask & MASK_AGE) {
            if (offset + 4 <= len) {
                resp.patchAge = data[offset] | (data[offset + 1] << 8) |
                                (data[offset + 2] << 16) | (data[offset + 3] << 24);
                offset += 4;
            }
        }

        if (fieldMask & MASK_MAGNETO_PLACE) {
            if (offset + 2 <= len) {
                resp.magnetoPlacement = data[offset] | (data[offset + 1] << 8);
                offset += 2;
            }
        }

        return resp;
    }
};

} // namespace M640GKit

#endif // M640G_SYNCHRONIZE_PACKET_H
