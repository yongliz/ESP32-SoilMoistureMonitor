# ESP32 花卉土壤湿度监控系统（核心版）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 ESP32-WROOM-32 电池版花卉土壤湿度监控核心固件：定时唤醒采集土壤湿度、水墨屏本地显示、深度睡眠低功耗。

**Architecture:** PlatformIO + Arduino 框架，模块化结构。纯逻辑（校准映射、告警判定）放在 `lib/core/` 头文件中可在宿主机用 Unity 做原生单元测试；硬件层（ADC/SPI/WiFi/深睡眠）放在 `src/`，用 `pio run` 编译验证 + 实机联调验证。唤醒流程：采集 → 告警判定 → NTP 时间同步（失败跳过）→ 刷新屏 → 断开网络 → 深度睡眠。

**Tech Stack:** PlatformIO、Arduino (espressif32/esp32dev)、GxEPD2、Unity 原生测试框架。

**Spec:** `docs/superpowers/specs/2026-09-08-soil-moisture-monitor-design.md`

## Global Constraints

- 主控：ESP32-WROOM-32（`board=esp32dev`），PlatformIO + Arduino 框架。
- 显示：GxEPD2 库驱动 EPD213R（212×104 红黑白三色，SPI）。
- 电池分压：100k/100k，分压比 `BATTERY_DIVIDER_RATIO = 2.0f`。
- 电容传感器 3.3V 供电，**严禁 5V**；AO 对地并 104 电容。
- 深度睡眠 + RTC 定时唤醒；告警锁存标记存 `RTC_DATA_ATTR`（掉电不丢）。
- 湿度映射 0–100%，clamp 边界；阈值 `DRY_THRESHOLD=30.0f`，回差 `HYSTERESIS=10.0f`。
- 采样/唤醒周期默认 7200s（2 小时）；WiFi 连接超时默认 10s，失败不阻塞。
- 时区 UTC+8（`TZ_OFFSET_SEC = 8*3600`）。
- 所有业务参数集中在 `include/config.h`，改配置不动业务逻辑。
- 代码中所有注释/中文文案使用 UTF-8（源码文件以 UTF-8 编码保存）。

---

## 文件结构

```
ESP32-SoilMoistureMonitor/
├─ platformio.ini              # esp32dev + native 两个环境
├─ .gitignore                  # 忽略 .pio/ .vscode/
├─ include/config.h            # 全部业务参数
├─ lib/core/
│  ├─ sensor_math.h            # 纯逻辑：湿度映射/clamp/电池换算（可原生测试）
│  └─ alarm.h                  # 纯逻辑：告警锁存判定（可原生测试）
├─ src/
│  ├─ main.cpp                 # 唤醒流程编排
│  ├─ sensor.h / sensor.cpp    # ADC 采集 + 硬件层
│  ├─ display.h / display.cpp  # 水墨屏驱动 + UI
│  ├─ network.h / network.cpp  # WiFi + NTP
│  └─ power.h / power.cpp      # 深度睡眠 + RTC 锁存
└─ test/
   ├─ test_smoke/test_smoke.cpp            # 环境自检
   ├─ test_sensor_math/test_sensor_math.cpp
   └─ test_alarm/test_alarm.cpp
```

---

## Task 1: 工程骨架 + 构建/测试环境

**Files:**
- Create: `platformio.ini`
- Create: `.gitignore`
- Create: `include/config.h`
- Create: `src/main.cpp`
- Create: `test/test_smoke/test_smoke.cpp`

**Interfaces:**
- Produces: 可运行的 PlatformIO 工程；`pio run` 编译通过，`pio test -e native` 通过；`config.h` 提供全部宏供后续模块使用。

- [ ] **Step 1: 初始化 git 仓库并写 .gitignore**

```bash
git init
```

创建 `.gitignore`：

```gitignore
.pio/
.vscode/
```

- [ ] **Step 2: 写 platformio.ini**

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    zinggjm/GxEPD2@^1.7.2

[env:native]
platform = native
test_framework = unity
build_src_filter = -<*>
```

- [ ] **Step 3: 写 include/config.h（全部业务参数）**

```cpp
#pragma once

// ============ WiFi ============
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"

// ============ 时间同步 ============
#define NTP_SERVER              "pool.ntp.org"
#define TZ_OFFSET_SEC           (8 * 3600)   // UTC+8 中国标准时间
#define NTP_SYNC_EVERY_N_WAKES  1            // 每 N 次唤醒同步一次时间（必须 >=1）

// ============ 采集/唤醒 ============
#define WAKE_INTERVAL_SEC  7200   // 唤醒周期(秒)，默认 2 小时
#define SAMPLE_COUNT       8      // 每次采集平均次数

// ============ 告警阈值 ============
#define DRY_THRESHOLD  30.0f      // 干旱告警阈值(%)
#define HYSTERESIS     10.0f      // 回差(百分比点)

// ============ 传感器校准 ============
// 需实测：ADC_DRY=探头在干燥空气的ADC读数；ADC_WET=探头泡水读数
// 假设湿度越大 ADC 读数越高（电容式典型）。若你的传感器方向相反，交换两值即可。
#define ADC_DRY  1600
#define ADC_WET  2800

// ============ ADC 引脚 ============
#define SOIL_MOISTURE_ADC_PIN  34   // 土壤湿度 AO（ADC1_CH6）
#define BATTERY_ADC_PIN        35   // 电池分压点（ADC1_CH7）
#define ADC_ATTENUATION        ADC_11db
#define BATTERY_DIVIDER_RATIO  2.0f // 100k/100k 分压比

// ============ WiFi 超时 ============
#define WIFI_CONNECT_TIMEOUT_SEC  10

// ============ 传感器电源控制(预留) ============
#define SENSOR_POWER_PIN  -1   // -1 表示未使用；启用时接 MOSFET 开关

// ============ 水墨屏引脚 ============
#define EPD_CS_PIN    22
#define EPD_DC_PIN    20
#define EPD_RST_PIN   21
#define EPD_BUSY_PIN  -1   // 无 BUSY 引脚用 -1(SPI 轮询)；有则接 GPIO4 并改为 4

// ============ 其他 ============
#define DISPLAY_TITLE  "花卉湿度监测"
```

- [ ] **Step 4: 写最小 src/main.cpp（后续 Task 8 填充完整逻辑）**

```cpp
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("boot");
}

void loop() {
    // 深度睡眠模式下不会执行到这里
}
```

- [ ] **Step 5: 写原生冒烟测试 test/test_smoke/test_smoke.cpp**

```cpp
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_smoke(void) {
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_smoke);
    return UNITY_END();
}
```

- [ ] **Step 6: 验证编译与测试**

Run: `pio run` （预期：esp32dev 编译成功）
Run: `pio test -e native` （预期：1 test passed）

- [ ] **Step 7: 提交**

```bash
git add -A
git commit -m "chore: scaffold PlatformIO project with native test env"
```

---

## Task 2: sensor_math 纯逻辑（TDD）

**Files:**
- Create: `lib/core/sensor_math.h`
- Test: `test/test_sensor_math/test_sensor_math.cpp`

**Interfaces:**
- Produces（`namespace sensor`）:
  - `float mapMoisture(uint16_t adcRaw, uint16_t adcDry, uint16_t adcWet)` — 线性映射到 0–100%，内部 clamp
  - `float clampPercent(float value)` — clamp 到 [0,100]
  - `float adcToBatteryVoltage(float adcVoltage, float dividerRatio)` — 分压换算

- [ ] **Step 1: 写失败测试 test/test_sensor_math/test_sensor_math.cpp**

```cpp
#include <unity.h>
#include <sensor_math.h>

void setUp(void) {}
void tearDown(void) {}

void test_map_dry_is_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::mapMoisture(1600, 1600, 2800));
}

void test_map_wet_is_100(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor::mapMoisture(2800, 1600, 2800));
}

void test_map_midpoint_is_50(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, sensor::mapMoisture(2200, 1600, 2800));
}

void test_map_below_dry_clamps_to_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::mapMoisture(1400, 1600, 2800));
}

void test_map_above_wet_clamps_to_100(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor::mapMoisture(3000, 1600, 2800));
}

void test_map_equal_calibration_no_div_by_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::mapMoisture(1000, 1600, 1600));
}

void test_clamp_percent(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor::clampPercent(-5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor::clampPercent(150.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, sensor::clampPercent(42.0f));
}

void test_battery_voltage(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.2f, sensor::adcToBatteryVoltage(2.1f, 2.0f));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_map_dry_is_zero);
    RUN_TEST(test_map_wet_is_100);
    RUN_TEST(test_map_midpoint_is_50);
    RUN_TEST(test_map_below_dry_clamps_to_zero);
    RUN_TEST(test_map_above_wet_clamps_to_100);
    RUN_TEST(test_map_equal_calibration_no_div_by_zero);
    RUN_TEST(test_clamp_percent);
    RUN_TEST(test_battery_voltage);
    return UNITY_END();
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pio test -e native -f test_sensor_math`
Expected: FAIL（`sensor_math.h` 不存在）

- [ ] **Step 3: 实现 lib/core/sensor_math.h**

```cpp
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
```

- [ ] **Step 4: 运行测试确认通过**

Run: `pio test -e native -f test_sensor_math`
Expected: PASS（8 tests）

- [ ] **Step 5: 提交**

```bash
git add lib/core/sensor_math.h test/test_sensor_math
git commit -m "feat: add soil moisture calibration math (TDD)"
```

---

## Task 3: alarm 告警锁存纯逻辑（TDD）

**Files:**
- Create: `lib/core/alarm.h`
- Test: `test/test_alarm/test_alarm.cpp`

**Interfaces:**
- Produces（`namespace alarm`）:
  - `bool evaluate(float moisture, float dryThreshold, float hysteresis, bool &latched)` — 单次评估，更新并返回锁存状态

- [ ] **Step 1: 写失败测试 test/test_alarm/test_alarm.cpp**

```cpp
#include <unity.h>
#include <alarm.h>

void setUp(void) {}
void tearDown(void) {}

void test_below_threshold_sets_latch(void) {
    bool latched = false;
    bool active = alarm::evaluate(20.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_TRUE(latched);
    TEST_ASSERT_TRUE(active);
}

void test_above_threshold_plus_hysteresis_clears_latch(void) {
    bool latched = true;
    bool active = alarm::evaluate(45.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_FALSE(latched);
    TEST_ASSERT_FALSE(active);
}

void test_deadband_keeps_latch_when_latched(void) {
    bool latched = true;
    bool active = alarm::evaluate(35.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_TRUE(latched);
    TEST_ASSERT_TRUE(active);
}

void test_deadband_keeps_latch_when_clear(void) {
    bool latched = false;
    bool active = alarm::evaluate(35.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_FALSE(latched);
    TEST_ASSERT_FALSE(active);
}

void test_exact_threshold_does_not_trigger(void) {
    bool latched = false;
    bool active = alarm::evaluate(30.0f, 30.0f, 10.0f, latched);
    TEST_ASSERT_FALSE(latched);
    TEST_ASSERT_FALSE(active);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_below_threshold_sets_latch);
    RUN_TEST(test_above_threshold_plus_hysteresis_clears_latch);
    RUN_TEST(test_deadband_keeps_latch_when_latched);
    RUN_TEST(test_deadband_keeps_latch_when_clear);
    RUN_TEST(test_exact_threshold_does_not_trigger);
    return UNITY_END();
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `pio test -e native -f test_alarm`
Expected: FAIL（`alarm.h` 不存在）

- [ ] **Step 3: 实现 lib/core/alarm.h**

```cpp
#pragma once

namespace alarm {

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

} // namespace alarm
```

- [ ] **Step 4: 运行测试确认通过**

Run: `pio test -e native -f test_alarm`
Expected: PASS（5 tests）

- [ ] **Step 5: 提交**

```bash
git add lib/core/alarm.h test/test_alarm
git commit -m "feat: add alarm latch evaluation (TDD)"
```

---

## Task 4: sensor 硬件采集层

**Files:**
- Create: `src/sensor.h`
- Create: `src/sensor.cpp`

**Interfaces:**
- Consumes: `lib/core/sensor_math.h`（`mapMoisture`/`adcToBatteryVoltage`）、`config.h` 宏
- Produces（`namespace sensor`）:
  - `struct SoilReading { uint16_t adcRaw; float moisture; }`
  - `void begin()`
  - `void setPower(bool on)`
  - `uint16_t readAdcRaw(uint8_t pin, uint8_t samples)`
  - `SoilReading readSoilMoisture()`
  - `float readBatteryVoltage()`

- [ ] **Step 1: 写 src/sensor.h**

```cpp
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
```

- [ ] **Step 2: 写 src/sensor.cpp**

```cpp
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

uint16_t readAdcRaw(uint8_t pin, uint8_t samples) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delay(20);
    }
    return (uint16_t)(sum / samples);
}

SoilReading readSoilMoisture() {
    uint16_t raw = readAdcRaw(SOIL_MOISTURE_ADC_PIN, SAMPLE_COUNT);
    float m = mapMoisture(raw, ADC_DRY, ADC_WET);
    Serial.printf("[sensor] ADC=%u 湿度=%.1f%%\n", raw, m);
    return { raw, m };
}

float readBatteryVoltage() {
    uint16_t raw = readAdcRaw(BATTERY_ADC_PIN, SAMPLE_COUNT);
    float v = (float)raw / 4095.0f * 3.3f;   // 12 位 ADC，约 0-3.3V
    return adcToBatteryVoltage(v, BATTERY_DIVIDER_RATIO);
}

} // namespace sensor
```

- [ ] **Step 3: 编译验证**

Run: `pio run`
Expected: esp32dev 编译成功

- [ ] **Step 4: 提交**

```bash
git add src/sensor.h src/sensor.cpp
git commit -m "feat: add soil moisture ADC sampling layer"
```

---

## Task 5: display 水墨屏显示层

**Files:**
- Create: `src/display.h`
- Create: `src/display.cpp`

**Interfaces:**
- Consumes: `sensor::SoilReading`（来自 Task 4）、`config.h` 屏引脚宏
- Produces（`namespace display`）:
  - `void begin()`
  - `void render(const sensor::SoilReading &r, float vbat, bool alarm, const char *timeStr)`
  - `void hibernate()`

- [ ] **Step 1: 写 src/display.h**

```cpp
#pragma once
#include "sensor.h"

namespace display {

void begin();   // 初始化水墨屏
void render(const sensor::SoilReading &r, float vbat, bool alarm,
            const char *timeStr);
void hibernate();   // 睡眠前进入低功耗

} // namespace display
```

- [ ] **Step 2: 写 src/display.cpp**

```cpp
#include "display.h"
#include "config.h"
#include <GxEPD2_3C.h>
#include <GxEPD2_213_B74.h>
#include <Fonts/FreeSans9pt7b.h>

namespace display {

GxEPD2_3C<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> epd(
    GxEPD2_213_B74(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

void begin() {
    epd.init(115200);
    epd.setRotation(1);               // 横屏
    epd.setTextColor(GxEPD_BLACK);
}

void render(const sensor::SoilReading &r, float vbat, bool alarm,
            const char *timeStr) {
    const bool dry = alarm;           // 告警 = 需要浇水
    epd.setFullWindow();
    epd.firstPage();
    do {
        epd.fillScreen(GxEPD_WHITE);

        // 标题
        epd.setFont(&FreeSans9pt7b);
        epd.setTextColor(GxEPD_BLACK);
        epd.setCursor(5, 18);
        epd.print(DISPLAY_TITLE);

        // 湿度（大字）
        epd.setCursor(5, 45);
        epd.print(r.moisture, 0);
        epd.print("%");

        // 土壤状态（告警红色，正常黑色）
        epd.setTextColor(dry ? GxEPD_RED : GxEPD_BLACK);
        epd.setCursor(5, 65);
        epd.print(dry ? "需要浇水" : "正常");

        // 采样时间
        epd.setTextColor(GxEPD_BLACK);
        epd.setCursor(5, 85);
        epd.print(timeStr);

        // 电池电压
        epd.setCursor(5, 102);
        epd.print(vbat, 2);
        epd.print("V");
    } while (epd.nextPage());
}

void hibernate() {
    epd.hibernate();
}

} // namespace display
```

> 说明：坐标/字号为初始布局，Task 9 实机联调时微调间距（212×104 高度较紧凑）。

- [ ] **Step 3: 编译验证**

Run: `pio run`
Expected: esp32dev 编译成功（GxEPD2 依赖自动拉取）

- [ ] **Step 4: 提交**

```bash
git add src/display.h src/display.cpp
git commit -m "feat: add EPD213R e-paper display layer"
```

---

## Task 6: network WiFi/NTP 层

**Files:**
- Create: `src/network.h`
- Create: `src/network.cpp`

**Interfaces:**
- Consumes: `config.h` 的 WiFi/NTP 宏
- Produces（`namespace network`）:
  - `bool syncTime(int timeoutSec)` — 连接 WiFi + NTP 同步，成功返回 true
  - `void disconnect()` — 断开 WiFi

- [ ] **Step 1: 写 src/network.h**

```cpp
#pragma once

namespace network {

bool syncTime(int timeoutSec);   // 连接 WiFi + NTP，成功返回 true；超时/失败返回 false
void disconnect();               // 断开 WiFi

} // namespace network
```

- [ ] **Step 2: 写 src/network.cpp**

```cpp
#include "network.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace network {

bool syncTime(int timeoutSec) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > (unsigned long)timeoutSec * 1000UL) {
            Serial.println("[net] WiFi 连接超时");
            return false;
        }
        delay(250);
    }
    Serial.printf("[net] WiFi 已连接 IP=%s\n", WiFi.localIP().toString().c_str());

    configTime(TZ_OFFSET_SEC, 0, NTP_SERVER, "ntp.aliyun.com");

    int retries = 0;
    time_t now = time(nullptr);
    while (now < 100000 && retries < 40) {   // 1970 年说明尚未同步
        delay(250);
        now = time(nullptr);
        retries++;
    }
    return now >= 100000;
}

void disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

} // namespace network
```

- [ ] **Step 3: 编译验证**

Run: `pio run`
Expected: esp32dev 编译成功

- [ ] **Step 4: 提交**

```bash
git add src/network.h src/network.cpp
git commit -m "feat: add WiFi and NTP time sync layer"
```

---

## Task 7: power 低功耗层

**Files:**
- Create: `src/power.h`
- Create: `src/power.cpp`

**Interfaces:**
- Consumes: `config.h` 的 `WAKE_INTERVAL_SEC`
- Produces（`namespace power`）:
  - `void begin()` — 配置定时器唤醒
  - `void enterDeepSleep()`
  - `bool getAlarmLatched()`
  - `void setAlarmLatched(bool v)`

- [ ] **Step 1: 写 src/power.h**

```cpp
#pragma once

namespace power {

void begin();                 // 配置 RTC 定时器唤醒
void enterDeepSleep();        // 进入深度睡眠
bool getAlarmLatched();       // 读告警锁存（RTC 域）
void setAlarmLatched(bool v); // 写告警锁存（RTC 域）

} // namespace power
```

- [ ] **Step 2: 写 src/power.cpp**

```cpp
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
```

- [ ] **Step 3: 编译验证**

Run: `pio run`
Expected: esp32dev 编译成功

- [ ] **Step 4: 提交**

```bash
git add src/power.h src/power.cpp
git commit -m "feat: add deep sleep and RTC alarm latch layer"
```

---

## Task 8: main 唤醒流程编排

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `sensor`/`alarm`/`display`/`network`/`power` 全部接口，`config.h` 宏
- Produces: 完整可烧录固件

- [ ] **Step 1: 重写 src/main.cpp**

```cpp
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
    strftime(g_timeStr, sizeof(g_timeStr), "%Y-%m-%d %H:%M:%S", &t);
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
    bool alarmActive = alarm::evaluate(r.moisture, DRY_THRESHOLD, HYSTERESIS, latched);
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
```

- [ ] **Step 2: 编译验证**

Run: `pio run`
Expected: esp32dev 编译成功

- [ ] **Step 3: 提交**

```bash
git add src/main.cpp
git commit -m "feat: wire wake-up flow state machine"
```

---

## Task 9: 接线与校准文档

**Files:**
- Create: `doc/接线说明.md`
- Create: `doc/校准说明.md`

**Interfaces:**
- Consumes: 无（纯文档）
- Produces: 满足需求文档第 8 节「需补充的文档」中 MVP 相关的两项（HA/Server酱 配置步骤因功能暂缓，留待对应模块实现时补充）

- [ ] **Step 1: 写 doc/接线说明.md**

```markdown
# 硬件接线说明

## 电容土壤湿度传感器（FC-28，3.3V）
| 传感器 | ESP32 |
|---|---|
| VCC | 3.3V（严禁 5V） |
| GND | GND |
| AO | GPIO34 |

- AO 与 GND 之间并联一个 104(100nF) 瓷片电容，抑制数值跳变。

## 水墨屏 EPD213R（2.13 寸红黑白三色，SPI）
| 屏 | ESP32 |
|---|---|
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| CS | GPIO22 |
| RST | GPIO21 |
| DC | GPIO20 |
| BUSY | GPIO4（若屏无 BUSY 引脚则悬空，代码用 -1 SPI 轮询） |
| VCC | 3.3V |
| GND | GND |

## 电池（18650 + TP4056）
- TP4056 输出正极经分压后接 ESP32：
  - 分压电阻：100k 接电池正极 + 100k 接 GND，中点接 GPIO35。
  - 分压比 2:1，4.2V 满电时中点约 2.1V，在 ADC 量程内。
```

- [ ] **Step 2: 写 doc/校准说明.md**

```markdown
# 传感器校准操作说明

湿度百分比基于两点线性校准，需在代码 `include/config.h` 中填入实测值。

## 步骤
1. 烧录固件，打开串口监视器（115200 波特率）。
2. **干土校准**：将探头置于空气中完全干燥，等待读数稳定，记录串口输出的 `ADC` 原始值，填入 `ADC_DRY`。
3. **湿土校准**：将探头完全浸泡水中（或极湿土壤），等待读数稳定，记录 `ADC` 原始值，填入 `ADC_WET`。
4. 重新烧录后，串口应显示湿度约 0%（干）与约 100%（湿）。

## 注意事项
- 本固件假设「湿度越大 ADC 读数越高」（电容式典型）。若你的传感器方向相反（干读数高、湿读数低），交换 `ADC_DRY` 与 `ADC_WET` 两个值即可。
- 校准时先让探头在不同湿度下静置数秒，取稳定后的平均值填入，避免跳变误差。
- 若数值仍跳变，检查 AO 对地 104 电容是否接好。
```

- [ ] **Step 3: 提交**

```bash
git add doc/接线说明.md doc/校准说明.md
git commit -m "docs: add wiring and calibration guides"
```

---

## 完成后的实机联调（非代码任务，手动执行）

烧录后验证：
1. 串口每次唤醒打印 `[sensor] ADC=… 湿度=…%` 与 `[main] 第 N 次唤醒`。
2. 屏幕显示湿度、状态、时间、电池电压；告警状态红色显示。
3. 拔电重上电，告警锁存标记保持（`RTC_DATA_ATTR`）。
4. 电池电压串口/屏显值 ×2 后与万用表实测一致。
5. 观察休眠电流（理想 μA 级），确认深度睡眠生效。
