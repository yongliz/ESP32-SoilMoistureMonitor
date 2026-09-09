#include "network.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace network {

bool syncTime(int timeoutSec) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > (unsigned long)timeoutSec * 1000UL) {
            Serial.println("[net] WiFi 连接超时");
            return false;
        }
        delay(250);
    }
    Serial.printf("[net] WiFi 已连接 IP=%s\n", WiFi.localIP().toString().c_str());

    configTime(TZ_OFFSET_SEC, 0, NTP_SERVER, "ntp.aliyun.com");

    int retries = 0;
    time_t now = time(nullptr);
    while (now < 100000 && retries < 40) {   // 1970 年说明尚未同步
        delay(250);
        now = time(nullptr);
        retries++;
    }
    return now >= 100000;
}

void disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

} // namespace network
