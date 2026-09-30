/**
 * Hardware Test 3 — Mega Master: 2x ATtiny85 Encoder Slaves
 *
 * - Reads encoder values from two slaves (0x10, 0x11)
 * - Stores values in EEPROM (persists across power cycles)
 * - On startup: sends stored values to slaves
 * - Polls slaves every 50ms, updates EEPROM on change
 * - Prints status to Serial every second
 * - Displays status on SSD1306 OLED (address 0x3C)
 *
 * OLED layout:
 *   E1 - 64
 *   E2 - off
 *
 * I2C Protocol:
 *   Master → Slave: write 2 bytes = current value (int16, big-endian)
 *   Master ← Slave: read  2 bytes = current value (int16, big-endian)
 *
 * Mega I2C pins: SDA=20, SCL=21
 * OLED address:  0x3C
 *
 * ATtiny85 USI I2C known issues:
 *   - USI can't reliably do 100kHz slave; use 50kHz or lower
 *   - requestFrom() can hang if slave not ready → use Wire.setWireTimeout()
 *   - Bus can get stuck (SDA/SCL held low) → bus recovery via 9 SCL pulses
 *
 * Slave debug serial (9600 baud, TX only):
 *   Slave 0x10 P3 → Mega pin 19 (Serial1 RX)
 *   Slave 0x11 P3 → Mega pin 17 (Serial2 RX)
 *   Mega forwards both streams to Serial (USB) with prefix [E1] / [E2]
 */

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Display ───────────────────────────────────────────────────────────────────

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Slaves ────────────────────────────────────────────────────────────────────

#define SLAVE_COUNT  2
const uint8_t SLAVE_ADDRS[SLAVE_COUNT] = { 0x10, 0x11 };

// EEPROM layout: 2 bytes per slave (int16, big-endian)
#define EEPROM_BASE 0

// ── EEPROM helpers ────────────────────────────────────────────────────────────

int16_t eepromRead(uint8_t idx) {
  uint8_t hi = EEPROM.read(EEPROM_BASE + idx * 2);
  uint8_t lo = EEPROM.read(EEPROM_BASE + idx * 2 + 1);
  return (int16_t)((hi << 8) | lo);
}

void eepromWrite(uint8_t idx, int16_t val) {
  EEPROM.update(EEPROM_BASE + idx * 2,     (uint8_t)(val >> 8));
  EEPROM.update(EEPROM_BASE + idx * 2 + 1, (uint8_t)(val & 0xFF));
}

// ── I2C bus recovery ──────────────────────────────────────────────────────────
// If bus is stuck (SDA/SCL held low by slave), send 9 SCL pulses to free it.
// Per NXP I2C specification AN10216.

#define PIN_SDA 20
#define PIN_SCL 21

void i2cBusRecovery() {
  Serial.println(F("I2C bus recovery..."));

  pinMode(SCL, OUTPUT);
  pinMode(SDA, OUTPUT);
  digitalWrite(SCL, HIGH);
  digitalWrite(SDA, HIGH);
  delayMicroseconds(5);

  for (uint8_t i = 0; i < 9; i++) {
    digitalWrite(SCL, LOW);  delayMicroseconds(5);
    digitalWrite(SCL, HIGH); delayMicroseconds(5);
    if (digitalRead(SDA) == HIGH) break;
  }

  // STOP condition
  digitalWrite(SDA, LOW);  delayMicroseconds(5);
  digitalWrite(SCL, HIGH); delayMicroseconds(5);
  digitalWrite(SDA, HIGH); delayMicroseconds(5);

  pinMode(SCL, INPUT);
  pinMode(SDA, INPUT);

  Wire.begin();
  Wire.setClock(50000);
  Wire.setWireTimeout(25000, true);  // 25ms, auto-reset on timeout
}

// ── I2C helpers ───────────────────────────────────────────────────────────────

void sendValue(uint8_t addr, int16_t val) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val & 0xFF));
  if (Wire.endTransmission() != 0) {
    if (Wire.getWireTimeoutFlag()) {
      Wire.clearWireTimeoutFlag();
      i2cBusRecovery();
    }
  }
}

bool readValue(uint8_t addr, int16_t &val) {
  Wire.clearWireTimeoutFlag();

  uint8_t n = Wire.requestFrom(addr, (uint8_t)2);

  if (Wire.getWireTimeoutFlag()) {
    //Serial.print(F("WireTimeout, n=")); Serial.print(n);
    //Serial.println();
    
    Wire.clearWireTimeoutFlag();
    i2cBusRecovery();
    return false;
  }

  if (n < 2) {
    while (Wire.available()) Wire.read();
    return false;
  }
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  val = (int16_t)((hi << 8) | lo);
  return true;
}

// ── State ─────────────────────────────────────────────────────────────────────

int16_t values[SLAVE_COUNT];
bool    online[SLAVE_COUNT];
bool    initialized[SLAVE_COUNT];  // true once slave echoed back our value
uint32_t tickCount = 0;

// ── Display ───────────────────────────────────────────────────────────────────

bool displayOk = false;

bool initDisplay() {
  displayOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  return displayOk;
}

void updateDisplay() {
  if (!displayOk) {
    // Try to reconnect
    if (!initDisplay()) return;
    Serial.println(F("OLED reconnected"));
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
    display.setTextSize(2);
    display.setCursor(0, i * 20);
    display.print(F("E"));
    display.print(i + 1);
    display.print(F(" - "));
    if (online[i]) {
      display.print(values[i]);
    } else {
      display.print(F("off"));
    }
  }

  // Tick counter at bottom — proves display is alive
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("tick: "));
  display.print(tickCount);

  display.display();
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("=== HW Test 3: Mega Master ==="));

  Serial1.begin(9600);  // RX from Slave 0x10 (pin 19)
  Serial2.begin(9600);  // RX from Slave 0x11 (pin 17)

  // 50kHz — ATtiny85 USI slave can't reliably do 100kHz
  Wire.begin();
  Wire.setClock(50000);
  Wire.setWireTimeout(25000, true);  // 25ms timeout, auto-reset bus on hang
  delay(100);

  // SIMULATION: skip OLED init and bus scan — they take too long in SimulIDE
  for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
    values[i] = 0;
    online[i] = false;
    initialized[i] = false;
  }

  Serial.println(F("Ready. Polling slaves..."));
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  // Forward slave debug serial to USB Serial with [E1]/[E2] prefix per line
  static char buf1[64]; static uint8_t len1 = 0;
  static char buf2[64]; static uint8_t len2 = 0;

  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\r') continue;
    if (c == '\n' || len1 >= sizeof(buf1) - 1) {
      buf1[len1] = '\0';
      if (len1 > 0) { Serial.print(F("[E1] ")); Serial.println(buf1); }
      len1 = 0;
    } else {
      buf1[len1++] = c;
    }
  }
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\r') continue;
    if (c == '\n' || len2 >= sizeof(buf2) - 1) {
      buf2[len2] = '\0';
      if (len2 > 0) { Serial.print(F("[E2] ")); Serial.println(buf2); }
      len2 = 0;
    } else {
      buf2[len2++] = c;
    }
  }

  // Poll slaves every 50ms, save to EEPROM on change
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll >= 50) {
    lastPoll = millis();

    for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
      int16_t newVal;
      bool ok = readValue(SLAVE_ADDRS[i], newVal);
      online[i] = ok;

      if (ok) {
        if (!initialized[i]) {
          // Slave just came online — send stored value and wait for echo
          sendValue(SLAVE_ADDRS[i], values[i]);
          if (newVal == values[i]) {
            initialized[i] = true;
            Serial.print(F("Slave 0x")); Serial.print(SLAVE_ADDRS[i], HEX);
            Serial.println(F(" initialized"));
          }
        } else if (newVal != values[i]) {
          values[i] = newVal;
          eepromWrite(i, newVal);
        }
      }
    }
  }

  // Update display and print to Serial every second
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
      Serial.print(F("E")); Serial.print(i + 1);
      Serial.print(F(": "));
      if (online[i]) {
        Serial.print(values[i]);
      } else {
        Serial.print(F("off"));
      }
      Serial.print(F("  "));
    }
    Serial.println();

    tickCount++;
    updateDisplay();
  }
}
