# Debug Session: medtrum-connect-fill-stuck

## Status: [OPEN]

## Symptom
Medtrum连接ESP32时一直卡在"connect and fill"页面，无法继续。

## Environment
- AndroidAPS on Android
- ESP32 as pump simulator
- BLE communication

## Hypotheses
1. ESP32 AUTH时状态重置逻辑未生效，仍返回EXPIRED/STOPPED而非FILLED
2. Android端SYNCHRONIZE后pumpState未正确更新为FILLED
3. Android端patchPrimed标志位残留为true，导致FILLED状态被忽略
4. ESP32的PRIME响应格式仍有问题，Android无法解析
5. Android ViewModel的pumpStateFlow收集未触发UI更新

## Next Steps
- 在ESP32关键位置添加日志上报
- 收集Android端和ESP32端的运行时日志
- 分析日志确认哪个假设成立
