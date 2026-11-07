/*
================================================================================
认证数据包 (C++ 版本)
================================================================================

对应 Python: packets/authorize_packet.py
================================================================================
*/

#ifndef M640G_AUTHORIZE_PACKET_H
#define M640G_AUTHORIZE_PACKET_H

#include "base_packet.h"
#include "crypto.h"

namespace M640GKit {

class AuthorizePacket : public BasePacket {
public:
    uint8_t role = 2;
    uint8_t pumpSN[4];
    uint8_t sessionToken[4];

    AuthorizePacket() {
        commandType = static_cast<uint8_t>(CommandType::AUTH_REQ);
        minimumDataSize = 10;
    }

    void setPumpSN(const uint8_t* sn) {
        pumpSN[0] = sn[3];
        pumpSN[1] = sn[2];
        pumpSN[2] = sn[1];
        pumpSN[3] = sn[0];
    }

    void setSessionToken(const uint8_t* token) {
        memcpy(sessionToken, token, 4);
    }

    std::vector<uint8_t> getRequestBytes() const override {
        uint8_t key[4];
        Crypto::genKey(pumpSN, key);

        std::vector<uint8_t> data;
        data.push_back(role);
        data.insert(data.end(), sessionToken, sessionToken + 4);
        data.insert(data.end(), key, key + 4);
        return data;
    }

    struct Response {
        uint8_t deviceType;
        uint8_t swMajor;
        uint8_t swMinor;
        uint8_t swPatch;
    };

    Response parseResponse() const {
        Response resp{};
        if (totalData.size() >= 11) {
            resp.deviceType = totalData[7];
            resp.swMajor = totalData[8];
            resp.swMinor = totalData[9];
            resp.swPatch = totalData[10];
        }
        return resp;
    }
};

} // namespace M640GKit

#endif // M640G_AUTHORIZE_PACKET_H
