/*
================================================================================
数据包基类 (C++ 版本)
================================================================================

定义所有数据包的基类, 提供编解码功能

数据包格式:
- Byte 0:     数据长度 (包括包头和CRC)
- Byte 1:     命令类型
- Byte 2:     序列号
- Byte 3:     包索引 (0=不分包/最后一包, 1-N=分包序号)
- Byte 4-5:   响应码
- Byte 6...:  数据内容
- 最后1字节:  CRC8校验

对应 Python: packets/base_packet.py
================================================================================
*/

#ifndef M640G_BASE_PACKET_H
#define M640G_BASE_PACKET_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include "enums.h"
#include "crc8.h"

namespace M640GKit {

class BasePacket {
public:
    uint8_t commandType = 0;
    uint8_t dataSize = 0;
    uint16_t responseCode = 0;
    std::vector<uint8_t> totalData;
    uint8_t sequenceNumber = 0;
    bool failed = false;
    uint8_t minimumDataSize = 0;

    BasePacket() = default;
    virtual ~BasePacket() = default;

    // 子类需要重写此方法, 返回请求数据部分
    virtual std::vector<uint8_t> getRequestBytes() const {
        return {};
    }

    // 编码数据包
    std::vector<std::vector<uint8_t>> encode(uint8_t seqNum) const {
        std::vector<uint8_t> content = getRequestBytes();

        // 构建包头: [长度, 命令类型, 序列号, 包索引]
        std::vector<uint8_t> header = {
            static_cast<uint8_t>(content.size() + 5),
            commandType,
            seqNum,
            0  // 包索引
        };

        // 计算CRC前的数据
        std::vector<uint8_t> tmp = header;
        tmp.insert(tmp.end(), content.begin(), content.end());
        uint8_t crc = crc8Calculate(tmp.data(), tmp.size());
        tmp.push_back(crc);

        // 单包不超过15字节时不分包
        if (tmp.size() - header.size() <= 15) {
            tmp.push_back(0);  // 填充0
            return {tmp};
        }

        // 分包处理
        std::vector<std::vector<uint8_t>> packages;
        uint8_t pkgIndex = 1;
        size_t offset = header.size();

        while (tmp.size() - offset > 15) {
            header[3] = pkgIndex;
            std::vector<uint8_t> pkg = header;
            pkg.insert(pkg.end(), tmp.begin() + offset, tmp.begin() + offset + 15);
            uint8_t pkgCrc = crc8Calculate(pkg.data(), pkg.size());
            pkg.push_back(pkgCrc);
            packages.push_back(pkg);
            offset += 15;
            pkgIndex++;
        }

        // 最后一包
        header[3] = pkgIndex;
        std::vector<uint8_t> lastPkg = header;
        lastPkg.insert(lastPkg.end(), tmp.begin() + offset, tmp.end());
        uint8_t lastCrc = crc8Calculate(lastPkg.data(), lastPkg.size());
        lastPkg.push_back(lastCrc);
        packages.push_back(lastPkg);

        return packages;
    }

    // 编码通知数据包 (使用 totalData)
    std::vector<std::vector<uint8_t>> encodeNotification(uint8_t seqNum) const {
        std::vector<uint8_t> content = totalData;

        std::vector<uint8_t> header = {
            static_cast<uint8_t>(content.size() + 5),
            commandType,
            seqNum,
            0
        };

        std::vector<uint8_t> tmp = header;
        tmp.insert(tmp.end(), content.begin(), content.end());
        uint8_t crc = crc8Calculate(tmp.data(), tmp.size());
        tmp.push_back(crc);

        if (tmp.size() - header.size() <= 15) {
            tmp.push_back(0);
            return {tmp};
        }

        std::vector<std::vector<uint8_t>> packages;
        uint8_t pkgIndex = 1;
        size_t offset = header.size();

        while (tmp.size() - offset > 15) {
            header[3] = pkgIndex;
            std::vector<uint8_t> pkg = header;
            pkg.insert(pkg.end(), tmp.begin() + offset, tmp.begin() + offset + 15);
            uint8_t pkgCrc = crc8Calculate(pkg.data(), pkg.size());
            pkg.push_back(pkgCrc);
            packages.push_back(pkg);
            offset += 15;
            pkgIndex++;
        }

        header[3] = pkgIndex;
        std::vector<uint8_t> lastPkg = header;
        lastPkg.insert(lastPkg.end(), tmp.begin() + offset, tmp.end());
        uint8_t lastCrc = crc8Calculate(lastPkg.data(), lastPkg.size());
        lastPkg.push_back(lastCrc);
        packages.push_back(lastPkg);

        return packages;
    }

    // 解码数据包
    void decode(const uint8_t* data, size_t len) {
        if (totalData.empty()) {
            if (data[1] != commandType) {
                failed = true;
            }

            totalData.assign(data, data + len - 1);
            dataSize = data[0];
            sequenceNumber = data[3];
            responseCode = data[4] | (data[5] << 8);

            uint8_t initialCrc = crc8Calculate(data, len - 1);
            if (initialCrc != data[len - 1]) {
                failed = true;
            }
            return;
        }

        totalData.insert(totalData.end(), data + 4, data + len - 1);
        sequenceNumber++;

        uint8_t newCrc = crc8Calculate(data, len - 1);
        if (newCrc != data[len - 1]) {
            failed = true;
        }
        if (sequenceNumber != data[3]) {
            failed = true;
        }
    }

    bool isComplete() const {
        return totalData.size() == dataSize;
    }

    bool hasEnoughData() const {
        return totalData.size() >= minimumDataSize;
    }
};

} // namespace M640GKit

#endif // M640G_BASE_PACKET_H
