# WiFi 网页配置门户 设计文档

日期：2026-09-20
状态：待评审

## 1. 背景与目标

当前 WiFi 凭据硬编码在 `include/config.h`，含真实 SSID/密码，且该文件处于未提交状态——一旦误执行 `git add .` 或分享代码就会泄露。

目标：

1. 从源码中移除 WiFi 凭据。
2. 提供 AP 网页配置门户，用户用手机浏览器配置 WiFi。
3. 配置持久化到 ESP32 的 NVS flash，断电不丢。
4. 保持现有"每 2 小时唤醒 → 采集 → 告警 → 同步时间 → 显示 → 深度睡眠"的电池供电工作模式不变。

## 2. 已确认决策

| 决策 | 结论 |
|---|---|
| 配置触发 | 自动回退：未配置 或 连接失败 → 进入 AP 门户 |
| 配置范围 | 仅 WiFi（SSID + 密码），其余参数仍留 `config.h` 编译期配置 |
| 配置超时 | 5 分钟无操作 → 自动深度睡眠，下次唤醒再进门户 |
| 存储 | ESP32 内置 `Preferences`（NVS 分区），命名空间 `wifi` |
| Web | 内置 `WebServer.h` + `DNSServer.h`（捕获门户），零第三方依赖 |

**明确不做（YAGNI）**：不引入 WiFiManager 库；不把 NTP/阈值/校准等参数搬进网页；配置模式下不在水墨屏显示提示（靠 AP 名称 + 串口日志）。

## 3. 架构

```
main.cpp (setup 编排)
   │
   ├─ wificfg  (flash 存取) ── Preferences / NVS
   ├─ network  (STA 连接 + NTP，改造后凭据由参数传入)
   └─ webcfg   (AP 门户: SoftAP + DNS 捕获 + WebServer + HTML)
```

- `wificfg` 只做 NVS 读写，不碰网络。
- `webcfg` 只做 AP 门户，不直接读写 NVS（通过 `wificfg::save`），不碰水墨屏。
- `network` 拆出 `connect()`（仅连接）与 `syncTime()`（仅 NTP），使 main 能区分"连接失败"与"NTP 失败"——只有连接失败才进门户。

## 4. 模块设计

### 4.1 `wifi_config.h/cpp` —— flash 存取

```cpp
#pragma once
#include <Arduino.h>

namespace wificfg {
bool hasConfig();                                   // NVS 中是否已保存非空 SSID
bool load(String &ssid, String &pass);              // 读取凭据，成功返回 true
bool save(const String &ssid, const String &pass);  // 保存凭据
void clear();                                       // 清除（恢复未配置状态）
} // namespace wificfg
```

- 命名空间 `wifi`，键 `ssid` / `pass`（均 String；`pass` 可为空以支持开放网络）。
- `hasConfig()` 判定：`ssid` 键存在且非空。
- 每次读写后 `Preferences.end()`，释放句柄。
- 关键日志前缀 `[wificfg]`。

### 4.2 `network.h/cpp` —— 改造

```cpp
#pragma once

namespace network {
bool connect(const char* ssid, const char* pass, int timeoutSec);  // 连接 WiFi，成功返回 true
bool syncTime();        // 在已连接状态下做 NTP 同步，成功返回 true
void disconnect();      // 断开 WiFi 并关闭射频
} // namespace network
```

- `connect()`：`WiFi.mode(WIFI_STA)` + `WiFi.begin(ssid, pass)`，轮询至连接或超时。日志沿用 `[net]`。
- `syncTime()`：`configTime(TZ_OFFSET_SEC, 0, NTP_SERVER, "ntp.aliyun.com")` + 轮询 `time(nullptr)`。不再负责连接。
- 拆分的意义：main 里连接失败 → 进门户；连接成功但 NTP 失败 → 照常显示（时间未同步），不进门户。

### 4.3 `web_config.h/cpp` —— AP 门户

```cpp
#pragma once
#include <stdint.h>

namespace webcfg {
// 进入 AP 配置门户，阻塞执行直到：
//   - 配置成功且验证连接通过 → 返回 true（调用方应 ESP.restart()）
//   - 超时无操作 → 返回 false（调用方进入深度睡眠）
bool runPortal(uint32_t timeoutSec);
} // namespace webcfg
```

实现要点：

- `WiFi.softAP(CONFIG_AP_SSID)`（无密码，AP IP 默认 192.168.4.1）。
- `DNSServer` 启动，把任意域名解析到 192.168.4.1（捕获门户：手机连上后任意网页都弹配置页）。
- `WebServer` 监听 80 端口，两个路由：
  - `GET /` → 返回 HTML 表单。
  - `POST /save` → 取 `ssid`/`pass` 参数 → `wificfg::save` → 用新凭据验证连接 → 成功返回"配置成功，设备将重启"页面并置成功标志；失败返回错误信息 + 表单重填。
- 主循环：`server.handleClient()` + `dns.processNextRequest()`，用 `millis()` 计时，超时返回 false。
- **验证连接失败的回退**：`WiFi.begin(新凭据)` 会把设备从 AP 切到 STA；验证失败后需 `WiFi.disconnect(true)` 再 `WiFi.softAP(...)` 重新开 AP，继续门户循环。
- 配置成功后 `runPortal` 返回 true，由 main 调用 `ESP.restart()` 走正常流程。
- `WebServer`/`DNSServer` 用函数内 `static` 局部对象，避免主循环任务栈压力。
- 关键日志前缀 `[webcfg]`。

### 4.4 `main.cpp` —— 编排

```cpp
// 0. 读凭据；首次无配置直接进门户
String ssid, pass;
if (!wificfg::load(ssid, pass)) {
    Serial.println("[main] 首次启动，进入配置门户");
    if (!webcfg::runPortal(CONFIG_PORTAL_TIMEOUT_SEC)) { power::begin(); power::enterDeepSleep(); }
    ESP.restart();                       // 配置成功 → 重启走正常流程，不返回
}

// 1. 采集 → 2. 告警（现有逻辑不变）
// ...

// 3. NTP 同步（每 N 次唤醒才联网，保持省电）
g_wakeCount++;
if (g_wakeCount % NTP_SYNC_EVERY_N_WAKES == 0) {
    if (!network::connect(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_SEC)) {
        Serial.println("[main] WiFi 连接失败，进入配置门户");
        if (!webcfg::runPortal(CONFIG_PORTAL_TIMEOUT_SEC)) { power::begin(); power::enterDeepSleep(); }
        ESP.restart();                   // 不返回
    }
    bool timeOk = network::syncTime();
    network::disconnect();
    Serial.printf("[main] 时间同步=%s\n", timeOk ? "成功" : "失败");
} else {
    Serial.println("[main] 本次跳过 NTP（非同步周期）");
}

// 4. 显示 → 5. 睡眠（现有逻辑）
```

- `connect()` 仅发生在 NTP 同步周期内，保持现有"每 N 次唤醒同步一次"的省电设计，不引入额外联网。
- 连接失败进门户会中断本次显示（超时睡眠则屏幕保持上次内容），属可接受行为。
- 现有 `network::syncTime(WIFI_CONNECT_TIMEOUT_SEC)` 调用点相应调整为 `connect` + `syncTime` 两段。

### 4.5 `config.h` 改动

- **删除** `WIFI_SSID` / `WIFI_PASSWORD`（敏感信息源头）。
- **删除** `DISPLAY_TITLE`（上一轮 UI 重构后已无用）。
- **新增**：
  - `#define CONFIG_AP_SSID "SoilMoisture-Setup"`（AP 名称）
  - `#define CONFIG_PORTAL_TIMEOUT_SEC 300`（配置门户超时）
- 保留 `WIFI_CONNECT_TIMEOUT_SEC`（10s）、`NTP_SERVER`、`TZ_OFFSET_SEC` 等非敏感项。

## 5. NVS 键结构

| 命名空间 | 键 | 类型 | 说明 |
|---|---|---|---|
| `wifi` | `ssid` | String | WiFi 名称，非空即视为已配置 |
| `wifi` | `pass` | String | WiFi 密码，可为空（开放网络） |

## 6. HTML 页面

单页表单，手机友好，UTF-8 中文。用 C++ raw string literal `R"raw(...)raw"` 内嵌。

```html
<!DOCTYPE html><html lang="zh"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi 配置</title>
<style>body{font-family:sans-serif;max-width:480px;margin:0 auto;padding:24px}
label{display:block;margin:12px 0 4px}input{width:100%;padding:10px;font-size:16px;box-sizing:border-box}
button{width:100%;padding:12px;font-size:16px;margin-top:16px}</style>
</head><body>
<h2>花卉湿度监测 · WiFi 配置</h2>
<form action="/save" method="post">
<label>WiFi 名称 (SSID)</label><input name="ssid" required>
<label>WiFi 密码</label><input name="pass" type="password">
<button type="submit">保存并连接</button>
</form>
</body></html>
```

`/save` 成功页 / 失败页为简短的纯文本或 HTML 片段，成功页提示"配置成功，设备即将重启"，失败页提示"连接失败，请检查 SSID/密码"并允许返回重填。

## 7. 启动流程

```text
上电(唤醒)
  ├─ 读 NVS 凭据
  │    ├─ 无凭据 ──→ AP 配置门户（5 分钟超时）
  │    │              ├─ 配置成功 → restart
  │    │              └─ 超时 → 深度睡眠，下次唤醒再进
  │    └─ 有凭据 → 采集 → 告警
  │                  └─ 是否同步周期？(每 N 次唤醒)
  │                        ├─ 否 → 显示(时间未同步) → 睡眠
  │                        └─ 是 → connect()（10s 超时）
  │                               ├─ 成功 → syncTime() → 显示 → 睡眠
  │                               └─ 失败 → AP 配置门户（同上）
```

## 8. 错误处理

| 场景 | 行为 |
|---|---|
| 首次上电（无配置） | 进 AP 门户 |
| 连接超时（路由器离线/换密码） | 进 AP 门户 |
| 连接成功但 NTP 失败 | 照常显示（时间未同步），不进门户 |
| 门户内用户填错密码 | 返回失败页 + 重填，继续门户 |
| 门户超时（无人操作） | 睡眠，下次唤醒再进 |
| NVS 读写失败 | 日志报错，按"未配置"处理进门户 |

## 9. 测试策略

`native` 环境无法运行 `Preferences`/`WiFi`/`WebServer`，自动化单元测试覆盖有限，故以编译 + 串口日志 + 手动验收为主：

1. **编译**：`pio run -e esp32s3` 通过（无告警）。
2. **串口日志**：观察 `[wificfg]`/`[net]`/`[webcfg]`/`[main]` 各阶段日志是否符合预期。
3. **手动验收清单**：
   - 首次烧录（NVS 空）→ 进 AP `SoilMoisture-Setup` → 手机连上弹配置页 → 填正确 WiFi → 保存 → 重启 → 正常采集显示。
   - 故意填错密码 → 返回失败页可重填。
   - 断掉路由器 → 唤醒后进门户 → 不操作 5 分钟 → 自动睡眠。
   - 恢复路由器 → 下次唤醒正常连接。

## 10. 文件清单

新增：

- `src/wifi_config.h` / `src/wifi_config.cpp`
- `src/web_config.h` / `src/web_config.cpp`

修改：

- `src/network.h` / `src/network.cpp`（拆 `connect`/`syncTime`）
- `src/main.cpp`（编排配置检测 + 分流）
- `include/config.h`（删凭据、加 AP 名称与超时）

不改 `platformio.ini`：`Preferences`/`WebServer`/`DNSServer` 均为 ESP32 Arduino core 内置。

## 11. 风险与取舍

- **明文存储**：NVS 存明文密码（`Preferences` 无加密）。家庭 WiFi 场景可接受；若需加密需引入 `nvs_flash` 加密或预配令牌，超出本次范围。
- **连接失败进门户的打扰**：路由器临时离线也会触发门户，但被"5 分钟超时睡眠"兜底，且设备 2 小时才醒一次，影响有限。
- **配置期间不采集**：进门户时跳过本次采集周期。对低频监测设备可接受，配置成功重启后立即恢复正常。
