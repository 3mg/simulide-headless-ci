#include <Arduino.h>

static uint32_t counter = 0;

void setup() {
    Serial.begin(9600);
    delay(100);
    Serial.println("tick-start");
}

void loop() {
    delay(1000);
    counter++;
    Serial.print("tick:");
    Serial.println(counter);
}
