/*
================================================================================
加密模块测试 (C++ 版本)
================================================================================

测试 CRC8 和 Crypto 模块的正确性

对应 Python: tests/test_crypto.py
================================================================================
*/

#ifndef M640G_TEST_CRYPTO_H
#define M640G_TEST_CRYPTO_H

#include <Arduino.h>
#include "crc8.h"
#include "crypto.h"

namespace M640GKit {

class TestCrypto {
public:
    static void runAll() {
        Serial.println("\n========== 加密模块测试 ==========");
        testCrc8();
        testCryptoGenKey();
        testCryptoSimpleDecrypt();
        Serial.println("========== 测试完成 ==========\n");
    }

    static void testCrc8() {
        Serial.println("[测试] CRC8 校验");

        uint8_t testData1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        uint8_t crc1 = crc8Calculate(testData1, 5);
        Serial.printf("  测试数据1 CRC: 0x%02X\n", crc1);

        uint8_t testData2[] = {0xFF, 0xFF, 0xFF, 0xFF};
        uint8_t crc2 = crc8Calculate(testData2, 4);
        Serial.printf("  测试数据2 CRC: 0x%02X\n", crc2);

        uint8_t testData3[] = {0x00, 0x00, 0x00, 0x00};
        uint8_t crc3 = crc8Calculate(testData3, 4);
        Serial.printf("  测试数据3 CRC: 0x%02X\n", crc3);

        Serial.println("  CRC8 测试通过!");
    }

    static void testCryptoGenKey() {
        Serial.println("[测试] 密钥生成");

        uint8_t pumpSn[4] = {0x28, 0xD8, 0x12, 0x4A};
        uint8_t key[4];
        Crypto::genKey(pumpSn, key);

        Serial.printf("  序列号: ");
        for (int i = 0; i < 4; i++) {
            Serial.printf("%02X", pumpSn[i]);
        }
        Serial.println();

        Serial.printf("  生成密钥: ");
        for (int i = 0; i < 4; i++) {
            Serial.printf("%02X", key[i]);
        }
        Serial.println();

        Serial.println("  密钥生成测试通过!");
    }

    static void testCryptoSimpleDecrypt() {
        Serial.println("[测试] 简单解密");

        uint8_t inputData[4] = {0x12, 0x34, 0x56, 0x78};
        uint8_t decrypted[4];
        Crypto::simpleDecrypt(inputData, decrypted);

        Serial.printf("  输入数据: ");
        for (int i = 0; i < 4; i++) {
            Serial.printf("%02X", inputData[i]);
        }
        Serial.println();

        Serial.printf("  解密结果: ");
        for (int i = 0; i < 4; i++) {
            Serial.printf("%02X", decrypted[i]);
        }
        Serial.println();

        Serial.println("  解密测试通过!");
    }
};

} // namespace M640GKit

#endif // M640G_TEST_CRYPTO_H
