#include "sensor.h"
#include "sensor_math.h"
#include "config.h"
#include <Arduino.h>

namespace sensor {

void begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_ATTENUATION);
    setPower(true);
}

void setPower(bool on) {
#if SENSOR_POWER_PIN >= 0
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, on ? HIGH : LOW);
#endif
}

uint16_t readAdcRaw(uint8_t pin) {
    uint16_t buf[SAMPLE_COUNT];
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        buf[i] = analogRead(pin);
        delay(20);
    }
    return medianFilter(buf, SAMPLE_COUNT);
}

SoilReading readSoilMoisture() {
    uint16_t raw = readAdcRaw(SOIL_MOISTURE_ADC_PIN);
    float m = mapMoisture(raw, ADC_DRY, ADC_WET);
    Serial.printf("[sensor] ADC=%u 湿度=%.1f%%\n", raw, m);
    return { raw, m };
}

float readBatteryVoltage() {
    uint16_t raw = readAdcRaw(BATTERY_ADC_PIN);
    float v = (float)raw / 4095.0f * 3.3f;   // 12 位 ADC，约 0-3.3V
    float vbat = adcToBatteryVoltage(v, BATTERY_DIVIDER_RATIO);
    Serial.printf("[sensor] 电池 ADC=%u 电压=%.2fV\n", raw, vbat);
    return vbat;
}

} // namespace sensor
