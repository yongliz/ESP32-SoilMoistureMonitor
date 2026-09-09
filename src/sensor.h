#pragma once
#include <stdint.h>

namespace sensor {

struct SoilReading {
    uint16_t adcRaw;   // 原始 ADC（多次平均后）
    float    moisture; // 0-100% 湿度
};

void begin();                            // 配置 ADC、传感器电源
void setPower(bool on);                  // 传感器电源开关（预留）
uint16_t readAdcRaw(uint8_t pin, uint8_t samples); // 多次采样取平均
SoilReading readSoilMoisture();          // 采集 + 映射 + clamp
float readBatteryVoltage();              // 分压换算后电池电压

} // namespace sensor
