/*
================================================================================
测试主入口 (C++ 版本)
================================================================================

运行所有测试

使用方法:
在 Arduino setup() 中调用 TestMain::runAll();
================================================================================
*/

#ifndef M640G_TEST_MAIN_H
#define M640G_TEST_MAIN_H

#include "test_crypto.h"
#include "test_packets.h"

namespace M640GKit {

class TestMain {
public:
    static void runAll() {
        Serial.println("\n");
        Serial.println("╔══════════════════════════════════════╗");
        Serial.println("║     M640GKit 测试套件启动           ║");
        Serial.println("╚══════════════════════════════════════╝");
        Serial.println("");

        TestCrypto::runAll();
        TestPackets::runAll();

        Serial.println("");
        Serial.println("╔══════════════════════════════════════╗");
        Serial.println("║     所有测试已完成                  ║");
        Serial.println("╚══════════════════════════════════════╝");
        Serial.println("");
    }
};

} // namespace M640GKit

#endif // M640G_TEST_MAIN_H
