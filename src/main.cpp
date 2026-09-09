#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "sensor.h"
#include "alarm.h"
#include "display.h"
#include "power.h"
#include "network.h"

// 深度睡眠不丢失的唤醒计数（用于 NTP 同步频率控制）
RTC_DATA_ATTR uint32_t g_wakeCount = 0;

static char g_timeStr[20];

// 返回当前时间字符串；未同步返回占位符
static const char *formatNow() {
    time_t now = time(nullptr);
    if (now < 100000) {                 // 1970 年说明尚未同步
        return "未同步时间";
    }
    struct tm t;
    localtime_r(&now, &t);
    strftime(g_timeStr, sizeof(g_timeStr), "%m-%d %H:%M", &t);
    return g_timeStr;
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.printf("[main] 第 %u 次唤醒\n", (unsigned)(g_wakeCount + 1));

    // 1. 采集
    sensor::begin();
    sensor::setPower(true);
    sensor::SoilReading r = sensor::readSoilMoisture();
    float vbat = sensor::readBatteryVoltage();
    sensor::setPower(false);

    // 2. 告警判定（读-改-写 RTC 锁存）
    bool latched = power::getAlarmLatched();
    bool alarmActive = alarmctl::evaluate(r.moisture, DRY_THRESHOLD, HYSTERESIS, latched);
    power::setAlarmLatched(latched);

    // 3. NTP 时间同步（失败跳过，不阻塞）
    g_wakeCount++;
    if (g_wakeCount % NTP_SYNC_EVERY_N_WAKES == 0) {
        if (network::syncTime(WIFI_CONNECT_TIMEOUT_SEC)) {
            Serial.println("[main] 时间同步成功");
        } else {
            Serial.println("[main] 时间同步失败，使用占位时间");
        }
        network::disconnect();
    }

    // 4. 刷新水墨屏
    display::begin();
    display::render(r, vbat, alarmActive, formatNow());
    display::hibernate();

    // 5. 配置唤醒源并进入深度睡眠
    power::begin();
    power::enterDeepSleep();
}

void loop() {
    // 深度睡眠模式下不会执行到这里
}
