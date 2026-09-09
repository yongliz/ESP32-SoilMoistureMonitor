#include "display.h"
#include "config.h"
#include "font.h"
#include <Arduino.h>
#include <SPI.h>

namespace display {

// 颜色枚举
enum : uint8_t { C_WHITE, C_RED, C_BLACK };

// 帧缓冲：black 1=白 0=黑；red 1=红 0=无（发送时对 red 取反）
static uint8_t g_black[EPD_BUF_WIDTH / 8 * EPD_HEIGHT];
static uint8_t g_red[EPD_BUF_WIDTH / 8 * EPD_HEIGHT];

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
static void drawPixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;
    uint16_t idx = (uint16_t)y * (EPD_BUF_WIDTH / 8) + (x >> 3);
    uint8_t mask = 0x80 >> (x & 7);
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
static void drawTextCentered(const char* s, int16_t y, uint8_t color) {
    drawText(s, (EPD_WIDTH - textWidth(s)) / 2, y, color);
}
static void drawAsciiStringScaled(const char* s, int16_t y, uint8_t color, uint8_t scale) {
    int w = (int)strlen(s) * 8 * scale;
    int16_t x = (EPD_WIDTH - w) / 2;
    while (*s) { drawAsciiScaled(*s, x, y, color, scale); x += 8 * scale; s++; }
}
static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t yy = y; yy < y + h; yy++)
        for (int16_t xx = x; xx < x + w; xx++) drawPixel(xx, yy, color);
}

// ---------- 面板驱动 ----------
void begin() {
    pinMode(EPD_CS_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    pinMode(EPD_RST_PIN, OUTPUT);
    pinMode(EPD_BUSY_PIN, INPUT_PULLUP);
    digitalWrite(EPD_CS_PIN, HIGH);

    SPI.begin();
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    reset();
    writeCmd(0x00); writeData(0x0F);              // Panel Setting: OTP LUT, BWR, 上扫
    writeCmd(0x44); writeData(0x00); writeData((EPD_BUF_WIDTH >> 3) - 1);  // X 0..15
    writeCmd(0x45); writeData(0x00); writeData(0x00);
    writeData((EPD_HEIGHT - 1) & 0xFF); writeData(((EPD_HEIGHT - 1) >> 8) & 0x01);  // Y 0..249
    writeCmd(0x4E); writeData(0x00);              // X 指针
    writeCmd(0x4F); writeData(0x00); writeData(0x00);  // Y 指针
    writeCmd(0x50); writeData(0x11); writeData(0x07);  // VCOM 与数据间隔
    writeCmd(0x04);                               // Power ON
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

    writeCmd(0x13);  // 红数据（取反）
    digitalWrite(EPD_DC_PIN, HIGH);
    digitalWrite(EPD_CS_PIN, LOW);
    for (uint16_t i = 0; i < sizeof(g_red); i++) SPI.transfer((uint8_t)~g_red[i]);
    digitalWrite(EPD_CS_PIN, HIGH);

    writeCmd(0x12);  // 全局刷新
    delay(10);
    waitBusy();
}

void render(const sensor::SoilReading &r, float vbat, bool alarm, const char* timeStr) {
    clearBuffer();

    drawTextCentered(DISPLAY_TITLE, 8, C_BLACK);   // 标题

    char buf[24];
    snprintf(buf, sizeof(buf), "%d%%", (int)(r.moisture + 0.5f));
    drawAsciiStringScaled(buf, 40, C_BLACK, 2);     // 湿度（2x 大字）

    drawTextCentered(alarm ? "需要浇水" : "正常", 92, alarm ? C_RED : C_BLACK);

    fillRect(8, 120, EPD_WIDTH - 16, 1, C_BLACK);   // 分隔线

    drawText(timeStr, 8, 132, C_BLACK);             // 时间

    snprintf(buf, sizeof(buf), "%.2fV", vbat);
    drawText(buf, 8, 154, C_BLACK);                 // 电压

    sendToPanel();
}

void hibernate() {
    writeCmd(0x50); writeData(0xF7);   // VCOM
    writeCmd(0x02);                    // Power OFF
    waitBusy();
    writeCmd(0x07); writeData(0xA5);   // Deep Sleep
}

} // namespace display
