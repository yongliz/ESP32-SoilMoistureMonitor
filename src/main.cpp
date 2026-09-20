#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "sensor.h"
#include "alarm.h"
#include "display.h"
#include "power.h"
#include "network.h"
#include "wifi_config.h"
#include <esp_sleep.h>
#include <esp_system.h>

// 深度睡眠不丢失的唤醒计数（用于 NTP 同步频率控制）
RTC_DATA_ATTR uint32_t g_wakeCount = 0;

static char g_timeStr[20];

// 唤醒原因转可读字符串
static const char *wakeCauseStr(esp_sleep_wakeup_cause_t c) {
    switch (c) {
        case ESP_SLEEP_WAKEUP_TIMER: return "定时器(RTC)";
        case ESP_SLEEP_WAKEUP_EXT0:  return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1:  return "EXT1";
        case ESP_SLEEP_WAKEUP_GPIO:  return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:  return "UART";
        default: return "上电/其他";
    }
}

// 复位原因转可读字符串
static const char *resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "上电复位";
        case ESP_RST_DEEPSLEEP: return "深度睡眠唤醒";
        case ESP_RST_SW:        return "软件复位";
        case ESP_RST_PANIC:     return "异常(PANIC)";
        case ESP_RST_INT_WDT:   return "中断看门狗";
        case ESP_RST_TASK_WDT:  return "任务看门狗";
        case ESP_RST_WDT:       return "看门狗";
        case ESP_RST_BROWNOUT:  return "欠压(BROWNOUT)";
        default: return "未知";
    }
}

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
    // USB CDC 每次唤醒会重新枚举，等待主机完成枚举，避免开机首段日志丢失
    delay(1000);
    Serial.println("\n==================== 唤醒 ====================");
    Serial.printf("[main] 第 %u 次唤醒 | 唤醒原因=%s | 复位原因=%s\n",
                  (unsigned)(g_wakeCount + 1),
                  wakeCauseStr(esp_sleep_get_wakeup_cause()),
                  resetReasonStr(esp_reset_reason()));
    Serial.printf("[main] 芯片=%s 空闲堆=%u 字节\n",
                  ESP.getChipModel(), (unsigned)ESP.getFreeHeap());

    // 0. 读取 WiFi 凭据（配置门户在后续任务接入）
    String ssid, pass;
    bool haveCreds = wificfg::load(ssid, pass);
    if (!haveCreds) Serial.println("[main] 未找到 WiFi 配置");

    // 1. 采集
    sensor::begin();
    sensor::setPower(true);
    sensor::SoilReading r = sensor::readSoilMoisture();
    float vbat = sensor::readBatteryVoltage();   // 内部已打印电池 ADC/电压
    sensor::setPower(false);

    // 2. 告警判定（读-改-写 RTC 锁存）
    bool latched = power::getAlarmLatched();
    bool alarmActive = alarmctl::evaluate(r.moisture, DRY_THRESHOLD, HYSTERESIS, latched);
    power::setAlarmLatched(latched);
    Serial.printf("[main] 告警判定=%s (锁存=%s)\n",
                  alarmActive ? "需要浇水" : "正常", latched ? "是" : "否");

    // 3. NTP 时间同步（每 N 次唤醒才联网）
    g_wakeCount++;
    if (g_wakeCount % NTP_SYNC_EVERY_N_WAKES == 0) {
        if (haveCreds &&
            network::connect(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_SEC)) {
            bool ok = network::syncTime();
            Serial.printf("[main] 时间同步=%s\n", ok ? "成功" : "失败");
        } else {
            Serial.println("[main] WiFi 连接失败或未配置，本次不更新时间");
        }
        network::disconnect();
    } else {
        Serial.println("[main] 本次跳过 NTP（非同步周期）");
    }

    // 4. 刷新水墨屏
    display::begin();
    const char *ts = formatNow();
    Serial.printf("[main] 采样时间=%s\n", ts);
    display::render(r, vbat, alarmActive, ts);
    display::hibernate();

    // 5. 配置唤醒源并进入深度睡眠
    Serial.printf("[main] 本次唤醒总耗时=%lu ms\n", (unsigned long)millis());
    power::begin();
    power::enterDeepSleep();
}

void loop() {
    // 深度睡眠模式下不会执行到这里
}
