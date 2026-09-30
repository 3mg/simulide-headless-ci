#include <Arduino.h>

// Simulates a sensor node: boots, reports node ID, then sends periodic readings.
// Used in multi-MCU boot scenario to verify independent MCU serial capture.

#define NODE_ID 2

void setup() {
    Serial.begin(9600);
    delay(150);
    Serial.print("node:");
    Serial.print(NODE_ID);
    Serial.println(":boot");
    delay(100);
    Serial.print("node:");
    Serial.print(NODE_ID);
    Serial.println(":ready");
}

void loop() {
    delay(2000);
    Serial.print("node:");
    Serial.print(NODE_ID);
    Serial.println(":alive");
}
