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
