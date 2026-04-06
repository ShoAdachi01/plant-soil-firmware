#ifndef PLANT_COMPANION_CONFIG_H
#define PLANT_COMPANION_CONFIG_H

// =============================================================================
// Pin Definitions
// =============================================================================

// DFRobot SEN0193 capacitive soil moisture sensor — analog output
#define SENSOR_PIN            A0

// =============================================================================
// Sensor Calibration
//
// The SEN0193 has an INVERSE relationship: higher value = drier soil.
// These defaults come from DFRobot documentation.
// To recalibrate: use serial command 'c' to enter calibration mode,
// note the value in air (AirValue) and in water (WaterValue).
// =============================================================================

#define DEFAULT_AIR_VALUE     520   // Sensor reading in dry air (0% moisture)
#define DEFAULT_WATER_VALUE   260   // Sensor reading in water  (100% moisture)

// =============================================================================
// Sampling Configuration
//
// We take multiple samples and use a trimmed mean (discard min/max)
// to filter ADC noise on the Arduino Uno's 10-bit ADC.
// =============================================================================

#define NUM_SAMPLES           10    // Total analog reads per measurement cycle
#define SAMPLE_DELAY_MS       50    // Delay between individual reads (ms)
                                    // Total sampling window = NUM_SAMPLES * SAMPLE_DELAY_MS = 500ms

// =============================================================================
// Reporting Intervals
// =============================================================================

#define NORMAL_INTERVAL_MS    30000 // 30 seconds — production mode
#define DEMO_INTERVAL_MS      5000  // 5 seconds  — demo/Kickstarter mode

// =============================================================================
// Serial Communication
// =============================================================================

#define SERIAL_BAUD           9600

// =============================================================================
// Error Detection
// =============================================================================

// If raw ADC value exceeds this, the sensor is likely disconnected
// (floating pin reads near 1023)
#define SENSOR_DISCONNECT_THRESHOLD  1000

// =============================================================================
// EEPROM Addresses
//
// We store persistent config in EEPROM so it survives reboot.
// =============================================================================

#define EEPROM_MODE_ADDR      0     // 1 byte: 'd' = demo, 'n' = normal
#define EEPROM_AIR_ADDR       1     // 2 bytes: calibrated air value (high byte, low byte)
#define EEPROM_WATER_ADDR     3     // 2 bytes: calibrated water value (high byte, low byte)
#define EEPROM_MAGIC_ADDR     5     // 1 byte: 0xAB = calibration has been written

#define EEPROM_MAGIC_VALUE    0xAB  // Marker that EEPROM has valid calibration data

#endif // PLANT_COMPANION_CONFIG_H
