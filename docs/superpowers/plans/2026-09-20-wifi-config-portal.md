# WiFi 配置门户 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 AP 网页门户替代硬编码 WiFi 凭据，配置持久化到 NVS flash。

**Architecture:** 新增 `wifi_config`（NVS 存取）与 `web_config`（AP 门户）两个模块；把 `network` 拆成 `connect`/`syncTime` 两段；`main` 在「首次无配置」或「连接失败」时进入配置门户。

**Tech Stack:** ESP32-S3 / Arduino core 2.0.17 / PlatformIO / Preferences(NVS) / WebServer / DNSServer

**Spec:** `docs/superpowers/specs/2026-09-20-wifi-config-portal-design.md`

## Global Constraints

- **测试约束**：`Preferences`/`WiFi`/`WebServer`/`DNSServer` 均为 ESP32 专有，native（unity）环境无法运行。故本计划每任务以编译通过为验证，无自动化单元测试；功能验证靠串口日志 + 任务 4 末尾的手动验收清单。
- **编译命令**：`~/.platformio/penv/Scripts/pio.exe run -e esp32s3`（`pio` 不在 PATH）。预期输出 `[SUCCESS]`。
- **日志前缀**：`[wificfg]` / `[net]` / `[webcfg]` / `[main]`。
- **Git 安全（硬性）**：绝不 `git add .` / `git commit -am`；每次只 `git add` 指定文件。`include/config.h` 在 Task 2 删除 `WIFI_SSID`/`WIFI_PASSWORD` 之前仍含真实密码，禁止提交；删除后方可提交。
- **绝不把真实 WiFi 密码写入任何文档或提交**。本计划与 spec 均不记录密码值；执行 config.h 编辑时以磁盘文件实际内容为准。
- **常量（verbatim）**：`CONFIG_AP_SSID "SoilMoisture-Setup"`、`CONFIG_PORTAL_TIMEOUT_SEC 300`、命名空间 `wificfg`/`network`/`webcfg`。

---

### Task 1: wifi_config 模块（NVS 存取）

**Files:**
- Create: `src/wifi_config.h`
- Create: `src/wifi_config.cpp`

**Interfaces:**
- Produces: `wificfg::hasConfig()`, `wificfg::load(String&, String&)`, `wificfg::save(const String&, const String&)`, `wificfg::clear()`

- [ ] **Step 1: 写 `src/wifi_config.h`**

```cpp
#pragma once
#include <Arduino.h>

// WiFi 凭据的 NVS 持久化（命名空间 "wifi"，键 ssid/pass）
namespace wificfg {

bool hasConfig();                                   // NVS 中是否已保存非空 SSID
bool load(String &ssid, String &pass);              // 读取凭据，成功返回 true
bool save(const String &ssid, const String &pass);  // 保存凭据
void clear();                                       // 清除（恢复未配置状态）

} // namespace wificfg
```

- [ ] **Step 2: 写 `src/wifi_config.cpp`**

```cpp
#include "wifi_config.h"
#include <Preferences.h>

namespace wificfg {

static const char* NS = "wifi";
static const char* K_SSID = "ssid";
static const char* K_PASS = "pass";

bool hasConfig() {
    Preferences p;
    if (!p.begin(NS, true)) {
        Serial.println("[wificfg] NVS 打开失败(只读)");
        return false;
    }
    bool ok = p.isKey(K_SSID) && p.getString(K_SSID, "").length() > 0;
    p.end();
    return ok;
}

bool load(String &ssid, String &pass) {
    Preferences p;
    if (!p.begin(NS, true)) {
        Serial.println("[wificfg] NVS 打开失败(只读)");
        return false;
    }
    ssid = p.getString(K_SSID, "");
    pass = p.getString(K_PASS, "");
    p.end();
    if (ssid.length() == 0) {
        Serial.println("[wificfg] 未找到已保存的 SSID");
        return false;
    }
    Serial.printf("[wificfg] 已加载凭据: SSID=%s\n", ssid.c_str());
    return true;
}

bool save(const String &ssid, const String &pass) {
    Preferences p;
    if (!p.begin(NS, false)) {
        Serial.println("[wificfg] NVS 打开失败(读写)");
        return false;
    }
    p.putString(K_SSID, ssid);
    p.putString(K_PASS, pass);
    p.end();
    Serial.printf("[wificfg] 已保存凭据: SSID=%s\n", ssid.c_str());
    return true;
}

void clear() {
    Preferences p;
    if (!p.begin(NS, false)) return;
    p.clear();
    p.end();
    Serial.println("[wificfg] 已清除凭据");
}

} // namespace wificfg
```

- [ ] **Step 3: 编译验证**

Run: `~/.platformio/penv/Scripts/pio.exe run -e esp32s3`
Expected: `[SUCCESS]`（新模块尚未被 main 引用，不影响现有代码）

- [ ] **Step 4: 提交**

```bash
git add src/wifi_config.h src/wifi_config.cpp
git commit -m "feat: add wifi credential NVS storage"
```

---

### Task 2: 拆分 network 接口 + 删除硬编码凭据 + main 改用 NVS

**Files:**
- Modify: `src/network.h`
- Modify: `src/network.cpp`
- Modify: `include/config.h`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `wificfg::load(String&, String&)`（Task 1）
- Produces: `network::connect(const char*, const char*, int)`, `network::syncTime()`, `network::disconnect()`

- [ ] **Step 1: 重写 `src/network.h`**

```cpp
#pragma once

namespace network {

bool connect(const char* ssid, const char* pass, int timeoutSec);  // 连接 WiFi，成功返回 true
bool syncTime();        // 在已连接状态下做 NTP 同步，成功返回 true
void disconnect();      // 断开 WiFi 并关闭射频

} // namespace network
```

- [ ] **Step 2: 重写 `src/network.cpp`**

把原 `syncTime()` 中的「连接」逻辑拆到 `connect()`，`syncTime()` 只保留 `configTime` + 轮询。

```cpp
#include "network.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace network {

bool connect(const char* ssid, const char* pass, int timeoutSec) {
    Serial.printf("[net] 连接 WiFi: SSID=%s\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > (unsigned long)timeoutSec * 1000UL) {
            Serial.printf("[net] WiFi 连接超时(%d s)，状态码=%d\n",
                          timeoutSec, (int)WiFi.status());
            return false;
        }
        delay(250);
    }
    Serial.printf("[net] WiFi 已连接: 用时=%lu ms IP=%s RSSI=%d dBm\n",
                  (unsigned long)(millis() - start),
                  WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    return true;
}

bool syncTime() {
    configTime(TZ_OFFSET_SEC, 0, NTP_SERVER, "ntp.aliyun.com");

    int retries = 0;
    time_t now = time(nullptr);
    while (now < 100000 && retries < 40) {   // 1970 年说明尚未同步
        delay(250);
        now = time(nullptr);
        retries++;
    }
    if (now >= 100000) {
        struct tm t;
        localtime_r(&now, &t);
        Serial.printf("[net] NTP 同步成功: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        Serial.println("[net] NTP 同步失败，时间未更新");
    }
    return now >= 100000;
}

void disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

} // namespace network
```

- [ ] **Step 3: 改 `include/config.h`——删除凭据宏、加门户常量**

两处修改：

（a）把 `// ============ WiFi ============` 区块下的两行 `#define WIFI_SSID ...` 和 `#define WIFI_PASSWORD ...`（当前为真实凭据，**以磁盘实际内容为准，勿复制到别处**）整段删除，替换为：

```cpp
// ============ WiFi / 配置门户 ============
#define CONFIG_AP_SSID "SoilMoisture-Setup"   // 配置门户 AP 名称
#define CONFIG_PORTAL_TIMEOUT_SEC 300         // 门户无操作超时(秒)
```

（b）删除文件末尾 `// ============ 其他 ============` 区块（含已无用的 `#define DISPLAY_TITLE ...` 一行）。

- [ ] **Step 4: 改 `src/main.cpp` 的 NTP 段 + include**

（a）在 include 区加一行（放在 `#include "network.h"` 之后）：

```cpp
#include "wifi_config.h"
```

（b）在 `setup()` 开头日志之后、`// 1. 采集` 之前，插入读取凭据：

```cpp
    // 0. 读取 WiFi 凭据（配置门户在后续任务接入）
    String ssid, pass;
    bool haveCreds = wificfg::load(ssid, pass);
    if (!haveCreds) Serial.println("[main] 未找到 WiFi 配置");
```

（c）把现有「3. NTP 时间同步」段替换为：

```cpp
    // 3. NTP 时间同步（每 N 次唤醒才联网）
    g_wakeCount++;
    if (g_wakeCount % NTP_SYNC_EVERY_N_WAKES == 0) {
        if (haveCreds &&
            network::connect(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_SEC)) {
            bool ok = network::syncTime();
            Serial.printf("[main] 时间同步=%s\n", ok ? "成功" : "失败");
        } else {
            Serial.println("[main] WiFi 连接失败或未配置，本次不更新时间");
        }
        network::disconnect();
    } else {
        Serial.println("[main] 本次跳过 NTP（非同步周期）");
    }
```

- [ ] **Step 5: 编译验证**

Run: `~/.platformio/penv/Scripts/pio.exe run -e esp32s3`
Expected: `[SUCCESS]`。此时设备已能从 NVS 读凭据连接（首次无配置则连接失败，走「不更新时间」路径，属中间态）。

- [ ] **Step 6: 提交**

```bash
git add src/network.h src/network.cpp include/config.h src/main.cpp
git commit -m "refactor: split network connect/syncTime, load WiFi creds from NVS"
```

> 注意：此提交后 `config.h` 已不含 WiFi 密码，可安全提交。

---

### Task 3: web_config 模块（AP 门户）

**Files:**
- Create: `src/web_config.h`
- Create: `src/web_config.cpp`

**Interfaces:**
- Consumes: `wificfg::save(const String&, const String&)`（Task 1）、`network::connect(...)` / `network::disconnect()`（Task 2）、`CONFIG_AP_SSID` / `CONFIG_PORTAL_TIMEOUT_SEC` / `WIFI_CONNECT_TIMEOUT_SEC`（config.h）
- Produces: `webcfg::runPortal(uint32_t) -> bool`

- [ ] **Step 1: 写 `src/web_config.h`**

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

- [ ] **Step 2: 写 `src/web_config.cpp`**

```cpp
#include "web_config.h"
#include "config.h"
#include "wifi_config.h"
#include "network.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

namespace webcfg {

static WebServer server(80);
static DNSServer dns;
static bool g_saved = false;

static const char PAGE_INDEX[] = R"raw(
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
)raw";

static void handleRoot() {
    server.send(200, "text/html", PAGE_INDEX);
}

static void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    ssid.trim();
    if (ssid.length() == 0) {
        server.send(400, "text/html",
                    "<meta charset='utf-8'><h3>SSID 不能为空</h3><a href='/'>返回重试</a>");
        return;
    }

    wificfg::save(ssid, pass);
    Serial.printf("[webcfg] 验证连接: SSID=%s\n", ssid.c_str());
    if (network::connect(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_SEC)) {
        network::disconnect();
        server.send(200, "text/html",
                    "<meta charset='utf-8'><h3>配置成功，设备即将重启</h3>");
        g_saved = true;
    } else {
        // 验证失败：切回 AP 模式继续门户
        WiFi.mode(WIFI_AP);
        WiFi.softAP(CONFIG_AP_SSID);
        server.send(200, "text/html",
                    "<meta charset='utf-8'><h3>连接失败，请检查 SSID/密码</h3>"
                    "<a href='/'>返回重试</a>");
    }
}

bool runPortal(uint32_t timeoutSec) {
    Serial.printf("[webcfg] 启动 AP 配置门户: %s\n", CONFIG_AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(CONFIG_AP_SSID);

    dns.start(53, "*", WiFi.softAPIP());
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.printf("[webcfg] Web 服务器已启动: http://%s\n",
                  WiFi.softAPIP().toString().c_str());

    g_saved = false;
    unsigned long start = millis();
    while (!g_saved) {
        server.handleClient();
        dns.processNextRequest();
        if (millis() - start > (unsigned long)timeoutSec * 1000UL) {
            Serial.println("[webcfg] 配置超时，退出门户");
            break;
        }
        delay(10);
    }

    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    if (g_saved) Serial.println("[webcfg] 配置完成");
    return g_saved;
}

} // namespace webcfg
```

- [ ] **Step 3: 编译验证**

Run: `~/.platformio/penv/Scripts/pio.exe run -e esp32s3`
Expected: `[SUCCESS]`（模块尚未被 main 调用，不影响现有代码）

- [ ] **Step 4: 提交**

```bash
git add src/web_config.h src/web_config.cpp
git commit -m "feat: add AP config portal with captive DNS"
```

---

### Task 4: main 接入配置门户

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `wificfg::load(String&, String&)`（Task 1）、`webcfg::runPortal(uint32_t)`（Task 3）、`network::connect(...)`（Task 2）、`power::begin()` / `power::enterDeepSleep()`

- [ ] **Step 1: 加 include**

在 include 区（`#include "wifi_config.h"` 之后）加：

```cpp
#include "web_config.h"
```

- [ ] **Step 2: 首次无配置 → 进门户**

把 Task 2 插入的「读取 WiFi 凭据」段替换为：

```cpp
    // 0. WiFi 配置检测：首次无配置直接进门户
    String ssid, pass;
    if (!wificfg::load(ssid, pass)) {
        Serial.println("[main] 首次启动，进入配置门户");
        if (!webcfg::runPortal(CONFIG_PORTAL_TIMEOUT_SEC)) {
            Serial.println("[main] 配置超时，进入睡眠");
            power::begin();
            power::enterDeepSleep();
        }
        ESP.restart();                       // 配置成功 → 重启走正常流程，不返回
    }
```

- [ ] **Step 3: 连接失败 → 进门户**

把「3. NTP 时间同步」段替换为：

```cpp
    // 3. NTP 时间同步（每 N 次唤醒；连接失败进门户）
    g_wakeCount++;
    if (g_wakeCount % NTP_SYNC_EVERY_N_WAKES == 0) {
        if (!network::connect(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_SEC)) {
            Serial.println("[main] WiFi 连接失败，进入配置门户");
            if (!webcfg::runPortal(CONFIG_PORTAL_TIMEOUT_SEC)) {
                Serial.println("[main] 配置超时，进入睡眠");
                power::begin();
                power::enterDeepSleep();
            }
            ESP.restart();                   // 不返回
        }
        bool ok = network::syncTime();
        Serial.printf("[main] 时间同步=%s\n", ok ? "成功" : "失败");
        network::disconnect();
    } else {
        Serial.println("[main] 本次跳过 NTP（非同步周期）");
    }
```

- [ ] **Step 4: 编译验证**

Run: `~/.platformio/penv/Scripts/pio.exe run -e esp32s3`
Expected: `[SUCCESS]`

- [ ] **Step 5: 提交**

```bash
git add src/main.cpp
git commit -m "feat: enter config portal on missing/failed WiFi config"
```

- [ ] **Step 6: 手动验收清单（烧录到 ESP32-S3）**

1. 首次烧录（NVS 空）→ 串口见 `[main] 首次启动，进入配置门户` → 手机搜到 AP `SoilMoisture-Setup` 连上 → 浏览器弹配置页 → 填正确 WiFi → 保存 → 见 `[webcfg] 配置完成` → 设备重启 → 正常采集显示。
2. 故意填错密码 → 返回 `连接失败，请检查 SSID/密码` 可重试。
3. 断开路由器 → 唤醒后 `[main] WiFi 连接失败，进入配置门户` → 不操作 5 分钟 → `[webcfg] 配置超时` → 自动睡眠。
4. 恢复路由器 → 下次唤醒正常连接。

---

## Self-Review

- **Spec 覆盖**：spec 的每个模块（4.1 wificfg→Task1、4.2 network→Task2、4.3 webcfg→Task3、4.4/4.5 main+config.h→Task2+Task4）、NVS 键（Task1）、HTML（Task3）、错误处理（Task3/4）、测试策略（各任务编译 + Task4 验收）均有对应任务。无遗漏。
- **类型一致性**：`wificfg::load(String&,String&)` 在 Task1 定义、Task2/4 使用，签名一致；`network::connect(const char*,const char*,int)` 在 Task2 定义、Task3/4 使用；`webcfg::runPortal(uint32_t)` 在 Task3 定义、Task4 使用。`ssid`/`pass` 为 `String`，`ssid.c_str()` 传参一致。
- **无占位符**：所有代码块均为完整可编译内容，config.h 删凭据处因安全约束用「以磁盘实际内容为准」描述（不写密码值），属有意为之而非占位。
