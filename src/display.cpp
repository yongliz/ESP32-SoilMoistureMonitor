#include "display.h"
#include "config.h"
#include <GxEPD2_3C.h>
#include <Fonts/FreeSans9pt7b.h>

namespace display {

GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> epd(
    GxEPD2_213_Z98c(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

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
