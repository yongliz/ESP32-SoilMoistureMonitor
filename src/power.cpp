#include "power.h"
#include "config.h"
#include <Arduino.h>
#include <esp_sleep.h>

namespace power {

// 深度睡眠不丢失的告警锁存标记
RTC_DATA_ATTR bool g_alarmLatched = false;

void begin() {
    esp_sleep_enable_timer_wakeup((uint64_t)WAKE_INTERVAL_SEC * 1000000ULL);
}

void enterDeepSleep() {
    Serial.println("[power] 进入深度睡眠");
    Serial.flush();
    esp_deep_sleep_start();
}

bool getAlarmLatched() { return g_alarmLatched; }
void setAlarmLatched(bool v) { g_alarmLatched = v; }

} // namespace power
