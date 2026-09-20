#include "wifi_config.h"
#include <Preferences.h>

namespace wificfg {

static const char* NS = "wifi";
static const char* K_SSID = "ssid";
static const char* K_PASS = "pass";

bool hasConfig() {
    Preferences p;
    if (!p.begin(NS, true)) {
        Serial.println("[wificfg] NVS 打开失败(只读)");
        return false;
    }
    bool ok = p.isKey(K_SSID) && p.getString(K_SSID, "").length() > 0;
    p.end();
    return ok;
}

bool load(String &ssid, String &pass) {
    Preferences p;
    if (!p.begin(NS, true)) {
        Serial.println("[wificfg] NVS 打开失败(只读)");
        return false;
    }
    ssid = p.getString(K_SSID, "");
    pass = p.getString(K_PASS, "");
    p.end();
    if (ssid.length() == 0) {
        Serial.println("[wificfg] 未找到已保存的 SSID");
        return false;
    }
    Serial.printf("[wificfg] 已加载凭据: SSID=%s\n", ssid.c_str());
    return true;
}

bool save(const String &ssid, const String &pass) {
    Preferences p;
    if (!p.begin(NS, false)) {
        Serial.println("[wificfg] NVS 打开失败(读写)");
        return false;
    }
    p.putString(K_SSID, ssid);
    p.putString(K_PASS, pass);
    p.end();
    Serial.printf("[wificfg] 已保存凭据: SSID=%s\n", ssid.c_str());
    return true;
}

void clear() {
    Preferences p;
    if (!p.begin(NS, false)) return;
    p.clear();
    p.end();
    Serial.println("[wificfg] 已清除凭据");
}

} // namespace wificfg
