/*
 * Plant Companion V1 Firmware
 *
 * Reads soil moisture from DFRobot SEN0193 capacitive sensor,
 * applies trimmed-mean filtering, and outputs JSON over USB serial
 * for the bridge script to forward to the cloud backend.
 *
 * Hardware:
 *   MCU:    Arduino Uno R3
 *   Sensor: DFRobot SEN0193 (AOUT → A0, VCC → 5V, GND → GND)
 *
 * Serial commands:
 *   'd' — switch to demo mode (5s interval)
 *   'n' — switch to normal mode (30s interval)
 *   'c' — calibration mode (raw values at 100ms for 30s)
 *   's' — print current status
 *
 * JSON output format:
 *   {"m":452,"p":63.08,"ts":30500}
 *   m  = raw ADC value (0-1023)
 *   p  = moisture percentage (0.0-100.0)
 *   ts = milliseconds since boot
 *
 * Error output:
 *   {"err":"no_sensor","ts":30500}
 */

#include <EEPROM.h>
#include "config.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

int airValue;              // Calibrated dry reading (high = dry)
int waterValue;            // Calibrated wet reading (low = wet)
unsigned long intervalMs;  // Current reporting interval
bool demoMode;             // true = 5s, false = 30s
unsigned long lastReportTime = 0;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);

  // Wait for serial port on boards that need it (Uno doesn't, but harmless)
  while (!Serial) {
    ;
  }

  loadConfig();

  // Announce boot
  Serial.print(F("{\"event\":\"boot\",\"mode\":\""));
  Serial.print(demoMode ? F("demo") : F("normal"));
  Serial.print(F("\",\"interval\":"));
  Serial.print(intervalMs);
  Serial.print(F(",\"air\":"));
  Serial.print(airValue);
  Serial.print(F(",\"water\":"));
  Serial.print(waterValue);
  Serial.println(F("}"));
}

// ---------------------------------------------------------------------------
// Main Loop
// ---------------------------------------------------------------------------

void loop() {
  // Check for serial commands (non-blocking)
  handleSerialCommands();

  unsigned long now = millis();

  // Handle millis() overflow (rolls over every ~49.7 days)
  if (now - lastReportTime >= intervalMs) {
    lastReportTime = now;
    takeMeasurementAndReport();
  }
}

// ---------------------------------------------------------------------------
// Measurement & Reporting
// ---------------------------------------------------------------------------

void takeMeasurementAndReport() {
  int samples[NUM_SAMPLES];

  // Collect samples
  for (int i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = analogRead(SENSOR_PIN);
    delay(SAMPLE_DELAY_MS);
  }

  // Sort for trimmed mean
  sortArray(samples, NUM_SAMPLES);

  // Check for sensor disconnect (all readings near max)
  if (samples[NUM_SAMPLES / 2] > SENSOR_DISCONNECT_THRESHOLD) {
    printError(F("no_sensor"));
    return;
  }

  // Trimmed mean: discard lowest and highest, average the rest
  long sum = 0;
  int trimCount = NUM_SAMPLES - 2; // 8 out of 10
  for (int i = 1; i < NUM_SAMPLES - 1; i++) {
    sum += samples[i];
  }
  int rawAvg = (int)(sum / trimCount);

  // Convert to moisture percentage
  // SEN0193 is INVERSE: higher raw = drier
  float moisturePct = calculateMoisturePercent(rawAvg);

  // Output JSON
  printReading(rawAvg, moisturePct);
}

float calculateMoisturePercent(int raw) {
  if (airValue == waterValue) {
    // Prevent division by zero if miscalibrated
    return 0.0;
  }

  float pct = (float)(airValue - raw) / (float)(airValue - waterValue) * 100.0;

  // Clamp to 0-100
  if (pct < 0.0) pct = 0.0;
  if (pct > 100.0) pct = 100.0;

  return pct;
}

// ---------------------------------------------------------------------------
// Serial Output (manual JSON to save RAM — no ArduinoJson needed)
// ---------------------------------------------------------------------------

void printReading(int raw, float moisturePct) {
  // {"m":452,"p":63.08,"ts":30500}
  Serial.print(F("{\"m\":"));
  Serial.print(raw);
  Serial.print(F(",\"p\":"));
  Serial.print(moisturePct, 2);
  Serial.print(F(",\"ts\":"));
  Serial.print(millis());
  Serial.println(F("}"));
}

void printError(const __FlashStringHelper* errMsg) {
  // {"err":"no_sensor","ts":30500}
  Serial.print(F("{\"err\":\""));
  Serial.print(errMsg);
  Serial.print(F("\",\"ts\":"));
  Serial.print(millis());
  Serial.println(F("}"));
}

// ---------------------------------------------------------------------------
// Serial Commands
// ---------------------------------------------------------------------------

void handleSerialCommands() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'd':
      setMode(true);
      Serial.println(F("{\"event\":\"mode\",\"mode\":\"demo\",\"interval\":5000}"));
      break;

    case 'n':
      setMode(false);
      Serial.println(F("{\"event\":\"mode\",\"mode\":\"normal\",\"interval\":30000}"));
      break;

    case 'c':
      runCalibration();
      break;

    case 's':
      printStatus();
      break;

    default:
      // Ignore unknown commands (including newlines, carriage returns)
      break;
  }
}

void setMode(bool demo) {
  demoMode = demo;
  intervalMs = demo ? DEMO_INTERVAL_MS : NORMAL_INTERVAL_MS;

  // Persist to EEPROM
  EEPROM.update(EEPROM_MODE_ADDR, demo ? 'd' : 'n');

  // Reset timer so next reading happens after new interval
  lastReportTime = millis();
}

void printStatus() {
  Serial.print(F("{\"event\":\"status\",\"mode\":\""));
  Serial.print(demoMode ? F("demo") : F("normal"));
  Serial.print(F("\",\"interval\":"));
  Serial.print(intervalMs);
  Serial.print(F(",\"air\":"));
  Serial.print(airValue);
  Serial.print(F(",\"water\":"));
  Serial.print(waterValue);
  Serial.print(F(",\"uptime\":"));
  Serial.print(millis());
  Serial.print(F(",\"free_ram\":"));
  Serial.print(freeRam());
  Serial.println(F("}"));
}

// ---------------------------------------------------------------------------
// Calibration Mode
//
// Prints raw sensor values at 100ms intervals for 30 seconds.
// User records the air value and water value, then sends them
// via the bridge or re-flashes with updated defaults.
//
// After calibration mode ends, it saves the min and max observed
// values as waterValue and airValue respectively.
// ---------------------------------------------------------------------------

void runCalibration() {
  Serial.println(F("{\"event\":\"cal_start\",\"duration_s\":30}"));
  Serial.println(F("# Hold sensor in AIR for first 15s, then in WATER for next 15s"));

  int minVal = 1023;
  int maxVal = 0;
  unsigned long calStart = millis();
  unsigned long calDuration = 30000UL; // 30 seconds

  while (millis() - calStart < calDuration) {
    int raw = analogRead(SENSOR_PIN);

    if (raw < minVal) minVal = raw;
    if (raw > maxVal) maxVal = raw;

    Serial.print(F("{\"cal\":"));
    Serial.print(raw);
    Serial.print(F(",\"ts\":"));
    Serial.print(millis());
    Serial.println(F("}"));

    delay(100);

    // Allow abort via 'x' command during calibration
    if (Serial.available() && Serial.read() == 'x') {
      Serial.println(F("{\"event\":\"cal_abort\"}"));
      return;
    }
  }

  // Save calibration: max = air (dry), min = water (wet)
  airValue = maxVal;
  waterValue = minVal;

  // Persist to EEPROM
  EEPROM.update(EEPROM_AIR_ADDR, highByte(airValue));
  EEPROM.update(EEPROM_AIR_ADDR + 1, lowByte(airValue));
  EEPROM.update(EEPROM_WATER_ADDR, highByte(waterValue));
  EEPROM.update(EEPROM_WATER_ADDR + 1, lowByte(waterValue));
  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);

  Serial.print(F("{\"event\":\"cal_done\",\"air\":"));
  Serial.print(airValue);
  Serial.print(F(",\"water\":"));
  Serial.print(waterValue);
  Serial.println(F("}"));
}

// ---------------------------------------------------------------------------
// Config (EEPROM)
// ---------------------------------------------------------------------------

void loadConfig() {
  // Load mode
  byte modeVal = EEPROM.read(EEPROM_MODE_ADDR);
  if (modeVal == 'd') {
    demoMode = true;
    intervalMs = DEMO_INTERVAL_MS;
  } else {
    demoMode = false;
    intervalMs = NORMAL_INTERVAL_MS;
  }

  // Load calibration if previously saved
  byte magic = EEPROM.read(EEPROM_MAGIC_ADDR);
  if (magic == EEPROM_MAGIC_VALUE) {
    airValue = (EEPROM.read(EEPROM_AIR_ADDR) << 8) | EEPROM.read(EEPROM_AIR_ADDR + 1);
    waterValue = (EEPROM.read(EEPROM_WATER_ADDR) << 8) | EEPROM.read(EEPROM_WATER_ADDR + 1);

    // Sanity check: if values look corrupted, reset to defaults
    if (airValue < 100 || airValue > 1023 || waterValue < 0 || waterValue > 900
        || waterValue >= airValue) {
      airValue = DEFAULT_AIR_VALUE;
      waterValue = DEFAULT_WATER_VALUE;
    }
  } else {
    airValue = DEFAULT_AIR_VALUE;
    waterValue = DEFAULT_WATER_VALUE;
  }
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

// Simple insertion sort — fine for 10 elements, no library needed
void sortArray(int arr[], int len) {
  for (int i = 1; i < len; i++) {
    int key = arr[i];
    int j = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

// Report free RAM — useful for debugging memory issues on Uno's 2KB
int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
