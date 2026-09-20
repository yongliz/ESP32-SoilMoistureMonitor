# ESP32 花卉土壤湿度实时监控系统

基于 **ESP32-S3** 的花卉盆栽土壤湿度监控系统：周期采集土壤湿度，在 2.13 寸红黑白三色水墨屏上本地显示湿度、状态、采样时间与电池电压；低于阈值时触发「需要浇水」告警（告警状态跨深度睡眠锁存）。系统大部分时间处于深度睡眠，以微安级功耗运行，适合电池供电的盆栽养护场景。

> 本项目按 [superpowers](docs/superpowers/plans/2026-09-08-soil-moisture-monitor.md) 的「头脑风暴 → 写计划 → 子代理驱动开发（SDD）」流程实现，九个任务逐项完成并经独立审查。

---

## 目录

- [功能特性](#功能特性)
- [硬件清单](#硬件清单)
- [引脚接线](#引脚接线)
- [快速开始](#快速开始)
- [配置参数](#配置参数)
- [传感器校准](#传感器校准)
- [显示界面](#显示界面)
- [系统架构](#系统架构)
- [目录结构](#目录结构)
- [测试](#测试)
- [已知限制与注意事项](#已知限制与注意事项)
- [相关文档](#相关文档)

---

## 功能特性

### 已实现（MVP）

| 模块 | 说明 |
|---|---|
| 土壤湿度采集 | 电容式传感器 ADC 采样，多次平均抗跳变，两点线性校准映射为 0–100% |
| 告警锁存 | 湿度 < 阈值触发告警；回升超过「阈值 + 回差」才清除，避免临界值反复告警 |
| 水墨屏显示 | 本地显示标题、湿度百分比、状态、采样时间、电池电压；告警态红色显示 |
| 深度睡眠 | RTC 定时唤醒（默认 2 小时），休眠期 CPU/WiFi 关闭；告警标记存 `RTC_DATA_ATTR` 掉电不丢 |
| WiFi 网页配置 + NTP 时间同步 | WiFi 凭据通过 AP 网页门户配置并存入 NVS flash；联网校时，失败跳过不阻塞主流程 |
| 串口调试输出 | 输出 ADC 原始值、湿度、唤醒次数等，便于校准排障 |

### 暂缓实现（需求文档已规划，代码未实现）

- Server酱 微信推送
- HomeAssistant MQTT 接入 / 自动发现

---

## 硬件清单

| 器件 | 规格 | 用途 |
|---|---|---|
| 主控 | ESP32-S3（PlatformIO `esp32-s3-devkitc-1`） | 主控单元 |
| 土壤湿度传感器 | FC-28 电容式（**禁止电阻式**，易腐蚀） | 采集土壤湿度 |
| 水墨屏 | SES2213JS0E1 / DEPG0213RWS800F41，2.13 寸红黑白三色，212×104，UC8151D 协议 | 本地显示 |
| 电池套件 | 18650 + TP4056 充放电模块 | 供电 |
| 其他 | 杜邦线、100nF（104）瓷片电容 | 连接、滤波 |

---

## 引脚接线

目标板为 **ESP32-S3**，具体接线如下（与 [doc/接线说明.md](doc/接线说明.md) 一致）：

### 电容土壤湿度传感器（FC-28，3.3V）

| 传感器 | ESP32-S3 |
|---|---|
| VCC | 3.3V（**严禁 5V**） |
| GND | GND |
| AO | GPIO1（ADC1_CH0） |

> AO 与 GND 之间并联一个 104(100nF) 瓷片电容，抑制数值跳变。

### 水墨屏（2.13 寸三色，SPI，写-only）

| 屏 | ESP32-S3 |
|---|---|
| SCK | GPIO12 |
| MOSI | GPIO11 |
| CS | GPIO10 |
| DC | GPIO13 |
| RST | GPIO14 |
| BUSY | GPIO15 |
| VCC | 3.3V |
| GND | GND |

> - 屏为**写-only**，无需 MISO；DC 复用 GPIO13（S3 FSPI 默认 MISO 位置，被 DC 输出覆盖）。
> - BUSY 低电平有效（0 = 忙）。

### 电池（18650 + TP4056 分压）

| 信号 | ESP32-S3 |
|---|---|
| 分压中点（100k 电池正极 + 100k GND） | GPIO2（ADC1_CH1） |

> 分压比 2:1，4.2V 满电时中点约 2.1V，在 `ADC_11db` 量程内。

---

## 快速开始

### 1. 前置条件

- [PlatformIO](https://platformio.org/)（VS Code 扩展或 CLI）
- 一块 ESP32-S3 开发板 + 上述硬件
- USB 数据线

### 2. 配置 WiFi（首次使用）

WiFi 凭据不再硬编码，首次烧录后设备会进入 AP 配置门户：

1. 上电后设备自动开启热点 **`SoilMoisture-Setup`**（无密码）。
2. 手机连接该热点，浏览器会弹出配置页（访问任意网页均可触发）。
3. 填入你的 WiFi 名称与密码，点击「保存并连接」。
4. 验证通过后设备自动重启并进入正常工作流程；配置保存在 ESP32 的 NVS flash 中，掉电不丢。

> 若 WiFi 连接失败（如路由器离线/换密码），设备下次唤醒会重新进入配置门户；门户无操作 5 分钟（`CONFIG_PORTAL_TIMEOUT_SEC`）后自动进入深度睡眠。

### 3. 编译

```bash
pio run
```

目标环境为 `esp32s3`（见 [platformio.ini](platformio.ini)）。

### 4. 烧录

```bash
pio run -t upload
```

### 5. 查看串口输出

```bash
pio device monitor -b 115200
```

每次唤醒会打印类似：

```
[main] 第 1 次唤醒
[sensor] ADC=2140 湿度=42.5%
[net] WiFi 已连接 IP=192.168.1.100
[main] 时间同步成功
[power] 进入深度睡眠
```

---

## 配置参数

除 WiFi 凭据（通过 AP 门户配置）外，其余可调参数集中在 [include/config.h](include/config.h)，修改后重新烧录即可，无需改动业务逻辑。

| 宏 | 默认值 | 说明 |
|---|---|---|
| `CONFIG_AP_SSID` | `"SoilMoisture-Setup"` | 配置门户 AP 名称 |
| `CONFIG_PORTAL_TIMEOUT_SEC` | `300` | 门户无操作超时（秒） |
| `WIFI_CONNECT_TIMEOUT_SEC` | `10` | WiFi 连接超时（秒），失败直接休眠不阻塞 |
| `NTP_SERVER` | `"pool.ntp.org"` | NTP 服务器 |
| `TZ_OFFSET_SEC` | `8 * 3600` | 时区偏移（UTC+8 北京时间） |
| `NTP_SYNC_EVERY_N_WAKES` | `1` | 每 N 次唤醒同步一次时间 |
| `WAKE_INTERVAL_SEC` | `7200` | 唤醒周期（秒），默认 2 小时 |
| `SAMPLE_COUNT` | `8` | 每次采集平均次数 |
| `DRY_THRESHOLD` | `30.0f` | 干旱告警阈值（%） |
| `HYSTERESIS` | `10.0f` | 告警回差（百分点），湿度回升超过「阈值+回差」才清除告警 |
| `ADC_DRY` / `ADC_WET` | `1600` / `2800` | 传感器两点校准值（见下方校准） |
| `SOIL_MOISTURE_ADC_PIN` | `1` | 土壤湿度 AO 引脚（GPIO1） |
| `BATTERY_ADC_PIN` | `2` | 电池分压引脚（GPIO2） |
| `ADC_ATTENUATION` | `ADC_11db` | ADC 衰减 |
| `BATTERY_DIVIDER_RATIO` | `2.0f` | 电池分压比（100k/100k） |
| `SENSOR_POWER_PIN` | `-1` | 传感器电源控制脚（`-1`=未用；接 MOSFET 开关可省电） |

---

## 传感器校准

湿度百分比依赖 `ADC_DRY`（干土 ADC 读数）与 `ADC_WET`（泡水 ADC 读数）的两点线性映射。出厂默认值仅作占位，**必须实测校准**：

1. 烧录固件，打开串口监视器（115200）。
2. **干土校准**：探头置于空气中完全干燥，稳定后记录串口 `ADC` 值，填入 `ADC_DRY`。
3. **湿土校准**：探头完全泡水，稳定后记录 `ADC` 值，填入 `ADC_WET`。
4. 重新烧录，干土应显示 ~0%，泡水应显示 ~100%。

> 若你的传感器方向相反（干读数高、湿读数低），交换 `ADC_DRY` 与 `ADC_WET` 即可。

详细步骤见 [doc/校准说明.md](doc/校准说明.md)。

---

## 显示界面

横屏 212×104 布局：

```
       花卉湿度监测          ← 标题（居中）
          42%               ← 湿度大字（2x）
          正常              ← 状态：正常 / 需要浇水（告警时红色）
 09-08 14:23       3.98V    ← 时间（左）/ 电池电压（右）
```

- 未同步时间时，时间位置显示「未同步时间」占位符。
- 告警状态（需要浇水）以**红色**显示，正常为黑色。

---

## 系统架构

五层架构（MVP 实现前三层 + 网络校时）：

```
┌─ 采集层    sensor.cpp        ADC 采样 → 湿度映射 / 电池电压
├─ 逻辑层    alarm.h          告警锁存（阈值 + 回差）
│            sensor_math.h    线性映射 / 钳位 / 分压换算
├─ 交互层    display.cpp      212×104 三色水墨屏（自定义 UC8151D SPI 驱动 + 点阵字库）
├─ 网络层    network.cpp      WiFi 连接 + NTP 校时（凭据来自 wifi_config，连接失败进 web_config 门户）
│            wifi_config.cpp  WiFi 凭据 NVS 存取
│            web_config.cpp   AP 网页配置门户（SoftAP + 捕获 DNS + WebServer）
└─ 电源层    power.cpp        深度睡眠 + RTC 锁存
```

### 唤醒流程（[src/main.cpp](src/main.cpp)）

```
上电/唤醒
  → sensor::begin() + 采集（多次平均）
  → 读 RTC 告警锁存 → alarmctl::evaluate 判定 → 写回锁存
  → (每 N 次唤醒) WiFi 连接 + NTP 校时，失败跳过
  → display 刷新（湿度/状态/时间/电压）
  → power::begin() 配置定时唤醒 → 进入深度睡眠
```

### 告警锁存逻辑（[lib/core/alarm.h](lib/core/alarm.h)）

- 湿度 < `DRY_THRESHOLD`：置位锁存（触发告警）。
- 湿度 ≥ `DRY_THRESHOLD + HYSTERESIS`：清除锁存。
- 中间区间（死区）：保持原状态，避免临界抖动反复告警。
- 锁存标记存 `RTC_DATA_ATTR`，深度睡眠不丢，掉电才复位。

---

## 目录结构

```
ESP32-SoilMoistureMonitor/
├── include/
│   └── config.h                 # 可配置参数（阈值/校准/引脚；WiFi 凭据走门户）
├── lib/
│   └── core/
│       ├── alarm.h              # 告警锁存判定（头文件内联，纯逻辑可测试）
│       └── sensor_math.h        # 湿度线性映射 / 电池电压换算（纯逻辑可测试）
├── src/
│   ├── main.cpp                 # 唤醒流程编排
│   ├── sensor.h / .cpp          # ADC 采集层
│   ├── display.h / .cpp         # 水墨屏驱动（UC8151D 协议 + 旋转映射）
│   ├── font.h                   # 内嵌中英文点阵字库
│   ├── network.h / .cpp         # WiFi 连接 + NTP
│   ├── wifi_config.h / .cpp     # WiFi 凭据 NVS 存取
│   ├── web_config.h / .cpp      # AP 网页配置门户
│   └── power.h / .cpp           # 深度睡眠 + RTC 锁存
├── test/                        # 原生单元测试（Unity）
│   ├── test_alarm/
│   ├── test_sensor_math/
│   └── test_smoke/
├── doc/                         # 需求 / 接线 / 校准文档
├── docs/superpowers/            # 设计 spec 与实现计划
├── platformio.ini               # esp32s3 + native 双环境
└── README.md
```

---

## 测试

纯逻辑模块（`alarm`、`sensor_math`）带原生单元测试，无需硬件即可运行：

```bash
pio test -e native
```

当前 **14/14 通过**（alarm 5 + sensor_math 8 + smoke 1）。

---

## 已知限制与注意事项

1. **电池电压偏高约 6%**：代码 `sensor.cpp` 以 3.3V 满量程换算，而 ESP32-S3 的 `ADC_11db` 满量程约 3.1V，且无 eFuse 校准，屏显电压可能比万用表实测偏高。需精确时按 [doc/校准说明.md](doc/校准说明.md) 用万用表修正 `BATTERY_DIVIDER_RATIO` 或 `3.3f` 基准。
2. **系统时间深睡后回零**：RTC 只做唤醒，不保留系统时间；因默认 `NTP_SYNC_EVERY_N_WAKES=1` 每醒校时，屏显时间始终有效。若调大该值，未同步期间显示「未同步时间」。
3. **告警锁存掉电才复位**：深度睡眠/软复位都保留锁存；如需强制复位可断电重上电。
4. **传感器漏电**：深度睡眠时若传感器常接电源会漏电，长续航建议接 MOSFET 由 `SENSOR_POWER_PIN` 控制断电（当前 `-1` 未启用）。
5. **暂缓功能**：Server酱 推送、HomeAssistant MQTT 均未实现（需求文档已规划）。
6. **需求文档引脚为旧值**：`doc/需求文档.md` 中的 GPIO34/35/18-23 为早期 WROOM-32 假设，实际以 ESP32-S3 接线为准（见上文「引脚接线」）。

---

## 相关文档

| 文档 | 路径 |
|---|---|
| 需求分析 | [doc/需求文档.md](doc/需求文档.md) |
| 硬件接线 | [doc/接线说明.md](doc/接线说明.md) |
| 传感器校准 | [doc/校准说明.md](doc/校准说明.md) |
| 设计规格 | [docs/superpowers/specs/2026-09-08-soil-moisture-monitor-design.md](docs/superpowers/specs/2026-09-08-soil-moisture-monitor-design.md) |
| 实现计划 | [docs/superpowers/plans/2026-09-08-soil-moisture-monitor.md](docs/superpowers/plans/2026-09-08-soil-moisture-monitor.md) |
| 开发台账 | [.superpowers/sdd/2026-09-08-soil-moisture-monitor/progress.md](.superpowers/sdd/2026-09-08-soil-moisture-monitor/progress.md) |
| 设计规格（WiFi 配置门户） | [docs/superpowers/specs/2026-09-20-wifi-config-portal-design.md](docs/superpowers/specs/2026-09-20-wifi-config-portal-design.md) |
| 实现计划（WiFi 配置门户） | [docs/superpowers/plans/2026-09-20-wifi-config-portal.md](docs/superpowers/plans/2026-09-20-wifi-config-portal.md) |
| 开发台账（WiFi 配置门户） | [.superpowers/sdd/2026-09-20-wifi-config-portal/progress.md](.superpowers/sdd/2026-09-20-wifi-config-portal/progress.md) |
