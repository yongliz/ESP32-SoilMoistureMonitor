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

// 中值滤波：对 n 个采样原地插入排序后取中值（偶数取中间两值平均）。
// 相比算术平均更能剔除偶发毛刺/尖峰。注意会修改 samples 数组。
inline uint16_t medianFilter(uint16_t* samples, uint8_t n) {
    if (n == 0) return 0;
    for (uint8_t i = 1; i < n; i++) {
        uint16_t v = samples[i];
        int j = (int)i - 1;
        while (j >= 0 && samples[j] > v) { samples[j + 1] = samples[j]; j--; }
        samples[j + 1] = v;
    }
    uint8_t mid = n / 2;
    if (n & 1) return samples[mid];
    return (uint16_t)((samples[mid - 1] + samples[mid]) / 2);
}

} // namespace sensor
