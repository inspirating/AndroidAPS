/*
================================================================================
连接追踪器 (C++ 版本)
================================================================================

追踪 BLE 连接统计信息

对应 Python: pump_manager/ble_manager.py 中的 ConnectionTracker
================================================================================
*/

#ifndef M640G_CONNECTION_TRACKER_H
#define M640G_CONNECTION_TRACKER_H

#include <Arduino.h>
#include <cstdint>

namespace M640GKit {

class ConnectionTracker {
public:
    uint32_t connectCount = 0;
    uint32_t disconnectCount = 0;
    uint32_t totalUptime = 0;
    uint32_t lastConnectTime = 0;
    uint32_t lastDisconnectTime = 0;
    uint32_t currentSessionStart = 0;

    void onConnect() {
        connectCount++;
        lastConnectTime = millis();
        currentSessionStart = lastConnectTime;
    }

    void onDisconnect(const char* reason) {
        disconnectCount++;
        lastDisconnectTime = millis();
        if (currentSessionStart > 0) {
            totalUptime += (lastDisconnectTime - currentSessionStart);
            currentSessionStart = 0;
        }
    }

    uint32_t getCurrentSessionDuration() const {
        if (currentSessionStart > 0) {
            return millis() - currentSessionStart;
        }
        return 0;
    }

    void getStats(char* buffer, size_t bufferSize) const {
        snprintf(buffer, bufferSize,
                 "连接次数: %lu, 断开次数: %lu, 总在线: %lu ms, 当前会话: %lu ms",
                 connectCount, disconnectCount, totalUptime, getCurrentSessionDuration());
    }
};

} // namespace M640GKit

#endif // M640G_CONNECTION_TRACKER_H
