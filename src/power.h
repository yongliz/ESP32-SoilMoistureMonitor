#pragma once

namespace power {

void begin();                 // 配置 RTC 定时器唤醒
void enterDeepSleep();        // 进入深度睡眠
bool getAlarmLatched();       // 读告警锁存（RTC 域）
void setAlarmLatched(bool v); // 写告警锁存（RTC 域）

} // namespace power
