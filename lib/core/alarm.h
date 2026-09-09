#pragma once

namespace alarmctl {

// 单次采样评估告警锁存状态。
// moisture:      当前湿度 0-100
// dryThreshold:  干旱告警阈值(%)
// hysteresis:    回差(百分比点)，用于清除锁存
// latched:       输入/输出，锁存标记(跨睡眠保存于 RTC_DATA_ATTR)
// 返回: 当前是否处于告警锁存状态
inline bool evaluate(float moisture, float dryThreshold, float hysteresis, bool &latched) {
    if (moisture < dryThreshold) {
        latched = true;
    } else if (moisture >= dryThreshold + hysteresis) {
        latched = false;
    }
    return latched;
}

} // namespace alarmctl
