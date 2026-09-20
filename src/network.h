#pragma once

namespace network {

bool connect(const char* ssid, const char* pass, int timeoutSec);  // 连接 WiFi，成功返回 true
bool syncTime();        // 在已连接状态下做 NTP 同步，成功返回 true
void disconnect();      // 断开 WiFi 并关闭射频

} // namespace network
