# ESP32 花卉土壤湿度监控系统 — 设计文档

文档版本：V1.0
日期：2026-09-08
对应需求：`doc/需求文档.md`（V1.0）

---

## 1. 概述与范围

本设计文档描述一套基于 ESP32 的花卉土壤湿度监控系统的**核心版（MVP）**实现。

### 1.1 首期范围（MVP）

- ✅ 土壤湿度采集（电容式传感器 ADC 采样 + 校准换算 + 边界约束）
- ✅ 水墨屏本地显示（湿度、状态、告警、采样时间、电池电压）
- ✅ 深度睡眠低功耗（RTC 定时唤醒 + 告警标记掉电保持）
- ✅ WiFi + NTP 网络时间同步（用于显示采样时间）
- ✅ 参数集中配置区
- ❌ Server酱 微信推送（本期不实现，预留接口）
- ❌ HomeAssistant / MQTT 接入（本期不实现，预留接口）

### 1.2 关键决策记录

| 决策项 | 结论 |
|---|---|
| 主控芯片 | ESP32-WROOM-32（经典，GPIO34/35 为仅输入 ADC 引脚，与需求引脚定义一致） |
| 开发框架 | PlatformIO + Arduino 框架（`board=esp32dev`） |
| 运行模式 | 只做电池低功耗版（带水墨屏）；插电版后续再加 |
| 电池分压 | 100k / 100k（18650 满电 4.2V → 分压后 ~2.1V，满足 ADC ≤3.3V 量程） |
| 水墨屏驱动库 | GxEPD2（类 `GxEPD2_213_B74`，212×104 红黑白三色） |

---

## 2. 总体架构与模块划分

采用**模块化**代码结构，每个模块单一职责、通过清晰接口交互、可独立测试。为满足需求文档 5.4「可扩展性」，后续新增光照传感器、水泵、微信推送、MQTT 时直接插入新模块，不动现有代码。

```
ESP32-SoilMoistureMonitor/
├─ platformio.ini            # board=esp32dev, framework=arduino, 依赖: GxEPD2
├─ include/
│  └─ config.h               # 全部业务参数集中配置区
├─ src/
│  ├─ main.cpp               # 唤醒流程编排 + 状态机
│  ├─ sensor.cpp / sensor.h  # ADC采集、多次平均、校准映射、边界约束
│  ├─ display.cpp / display.h# 水墨屏驱动 + UI 布局
│  ├─ power.cpp / power.h    # 深度睡眠、RTC 唤醒、RTC_DATA_ATTR 标记
│  ├─ network.cpp / network.h# WiFi + NTP 时间同步（超时、失败跳过）
│  └─ alarm.cpp / alarm.h    # 告警阈值 + 回差 + 锁存判定
└─ doc/
   └─ 需求文档.md
```

模块职责与依赖：

| 模块 | 职责 | 依赖 |
|---|---|---|
| `config` | 集中管理所有业务参数（WiFi、周期、阈值、校准、NTP 频率） | 无 |
| `sensor` | 采集 ADC、多次平均抗跳变、校准映射 0–100%、边界约束、串口调试输出 | `config` |
| `alarm` | 根据湿度与阈值/回差判定告警锁存状态 | `config` |
| `display` | 驱动水墨屏，绘制主界面 | `config` |
| `power` | 深度睡眠、定时器唤醒、RTC_DATA_ATTR 读写告警标记 | `config` |
| `network` | WiFi 连接（超时）、NTP 时间同步 | `config` |
| `main` | 编排唤醒流程状态机，串联各模块 | 全部 |

---

## 3. 硬件抽象层

### 3.1 引脚定义

| 器件 | 引脚 | ESP32 GPIO | 说明 |
|---|---|---|---|
| 土壤湿度传感器 VCC | — | 3.3V | 严禁 5V |
| 土壤湿度传感器 GND | — | GND | — |
| 土壤湿度传感器 AO | 模拟输出 | GPIO34（ADC1_CH6） | 对地并 104 瓷片电容抑制跳变 |
| 水墨屏 SCK | — | GPIO18 | SPI 时钟 |
| 水墨屏 MISO | — | GPIO19 | SPI（屏只读，实际可悬空/接） |
| 水墨屏 MOSI | — | GPIO23 | SPI 数据 |
| 水墨屏 CS | — | GPIO22 | 片选（空闲低） |
| 水墨屏 RST | — | GPIO21 | 复位（空闲低） |
| 水墨屏 DC | — | GPIO20 | 数据/命令（空闲低） |
| 水墨屏 VCC | — | 3.3V | — |
| 水墨屏 GND | — | GND | — |
| 电池 VBAT | 经 100k/100k 分压后 | GPIO35（ADC1_CH7） | 分压点接 GPIO35，量程安全 |

### 3.2 电池电压测量

- 18650 满电 4.2V，经 100k/100k 分压得 ~2.1V，接入 GPIO35（ADC 量程 ≤3.3V）。
- 换算：`Vbat = adcVoltage × 2`（分压比 1:2）。
- ADC 衰减选 `ADC_11db`，量程约 0–3.3V。

### 3.3 传感器电源控制（预留，可选）

需求文档风险 4 指出深度睡眠下 GPIO 常接传感器存在漏电，长续航建议 GPIO 控制传感器电源。
本期作为**配置项预留**：`config.h` 中定义 `SENSOR_POWER_PIN`（默认 -1 表示未使用）。若使用，硬件需外接 MOSFET/三极管开关，采集前拉高供电、采集后拉低断电。

---

## 4. 数据采集模块（sensor）

1. **采样**：每次唤醒采集 N 次（默认 8 次），取算术平均，抑制随机误差。
2. **校准映射**：
   ```
   湿度% = (adcRaw - adcWet) / (adcDry - adcWet) × 100
   ```
   其中 `adcDry`（空气中干燥读数）、`adcWet`（泡水读数）为校准参数。
3. **边界约束**：结果 clamp 到 [0, 100]。
4. **串口调试**：打印 ADC 原始值 + 湿度百分比，便于校准（需求 4.1）。

### 4.1 关键接口

```cpp
// sensor.h
struct SoilReading {
    uint16_t adcRaw;   // 原始 ADC 值（多次平均后）
    float    moisture; // 0-100% 湿度
};
SoilReading readSoilMoisture();   // 采集 + 平均 + 映射 + clamp
float       readBatteryVoltage(); // 分压后换算为实际 V
```

---

## 5. 水墨屏显示模块（display）

### 5.1 主界面布局（三色）

| 区域 | 内容 | 颜色 |
|---|---|---|
| 标题 | 项目名称（如「花卉湿度监测」） | 黑 |
| 湿度 | `XX%`（大字） | 黑/红（告警时红） |
| 土壤状态 | 正常 / 需要浇水 | 正常黑、需浇水红 |
| 告警状态 | 正常 / 告警 | 同上 |
| 采样时间 | `YYYY-MM-DD HH:MM:SS` | 黑 |
| 电池电压 | `XX.XXV` | 黑 |

### 5.2 刷新策略

- 每次唤醒：采集 → 刷新屏幕 → 深度睡眠。
- 水墨屏掉电保持画面，睡眠期间无需供电即可继续显示。
- 采样/刷新周期由 `config.h` 的 `WAKE_INTERVAL_SEC` 控制（默认 2 小时）。
- 三色屏全刷较慢（约 15–20s），刷新期间设备保持唤醒，刷新完成后立即进入睡眠。
- 使用 GxEPD2 的 `hibernate()` 在睡眠前让屏进入低功耗状态。

### 5.3 关键接口

```cpp
// display.h
void displayInit();
void displayRender(const SoilReading& r, float vbat, bool alarm,
                   const char* timeStr, const char* statusStr);
void displayHibernate();   // 睡眠前调用
```

---

## 6. 低功耗模块（power）

### 6.1 深度睡眠与唤醒

- 使用 `esp_sleep_enable_timer_wakeup()` + `esp_deep_sleep_start()`。
- 唤醒周期 = `WAKE_INTERVAL_SEC`（默认 7200s，可配置）。
- 睡眠期间 CPU、WiFi、外设全部关闭，实现微安级功耗。

### 6.2 告警标记掉电保持

- 告警锁存标记存于 `RTC_DATA_ATTR`，深度睡眠不丢失。
- 用途：防止重复告警（为将来 Server酱 推送预留，本期在屏上体现锁存状态）。
- 提供 `setAlarmLatched(bool)` / `getAlarmLatched()` 接口。

### 6.3 唤醒流程状态机（main.cpp 编排）

```
[深度睡眠] --RTC定时器唤醒-->
  (1) 采集湿度 + 电池电压
  (2) 告警判定（更新 RTC 锁存标记）
  (3) 刷新水墨屏
  (4) WiFi 连接（带超时）
       ├─ 成功 → NTP 同步时间 → 断开 WiFi
       └─ 失败 → 直接跳过，不阻塞
  (5) 屏进入 hibernate
  (6) 进入深度睡眠
```

> 说明：NTP 时间同步放在刷新屏幕之后；若首次上电尚未同步到时间，屏幕采样时间显示占位符（如「--」，或 RTC 上一次同步值），待同步后修正。可通过 `NTP_SYNC_EVERY_N_WAKES`（默认 1，即每次同步；可调大省电）控制同步频率。

---

## 7. 网络模块（network）

- 核心版网络仅用于 NTP 时间同步。
- WiFi 连接超时 `WIFI_CONNECT_TIMEOUT_SEC`（默认 10s），超时失败直接返回，不阻塞主流程。
- NTP 服务器：`pool.ntp.org`，时区 `CST-8`（UTC+8）。
- 时间通过标准 `time.h` + `configTime()` 获取，供 display 显示采样时间。
- 预留接口（本期不实现）：`mqttReport()`、`pushWechat()`，为后续 MQTT/Server酱 接入占位。

---

## 8. 告警逻辑模块（alarm）

```
if (湿度 < dryThreshold)      → 告警锁存置位（RTC 保存）
if (湿度 >= dryThreshold + 回差) → 清锁存
回差 = 10（百分比点）
```

- 锁存标记防止临界值反复报警（需求 4.3 第三条，虽推送暂缓，逻辑先实现并在屏上体现）。
- 告警状态供 display 决定红色显示，供后续推送模块判断是否触发推送。

---

## 9. 配置模块（config.h）

集中所有业务参数，改配置不动业务逻辑（需求 4.6）：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | 空 | WiFi 账号密码 |
| `NTP_SERVER` | `pool.ntp.org` | 时间服务器 |
| `TZ_OFFSET_SEC` | `8*3600` | 时区（UTC+8） |
| `WAKE_INTERVAL_SEC` | `7200` | 唤醒/采集/刷新周期（2 小时） |
| `SAMPLE_COUNT` | `8` | 每次采集平均次数 |
| `DRY_THRESHOLD` | `30` | 干旱告警阈值（%） |
| `HYSTERESIS` | `10` | 回差（百分比点） |
| `ADC_DRY` / `ADC_WET` | 待实测校准 | 传感器校准参数 |
| `NTP_SYNC_EVERY_N_WAKES` | `1` | 每 N 次唤醒同步一次时间 |
| `WIFI_CONNECT_TIMEOUT_SEC` | `10` | WiFi 连接超时 |
| `SENSOR_POWER_PIN` | `-1` | 传感器电源控制引脚（-1=未用） |
| `BATTERY_DIVIDER_RATIO` | `2.0` | 分压比（100k/100k → 2） |

---

## 10. 稳定性与非功能需求

- WiFi 连接失败有超时，不死机、不阻塞（需求 5.1）。
- 传感器采集做多次平均 + 硬件 104 电容抗跳变。
- 任何网络异常不阻塞采集→显示→睡眠主流程。
- 参数集中配置区，串口输出调试信息便于排障（需求 5.3）。

---

## 11. 测试策略

### 11.1 可脱离硬件的单元测试（PC 上跑）

- `sensor`：校准映射公式、边界约束（<0 与 >100 clamp）。
- `alarm`：阈值触发、回差清除、锁存不重复触发。
- 状态机：唤醒流程各分支（WiFi 成功/失败）的转移逻辑。

> 通过把纯逻辑（映射、阈值）与硬件 IO 分离，使这些函数可直接在宿主机编译测试；ADC/SPI 读写封装在薄驱动层。

### 11.2 硬件联调验证

- 串口打印 ADC 原始值 + 湿度%，完成干土/湿土校准（记录 `ADC_DRY`/`ADC_WET`）。
- 实机验证：唤醒→采集→刷屏→睡眠循环；拔电重上电后告警标记保持。
- 测量分压点电压与换算的电池电压一致性。

---

## 12. 风险与注意事项

1. 电容传感器必须 3.3V 供电，严禁 5V（需求风险 2）。
2. 电池分压电阻必须接好，否则 4.2V 直接进 GPIO35 会超量程（本文已按 100k/100k 设计）。
3. 水墨屏三色全刷耗时约 15–20s，期间保持唤醒，功耗设计需容忍该时长。
4. 睡眠期间 GPIO 常接传感器漏电，长续航建议启用 `SENSOR_POWER_PIN`（预留）。
5. GxEPD2 类名（`GxEPD2_213_B74` vs `_B73`）需按实际屏控制芯片（SSD1680）最终核实。

---

## 13. 未来扩展预留

- **光照传感器**：新增 `sensor_light` 模块，复用采集→显示→睡眠框架。
- **水泵自动浇水**：新增 `actuator_pump` 模块，在告警判定后触发。
- **Server酱 微信推送**：新增 `push` 模块，复用 `alarm` 锁存标记与 `network` 接口。
- **HomeAssistant MQTT**：新增 `mqtt` 模块，上报湿度 + 自动发现。
