#pragma once
#include <stdint.h>

namespace sensor {

// clamp 到 [0, 100]
inline float clampPercent(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 100.0f) return 100.0f;
    return value;
}

// 线性映射：adcDry -> 0%，adcWet -> 100%
inline float mapMoisture(uint16_t adcRaw, uint16_t adcDry, uint16_t adcWet) {
    if (adcDry == adcWet) return 0.0f;   // 防止除零
    float m = (float)(adcRaw - adcDry) / (float)(adcWet - adcDry) * 100.0f;
    return clampPercent(m);
}

// 分压换算：分压点电压 × 分压比 = 实际电池电压
inline float adcToBatteryVoltage(float adcVoltage, float dividerRatio) {
    return adcVoltage * dividerRatio;
}

} // namespace sensor
