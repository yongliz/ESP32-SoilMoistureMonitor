#pragma once

// ============ WiFi / 配置门户 ============
#define CONFIG_AP_SSID "SoilMoisture-Setup"   // 配置门户 AP 名称
#define CONFIG_PORTAL_TIMEOUT_SEC 300         // 门户无操作超时(秒)

// ============ 时间同步 ============
#define NTP_SERVER "pool.ntp.org"
#define TZ_OFFSET_SEC (8 * 3600)  // UTC+8 中国标准时间
#define NTP_SYNC_EVERY_N_WAKES 1  // 每 N 次唤醒同步一次时间（必须 >=1）

// ============ 采集/唤醒 ============
#define WAKE_INTERVAL_SEC 7200  // 唤醒周期(秒)，默认 2 小时
#define SAMPLE_COUNT 8          // 每次采集平均次数

// ============ 告警阈值 ============
#define DRY_THRESHOLD 30.0f  // 干旱告警阈值(%)
#define HYSTERESIS 10.0f     // 回差(百分比点)

// ============ 传感器校准 ============
// 需实测：ADC_DRY=探头在干燥空气的ADC读数；ADC_WET=探头泡水读数
// 假设湿度越大 ADC 读数越高（电容式典型）。若你的传感器方向相反，交换两值即可。
#define ADC_DRY 1600
#define ADC_WET 2800

// ============ ADC 引脚 ============
// ESP32-S3 ADC1：土壤湿度 AO=GPIO1(ADC1_CH0)，电池分压=GPIO2(ADC1_CH1)
#define SOIL_MOISTURE_ADC_PIN 1
#define BATTERY_ADC_PIN 2
#define ADC_ATTENUATION ADC_11db
#define BATTERY_DIVIDER_RATIO 2.0f  // 100k/100k 分压比

// ============ WiFi 超时 ============
#define WIFI_CONNECT_TIMEOUT_SEC 10

// ============ 传感器电源控制(预留) ============
#define SENSOR_POWER_PIN -1  // -1 表示未使用；启用时接 MOSFET 开关

// ============ 水墨屏 ============
// 汉朔拆机 SES2213JS0E1 / DEPG0213RWS800F41（三色，UC8151D/IL0373 协议，自定义
// SPI 驱动） 物理可视：横向 212 × 纵向 104；控制器原生 RAM：X=104（13
// 字节/行）× Y=212（212 行） 参考驱动
// gitee.com/jetmie/evernote/open/idisplay/web/epd.py + index.html
#define EPD_WIDTH 212                      // 可视宽（横向像素）
#define EPD_HEIGHT 104                     // 可视高（纵向像素）
#define EPD_NATIVE_W 104                   // 控制器原生 X（像素，=13 字节/行）
#define EPD_NATIVE_H 212                   // 控制器原生 Y（像素，=行数）
#define EPD_BUF_STRIDE (EPD_NATIVE_W / 8)  // 每行字节数 = 13
#define EPD_BUF_SIZE (EPD_NATIVE_W * EPD_NATIVE_H / 8)  // 缓冲字节 = 2756
// ESP32-S3 接线（参考 utils.py S3 分支: EPD(12,11,10,13,14,15) =
// sck,mosi,cs,dc,rst,busy）
#define EPD_SCK_PIN 12
#define EPD_MOSI_PIN 11
#define EPD_CS_PIN 10
#define EPD_DC_PIN 13
#define EPD_RST_PIN 14
#define EPD_BUSY_PIN 15  // BUSY 低电平有效(0=忙)
