#pragma once

namespace network {

bool syncTime(int timeoutSec);   // 连接 WiFi + NTP，成功返回 true；超时/失败返回 false
void disconnect();               // 断开 WiFi

} // namespace network
