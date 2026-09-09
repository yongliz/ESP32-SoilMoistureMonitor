#pragma once
#include "sensor.h"

namespace display {

void begin();   // 初始化水墨屏
void render(const sensor::SoilReading &r, float vbat, bool alarm,
            const char *timeStr);
void hibernate();   // 睡眠前进入低功耗

} // namespace display
