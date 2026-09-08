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
