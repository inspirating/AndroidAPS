/*
================================================================================
加密解密模块 (C++ 版本)
================================================================================

提供与 M640G 泵通信的加密解密功能

算法原理:
- 使用线性同余生成器 (LCG) 生成伪随机数
- 使用类 Rijndael S-Box 进行字节替换
- 32轮迭代的类 AES 变换

对应 Python: crypto.py
================================================================================
*/

#ifndef M640G_CRYPTO_H
#define M640G_CRYPTO_H

#include <cstdint>
#include <cstring>

namespace M640GKit {

class Crypto {
public:
    // 固定 Cipher 值, 用于配对密钥生成
    static constexpr uint32_t M640G_CIPHER = 1344751489;

    // Rijndael S-Box 正向查找表 (AES 标准表)
    static constexpr uint8_t RIJNDAEL_S_BOX[256] = {
        99, 124, 119, 123, 242, 107, 111, 197, 48, 1, 103, 43, 254, 215, 171, 118,
        202, 130, 201, 125, 250, 89, 71, 240, 173, 212, 162, 175, 156, 164, 114, 192,
        183, 253, 147, 38, 54, 63, 247, 204, 52, 165, 229, 241, 113, 216, 49, 21,
        4, 199, 35, 195, 24, 150, 5, 154, 7, 18, 128, 226, 235, 39, 178, 117,
        9, 131, 44, 26, 27, 110, 90, 160, 82, 59, 214, 179, 41, 227, 47, 132,
        83, 209, 0, 237, 32, 252, 177, 91, 106, 203, 190, 57, 74, 76, 88, 207,
        208, 239, 170, 251, 67, 77, 51, 133, 69, 249, 2, 127, 80, 60, 159, 168,
        81, 163, 64, 143, 146, 157, 56, 245, 188, 182, 218, 33, 16, 255, 243, 210,
        205, 12, 19, 236, 95, 151, 68, 23, 196, 167, 126, 61, 100, 93, 25, 115,
        96, 129, 79, 220, 34, 42, 144, 136, 70, 238, 184, 20, 222, 94, 11, 219,
        224, 50, 58, 10, 73, 6, 36, 92, 194, 211, 172, 98, 145, 149, 228, 121,
        231, 200, 55, 109, 141, 213, 78, 169, 108, 86, 244, 234, 101, 122, 174, 8,
        186, 120, 37, 46, 28, 166, 180, 198, 232, 221, 116, 31, 75, 189, 139, 138,
        112, 62, 181, 102, 72, 3, 246, 14, 97, 53, 87, 185, 134, 193, 29, 158,
        225, 248, 152, 17, 105, 217, 142, 148, 155, 30, 135, 233, 206, 85, 40, 223,
        140, 161, 137, 13, 191, 230, 66, 104, 65, 153, 45, 15, 176, 84, 187, 22
    };

    // Rijndael S-Box 逆向查找表
    static constexpr uint8_t RIJNDAEL_INVERSE_S_BOX[256] = {
        82, 9, 106, 213, 48, 54, 165, 56, 191, 64, 163, 158, 129, 243, 215, 251,
        124, 227, 57, 130, 155, 47, 255, 135, 52, 142, 67, 68, 196, 222, 233, 203,
        84, 123, 148, 50, 166, 194, 35, 61, 238, 76, 149, 11, 66, 250, 195, 78,
        8, 46, 161, 102, 40, 217, 36, 178, 118, 91, 162, 73, 109, 139, 209, 37,
        114, 248, 246, 100, 134, 104, 152, 22, 212, 164, 92, 204, 93, 101, 182, 146,
        108, 112, 72, 80, 253, 237, 185, 218, 94, 21, 70, 87, 167, 141, 157, 132,
        144, 216, 171, 0, 140, 188, 211, 10, 247, 228, 88, 5, 184, 179, 69, 6,
        208, 44, 30, 143, 202, 63, 15, 2, 193, 175, 189, 3, 1, 19, 138, 107,
        58, 145, 17, 65, 79, 103, 220, 234, 151, 242, 207, 206, 240, 180, 230, 115,
        150, 172, 116, 34, 231, 173, 53, 133, 226, 249, 55, 232, 28, 117, 223, 110,
        71, 241, 26, 113, 29, 41, 197, 137, 111, 183, 98, 14, 170, 24, 190, 27,
        252, 86, 62, 75, 198, 210, 121, 32, 154, 219, 192, 254, 120, 205, 90, 244,
        31, 221, 168, 51, 136, 7, 199, 49, 177, 18, 16, 89, 39, 128, 236, 95,
        96, 81, 127, 169, 25, 181, 74, 13, 45, 229, 122, 159, 147, 201, 156, 239,
        160, 224, 59, 77, 174, 42, 245, 176, 200, 235, 187, 60, 131, 83, 153, 97,
        23, 43, 4, 126, 186, 119, 214, 38, 225, 105, 20, 99, 85, 33, 12, 125,
    };

    static uint32_t rotateLeft(uint32_t x, uint8_t s, uint8_t n) {
        return ((x << n) | (x >> (s - n))) & ((1u << s) - 1);
    }

    static uint32_t rotateRight(uint32_t x, uint8_t s, uint8_t n) {
        return ((x >> n) | (x << (s - n))) & ((1u << s) - 1);
    }

    static uint32_t changeByTable(uint32_t inputVal, const uint8_t* table) {
        uint32_t result = 0;
        for (int i = 0; i < 4; i++) {
            result |= static_cast<uint32_t>(table[(inputVal >> (i * 8)) & 0xFF]) << (i * 8);
        }
        return result;
    }

    static uint32_t randomGen(uint32_t inputVal) {
        constexpr uint32_t a = 16807;
        constexpr uint32_t q = 127773;
        constexpr uint32_t r = 2836;
        uint32_t tmp1 = inputVal / q;
        int32_t ret = static_cast<int32_t>((inputVal - tmp1 * q) * a - tmp1 * r);
        if (ret < 0) {
            ret += 2147483647;
        }
        return static_cast<uint32_t>(ret);
    }

    static uint32_t simpleCrypt(uint32_t inputVal) {
        uint32_t temp = inputVal ^ M640G_CIPHER;
        for (int i = 0; i < 32; i++) {
            temp = changeByTable(rotateLeft(temp, 32, 1), RIJNDAEL_S_BOX);
        }
        return temp;
    }

    static void genKey(const uint8_t* pumpSn, uint8_t* outKey) {
        uint32_t sn = pumpSn[0] | (pumpSn[1] << 8) | (pumpSn[2] << 16) | (pumpSn[3] << 24);
        uint32_t key = randomGen(randomGen(M640G_CIPHER ^ sn));
        key = simpleCrypt(key);
        outKey[0] = key & 0xFF;
        outKey[1] = (key >> 8) & 0xFF;
        outKey[2] = (key >> 16) & 0xFF;
        outKey[3] = (key >> 24) & 0xFF;
    }

    static void simpleDecrypt(const uint8_t* inputData, uint8_t* outData) {
        uint32_t temp = inputData[0] | (inputData[1] << 8) | (inputData[2] << 16) | (inputData[3] << 24);
        for (int i = 0; i < 32; i++) {
            uint32_t x = changeByTable(temp, RIJNDAEL_INVERSE_S_BOX);
            temp = rotateRight(x, 32, 1);
        }
        uint32_t result = temp ^ M640G_CIPHER;
        outData[0] = result & 0xFF;
        outData[1] = (result >> 8) & 0xFF;
        outData[2] = (result >> 16) & 0xFF;
        outData[3] = (result >> 24) & 0xFF;
    }
};

}  // namespace M640GKit

// C++14 兼容: static constexpr 数组成员需要类外定义 (C++17 隐式 inline, 不需要)
// Adafruit nRF52 默认 C++14, 若缺少此定义会报 undefined reference 链接错误
#if __cplusplus < 201703L
constexpr uint8_t M640GKit::Crypto::RIJNDAEL_S_BOX[256];
constexpr uint8_t M640GKit::Crypto::RIJNDAEL_INVERSE_S_BOX[256];
#endif

#endif // M640G_CRYPTO_H
