/**
 * Wokwi Parity Test — Mega Master (I2C)
 */

#include <Arduino.h>
#include <Wire.h>

#define SLAVE_ADDR 0x08

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("initialized"));

  Wire.begin();
  Wire.setClock(50000);  // 50kHz like in hardware test 3

  delay(500);
}

void loop() {
  Serial.println(F("requesting..."));
  uint8_t n = Wire.requestFrom(SLAVE_ADDR, (uint8_t)4);
  Serial.print(F("got n="));
  Serial.println(n);

  if (n == 4) {
    uint8_t hi  = Wire.read();
    uint8_t lo  = Wire.read();
    uint8_t typ = Wire.read();
    uint8_t pos = Wire.read();

    uint16_t value = ((uint16_t)hi << 8) | lo;

    Serial.print(F("value="));
    Serial.print(value);
    Serial.print(F(" type="));
    Serial.print(typ);
    Serial.print(F(" pos="));
    Serial.println(pos);

    if (value == 512 && typ == 1 && pos == 0) {
      Serial.println(F("TEST PASSED"));
    } else {
      Serial.println(F("TEST FAILED: unexpected values"));
    }
  }

  delay(3000);
}
