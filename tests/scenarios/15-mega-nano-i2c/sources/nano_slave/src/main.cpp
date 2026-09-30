/**
 * Wokwi Parity Test — Nano Slave (I2C)
 *
 * Responds to master requests with 4 bytes: value_hi, value_lo, type, pos.
 * Fixed values: value=512, type=1 (encoder), pos=0.
 *
 * I2C address: 0x08
 */

#include <Arduino.h>
#include <Wire.h>

#define MY_ADDRESS 0x08

void onRequest() {
  Wire.write(0x02);  // value high byte (512 = 0x0200)
  Wire.write(0x00);  // value low byte
  Wire.write(0x01);  // type: encoder
  Wire.write(0x00);  // position
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("slave init"));

  Wire.begin(MY_ADDRESS);
  Wire.onRequest(onRequest);

  Serial.print(F("slave ready at 0x"));
  Serial.println(MY_ADDRESS, HEX);
}

void loop() {
  delay(5000);
  Serial.println(F("slave alive"));
}
