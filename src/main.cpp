#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("boot");
}

void loop() {
    // 深度睡眠模式下不会执行到这里
}
