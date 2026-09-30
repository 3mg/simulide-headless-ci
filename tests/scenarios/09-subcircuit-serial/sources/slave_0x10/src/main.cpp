/**
 * ATtiny85 (Digispark Round) — encoder on P1+P3 + I2C slave + bit-bang TX on P4
 *
 * Pinout:
 *   P0 (PB0) — SDA
 *   P1 (PB1) — encoder CLK (A)
 *   P2 (PB2) — SCL
 *   P3 (PB3) — encoder DT (B)
 *   P4 (PB4) — TX (bit-bang UART 9600) → Mega Serial1 RX (pin 19)
 *   5V       — from Mega 5V pin (NOT USB, NOT VIN)
 */

#include <Arduino.h>
#include <USIWire.h>
#include <util/delay.h>

#ifndef MY_ADDRESS
  #define MY_ADDRESS 0x10
#endif

#define TX_BIT  (1 << 4)

// ── Bit-bang TX ───────────────────────────────────────────────────────────────

static void txByte(uint8_t b) {
  PORTB &= ~TX_BIT;
  _delay_us(104);
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 1) PORTB |= TX_BIT;
    else       PORTB &= ~TX_BIT;
    b >>= 1;
    _delay_us(104);
  }
  PORTB |= TX_BIT;
  _delay_us(104);
}

static void txStr(const char *s) { while (*s) txByte(*s++); }

static void txInt(int16_t v) {
  char buf[8];
  itoa(v, buf, 10);
  txStr(buf);
}

// ── State ─────────────────────────────────────────────────────────────────────

volatile int16_t encValue = 0;

void receiveEvent(int n) {
  if (n >= 2) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    while (Wire.available()) Wire.read();
    encValue = (int16_t)((hi << 8) | lo);
  }
}

void requestEvent() {
  Wire.write((uint8_t)(encValue >> 8));
  Wire.write((uint8_t)(encValue & 0xFF));
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  DDRB  |=  TX_BIT;
  PORTB |=  TX_BIT;

  // P1, P3 as input with pullup
  DDRB  &= ~((1 << 1) | (1 << 3));
  PORTB |=   (1 << 1) | (1 << 3);

  Wire.begin(MY_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  _delay_ms(100);
  txStr("start\r\n");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  static uint8_t  lastCLK = 1;
  static uint16_t ticks   = 0;
  static uint32_t counter = 0;

  // Encoder: CLK=P1, DT=P3
  uint8_t pinb = PINB;
  uint8_t clk  = (pinb >> 1) & 1;
  uint8_t dt   = (pinb >> 3) & 1;

  if (clk != lastCLK) {
    if (clk == 0) {
      if (dt == 0) encValue++;
      else         encValue--;
    }
    lastCLK = clk;
  }

  _delay_us(100);

  if (++ticks >= 10000) {
    ticks = 0;
    counter++;
    txStr("t=");   txInt(counter);
    txStr(" enc="); txInt(encValue);
    txStr("\r\n");
  }
}
