#include "display.h"
#include "config.h"
#include "font.h"
#include <Arduino.h>
#include <SPI.h>

namespace display {

enum : uint8_t { C_WHITE, C_RED, C_BLACK };

// 帧缓冲：black 1=白 0=黑；red 1=红 0=无（红数据直接发送：1=红）
static uint8_t g_black[EPD_BUF_SIZE];
static uint8_t g_red[EPD_BUF_SIZE];

// ---------- 底层 SPI ----------
static void writeCmd(uint8_t cmd) {
    digitalWrite(EPD_DC_PIN, LOW);
    digitalWrite(EPD_CS_PIN, LOW);
    SPI.transfer(cmd);
    digitalWrite(EPD_CS_PIN, HIGH);
}
static void writeData(uint8_t d) {
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_CS_PIN, LOW);
    SPI.transfer(d);
    digitalWrite(EPD_CS_PIN, HIGH);
}
static void waitBusy() {  // BUSY 低电平有效(0=忙)
    while (digitalRead(EPD_BUSY_PIN) == LOW) delay(10);
}
static void reset() {
    digitalWrite(EPD_RST_PIN, LOW); delay(50);
    digitalWrite(EPD_RST_PIN, HIGH); delay(50);
    waitBusy();
}

// ---------- 绘图 ----------
static void clearBuffer() {
    memset(g_black, 0xFF, sizeof(g_black));  // 全白
    memset(g_red, 0x00, sizeof(g_red));      // 无红
}

// 物理(横向 px, 纵向 py) → 控制器原生 RAM。
// 控制器 X 轴 = 物理纵向(垂直翻转)，控制器 Y 轴 = 物理横向。参考 index.html sendToEink 的字节轨迹。
static void drawPixel(int16_t px, int16_t py, uint8_t color) {
    if (px < 0 || px >= EPD_WIDTH || py < 0 || py >= EPD_HEIGHT) return;
    uint16_t xn = (uint16_t)(EPD_HEIGHT - 1 - py);   // 0..103
    uint16_t yn = (uint16_t)px;                       // 0..211
    uint16_t idx = yn * EPD_BUF_STRIDE + (xn >> 3);
    uint8_t  mask = 0x80 >> (xn & 7);
    if (color == C_BLACK) { g_black[idx] &= ~mask; g_red[idx] &= ~mask; }
    else if (color == C_RED) { g_red[idx] |= mask; g_black[idx] |= mask; }
    else { g_black[idx] |= mask; g_red[idx] &= ~mask; }  // 白
}

static void drawGlyph(const uint8_t* g, int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color) {
    uint8_t wb = (w + 7) / 8;
    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t xb = 0; xb < wb; xb++) {
            uint8_t b = g[row * wb + xb];
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (b & (0x80 >> bit)) drawPixel(x + xb * 8 + bit, y + row, color);
            }
        }
    }
}
static void drawAscii(char c, int16_t x, int16_t y, uint8_t color) {
    if (c < 0x20 || c > 0x7E) return;
    drawGlyph(FONT_ASCII_8X16[c - 0x20], x, y, 8, 16, color);
}
static void drawAsciiScaled(char c, int16_t x, int16_t y, uint8_t color, uint8_t scale) {
    if (c < 0x20 || c > 0x7E) return;
    const uint8_t* g = FONT_ASCII_8X16[c - 0x20];
    for (uint8_t row = 0; row < 16; row++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (g[row] & (0x80 >> bit)) {
                for (uint8_t dy = 0; dy < scale; dy++)
                    for (uint8_t dx = 0; dx < scale; dx++)
                        drawPixel(x + bit * scale + dx, y + row * scale + dy, color);
            }
        }
    }
}
static int findCn(const char* u3) {
    int idx = 0;
    for (const char* p = CN_CHARSET; *p; p += 3, idx++) {
        if (p[0] == u3[0] && p[1] == u3[1] && p[2] == u3[2]) return idx;
    }
    return -1;
}
static void drawText(const char* s, int16_t x, int16_t y, uint8_t color) {
    int16_t cx = x;
    while (*s) {
        if ((uint8_t)*s < 0x80) {
            if (*s >= 0x20) drawAscii(*s, cx, y, color);
            cx += 8;
            s++;
        } else {
            int idx = findCn(s);
            if (idx >= 0) drawGlyph(FONT_CN_16X16[idx], cx, y, 16, 16, color);
            cx += 16;
            s += 3;
        }
    }
}
static int textWidth(const char* s) {
    int w = 0;
    while (*s) {
        if ((uint8_t)*s < 0x80) { w += 8; s++; }
        else { w += 16; s += 3; }
    }
    return w;
}
static int16_t drawLabeled(const char* label, int16_t x, int16_t y, uint8_t color) {
    drawText(label, x, y, color);
    return x + textWidth(label);
}
static void drawAsciiStringScaledAt(const char* s, int16_t x, int16_t y, uint8_t color, uint8_t scale) {
    while (*s) { drawAsciiScaled(*s, x, y, color, scale); x += 8 * scale; s++; }
}
static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t yy = y; yy < y + h; yy++)
        for (int16_t xx = x; xx < x + w; xx++) drawPixel(xx, yy, color);
}
static void fillCircle(int16_t cx, int16_t cy, int16_t r, uint8_t color) {
    for (int16_t dy = -r; dy <= r; dy++)
        for (int16_t dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) drawPixel(cx + dx, cy + dy, color);
}
static void drawCircle(int16_t cx, int16_t cy, int16_t r, uint8_t color) {
    int16_t x = r, y = 0, err = 1 - r;
    while (x >= y) {
        drawPixel(cx + x, cy + y, color);
        drawPixel(cx + y, cy + x, color);
        drawPixel(cx - y, cy + x, color);
        drawPixel(cx - x, cy + y, color);
        drawPixel(cx - x, cy - y, color);
        drawPixel(cx - y, cy - x, color);
        drawPixel(cx + y, cy - x, color);
        drawPixel(cx + x, cy - y, color);
        y++;
        if (err < 0) { err += 2 * y + 1; }
        else { x--; err += 2 * (y - x) + 1; }
    }
}
// 竖向湿度计（温度计样式）：球泡常满，管内液柱高度=湿度%；正常黑、需浇水红
static void drawMoistureGauge(uint8_t moisture, uint8_t color) {
    const int16_t cx = 188;         // 温度计中心 X
    const int16_t yTop = 8;         // 竖管顶部
    const int16_t liqBottom = 76;   // 液柱底部（球泡内芯顶）
    const int16_t liqH = liqBottom - yTop;
    int16_t h = (int16_t)((int32_t)(moisture > 100 ? 100 : moisture) * liqH / 100);

    fillCircle(cx, 86, 10, color);                             // 球泡内芯液体
    if (h > 0) fillRect(cx - 4, liqBottom - h, 8, h, color);   // 管内液柱

    drawCircle(cx, 86, 12, C_BLACK);                           // 球泡外圈(2px)
    drawCircle(cx, 86, 11, C_BLACK);
    fillRect(cx - 6, yTop, 2, liqH, C_BLACK);                  // 左管壁
    fillRect(cx + 4, yTop, 2, liqH, C_BLACK);                  // 右管壁
    fillRect(cx - 6, yTop, 12, 2, C_BLACK);                    // 管顶封口
}

// ---------- 面板驱动 ----------
void begin() {
    SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);   // SCK=12/MOSI=11；MISO 默认 13(随后被 DC 覆盖)，SS 手动
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);      // 13（写-only 屏复用 MISO 脚为 DC）
    pinMode(EPD_RST_PIN, OUTPUT);
    pinMode(EPD_BUSY_PIN, INPUT_PULLUP);
    digitalWrite(EPD_CS_PIN, HIGH);

    reset();
    writeCmd(0x00); writeData(0x0F);                    // Panel Setting: OTP LUT, BWR
    writeCmd(0x44); writeData(0x00); writeData(EPD_BUF_STRIDE - 1);  // X 0..12（13 字节=104px）
    writeCmd(0x45); writeData(0x00); writeData(0x00);
    writeData((EPD_NATIVE_H - 1) & 0xFF); writeData(((EPD_NATIVE_H - 1) >> 8) & 0x01);  // Y 0..211
    writeCmd(0x4E); writeData(0x00);
    writeCmd(0x4F); writeData(0x00); writeData(0x00);
    writeCmd(0x50); writeData(0x11); writeData(0x07);  // VCOM 与数据间隔
    writeCmd(0x04);                                     // Power ON
    waitBusy();
}

static void sendToPanel() {
    writeCmd(0x4E); writeData(0x00);
    writeCmd(0x4F); writeData(0x00); writeData(0x00);

    writeCmd(0x10);  // 黑数据
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_CS_PIN, LOW);
    for (uint16_t i = 0; i < sizeof(g_black); i++) SPI.transfer(g_black[i]);
    digitalWrite(EPD_CS_PIN, HIGH);

    writeCmd(0x13);  // 红数据（1=红，直接发送）
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_CS_PIN, LOW);
    for (uint16_t i = 0; i < sizeof(g_red); i++) SPI.transfer(g_red[i]);
    digitalWrite(EPD_CS_PIN, HIGH);

    writeCmd(0x12);  // 全局刷新
    delay(10);
    waitBusy();
}

void render(const sensor::SoilReading &r, float vbat, bool alarm, const char* timeStr) {
    clearBuffer();

    char buf[32];
    int16_t x;

    // 左侧：时间 / 湿度 / 电压 / 提示
    x = drawLabeled("时间:", 4, 6, C_BLACK);
    snprintf(buf, sizeof(buf), "%s", timeStr);
    drawText(buf, x, 6, C_BLACK);

    x = drawLabeled("湿度:", 4, 34, C_BLACK);                            // 标签垂直居中于 2x 数值
    snprintf(buf, sizeof(buf), "%d%%", (int)(r.moisture + 0.5f));
    drawAsciiStringScaledAt(buf, x, 26, C_BLACK, 2);                    // 湿度值 2x 放大

    x = drawLabeled("电压:", 4, 64, C_BLACK);
    snprintf(buf, sizeof(buf), "%.2fV", vbat);
    drawText(buf, x, 64, C_BLACK);

    x = drawLabeled("提示:", 4, 86, C_BLACK);
    drawText(alarm ? "需要浇水" : "正常", x, 86, alarm ? C_RED : C_BLACK);  // 状态值随告警变色

    // 右侧：竖向湿度计（液柱高度=湿度，正常黑/需浇水红）
    drawMoistureGauge((uint8_t)(r.moisture + 0.5f), alarm ? C_RED : C_BLACK);

    sendToPanel();
}

void hibernate() {
    writeCmd(0x50); writeData(0xF7);   // VCOM
    writeCmd(0x02);                    // Power OFF
    waitBusy();
    writeCmd(0x07); writeData(0xA5);   // Deep Sleep
}

} // namespace display
