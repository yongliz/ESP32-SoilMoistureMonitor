#include "network.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace network {

bool syncTime(int timeoutSec) {
    Serial.printf("[net] 连接 WiFi: SSID=%s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > (unsigned long)timeoutSec * 1000UL) {
            Serial.printf("[net] WiFi 连接超时(%d s)，状态码=%d\n",
                          timeoutSec, (int)WiFi.status());
            return false;
        }
        delay(250);
    }
    Serial.printf("[net] WiFi 已连接: 用时=%lu ms IP=%s RSSI=%d dBm\n",
                  (unsigned long)(millis() - start),
                  WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());

    configTime(TZ_OFFSET_SEC, 0, NTP_SERVER, "ntp.aliyun.com");

    int retries = 0;
    time_t now = time(nullptr);
    while (now < 100000 && retries < 40) {   // 1970 年说明尚未同步
        delay(250);
        now = time(nullptr);
        retries++;
    }
    if (now >= 100000) {
        struct tm t;
        localtime_r(&now, &t);
        Serial.printf("[net] NTP 同步成功: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        Serial.println("[net] NTP 同步失败，时间未更新");
    }
    return now >= 100000;
}

void disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

} // namespace network
