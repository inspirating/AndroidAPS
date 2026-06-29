/*
================================================================================
数据包测试 (C++ 版本)
================================================================================

测试所有数据包的编解码功能

对应 Python: tests/test_packets.py
================================================================================
*/

#ifndef M640G_TEST_PACKETS_H
#define M640G_TEST_PACKETS_H

#include <Arduino.h>
#include "base_packet.h"
#include "authorize_packet.h"
#include "synchronize_packet.h"
#include "bolus_packet.h"
#include "basal_packet.h"
#include "time_packet.h"
#include "pump_control_packet.h"

namespace M640GKit {

class TestPackets {
public:
    static void runAll() {
        Serial.println("\n========== 数据包测试 ==========");
        testBasePacket();
        testAuthorizePacket();
        testSynchronizePacket();
        testBolusPacket();
        testBasalPacket();
        testTimePacket();
        testPumpControlPacket();
        Serial.println("========== 测试完成 ==========\n");
    }

    static void testBasePacket() {
        Serial.println("[测试] 基础数据包");

        BasePacket packet;
        packet.commandType = 0x03;
        auto encoded = packet.encode(1);

        Serial.printf("  编码包数量: %d\n", encoded.size());
        Serial.printf("  包大小: %d\n", encoded[0].size());

        Serial.println("  基础数据包测试通过!");
    }

    static void testAuthorizePacket() {
        Serial.println("[测试] 认证数据包");

        AuthorizePacket packet;
        auto encoded = packet.encode(1);

        Serial.printf("  命令类型: 0x%02X\n", packet.commandType);
        Serial.printf("  编码包数量: %d\n", encoded.size());

        Serial.println("  认证数据包测试通过!");
    }

    static void testSynchronizePacket() {
        Serial.println("[测试] 同步数据包");

        SynchronizePacket packet;
        auto encoded = packet.encode(1);

        Serial.printf("  命令类型: 0x%02X\n", packet.commandType);
        Serial.printf("  编码包数量: %d\n", encoded.size());

        // 测试解析器
        uint8_t testData[] = {0x01, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
        auto response = SynchronizeResponseParser::parse(testData, sizeof(testData));
        Serial.printf("  解析状态: %d\n", static_cast<int>(response.state));

        Serial.println("  同步数据包测试通过!");
    }

    static void testBolusPacket() {
        Serial.println("[测试] 大剂量数据包");

        SetBolusPacket packet(0x01, 100);
        auto encoded = packet.encode(1);

        Serial.printf("  命令类型: 0x%02X\n", packet.commandType);
        Serial.printf("  编码包数量: %d\n", encoded.size());

        CancelBolusPacket cancelPacket;
        auto cancelEncoded = cancelPacket.encode(2);
        Serial.printf("  取消包数量: %d\n", cancelEncoded.size());

        Serial.println("  大剂量数据包测试通过!");
    }

    static void testBasalPacket() {
        Serial.println("[测试] 基础率数据包");

        SetTempBasalPacket packet(0x01, 50, 30);
        auto encoded = packet.encode(1);

        Serial.printf("  命令类型: 0x%02X\n", packet.commandType);
        Serial.printf("  编码包数量: %d\n", encoded.size());

        CancelTempBasalPacket cancelPacket;
        auto cancelEncoded = cancelPacket.encode(2);
        Serial.printf("  取消包数量: %d\n", cancelEncoded.size());

        Serial.println("  基础率数据包测试通过!");
    }

    static void testTimePacket() {
        Serial.println("[测试] 时间数据包");

        GetTimePacket getPacket;
        auto encoded = getPacket.encode(1);
        Serial.printf("  获取时间包数量: %d\n", encoded.size());

        SetTimePacket setPacket(123456);
        auto setEncoded = setPacket.encode(2);
        Serial.printf("  设置时间包数量: %d\n", setEncoded.size());

        SetTimeZonePacket tzPacket(480, 123456);
        auto tzEncoded = tzPacket.encode(3);
        Serial.printf("  设置时区包数量: %d\n", tzEncoded.size());

        Serial.println("  时间数据包测试通过!");
    }

    static void testPumpControlPacket() {
        Serial.println("[测试] 泵控制数据包");

        SuspendPumpPacket suspendPacket;
        auto suspendEncoded = suspendPacket.encode(1);
        Serial.printf("  暂停包数量: %d\n", suspendEncoded.size());

        ResumePumpPacket resumePacket;
        auto resumeEncoded = resumePacket.encode(2);
        Serial.printf("  恢复包数量: %d\n", resumeEncoded.size());

        ActivatePacket activatePacket;
        auto activateEncoded = activatePacket.encode(3);
        Serial.printf("  激活包数量: %d\n", activateEncoded.size());

        Serial.println("  泵控制数据包测试通过!");
    }
};

} // namespace M640GKit

#endif // M640G_TEST_PACKETS_H
