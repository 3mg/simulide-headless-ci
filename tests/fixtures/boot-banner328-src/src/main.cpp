#include <Arduino.h>

void setup() {
    Serial.begin(9600);
    delay(100);
    Serial.println("boot:start");
    delay(200);
    Serial.println("boot:init-ok");
    delay(200);
    Serial.println("boot:ready");
}

void loop() {}
