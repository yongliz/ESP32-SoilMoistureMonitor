#pragma once
#include <Arduino.h>

// WiFi 凭据的 NVS 持久化（命名空间 "wifi"，键 ssid/pass）
namespace wificfg {

bool hasConfig();                                   // NVS 中是否已保存非空 SSID
bool load(String &ssid, String &pass);              // 读取凭据，成功返回 true
bool save(const String &ssid, const String &pass);  // 保存凭据
void clear();                                       // 清除（恢复未配置状态）

} // namespace wificfg
