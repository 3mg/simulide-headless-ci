#include <Arduino.h>

void setup() {
    Serial.begin(9600);
    delay(100);
    Serial.print("hello simulide\r\n");
}

void loop() {
    // one-shot startup banner only
}
