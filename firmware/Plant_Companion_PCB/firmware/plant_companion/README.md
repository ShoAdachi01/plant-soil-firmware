# Plant Companion — Firmware

Arduino Uno R3 firmware for reading the DFRobot SEN0193 capacitive soil moisture sensor and outputting JSON over USB serial.

## Wiring

```
DFRobot SEN0193          Arduino Uno R3
─────────────────        ──────────────
Red   (VCC)         →    5V
Black (GND)         →    GND
Yellow (AOUT)       →    A0
```

**Important:** The SEN0193 probe is only waterproof below the red line. Do NOT submerge the circuit board area above the red line.

## Quick Start

```bash
# 1. Install Arduino CLI (macOS)
brew install arduino-cli

# 2. One-time setup
arduino-cli core update-index
arduino-cli core install arduino:avr

# 3. Plug in Arduino Uno via USB

# 4. Find your port
arduino-cli board list
# Note the port (e.g., /dev/ttyACM0 or /dev/cu.usbmodem14101)

# 5. Compile and upload (replace port)
arduino-cli compile --upload -p /dev/ttyACM0 --fqbn arduino:avr:uno firmware/plant_companion

# 6. Monitor output
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600
```

See `docs/vibe/arduino-cli-reference.md` for detailed CLI reference.

## Serial Output

### Sensor readings (normal operation)

```json
{"m":452,"p":63.08,"ts":30500}
```

| Field | Type | Description |
|-------|------|-------------|
| `m` | int | Raw ADC value (0–1023). Higher = drier. |
| `p` | float | Moisture percentage (0.0–100.0). Higher = wetter. |
| `ts` | long | Milliseconds since boot (rolls over at ~49.7 days) |

### Events

```json
{"event":"boot","mode":"normal","interval":30000,"air":520,"water":260}
{"event":"mode","mode":"demo","interval":5000}
{"event":"status","mode":"normal","interval":30000,"air":520,"water":260,"uptime":84000,"free_ram":1200}
{"event":"cal_start","duration_s":30}
{"cal":487,"ts":5100}
{"event":"cal_done","air":520,"water":258}
{"event":"cal_abort"}
```

### Errors

```json
{"err":"no_sensor","ts":30500}
```

Sent when raw ADC value exceeds 1000 (sensor likely disconnected or floating pin).

## Serial Commands

Send these characters over serial to control the firmware:

| Command | Action |
|---------|--------|
| `d` | Switch to **demo mode** (5s interval). Persists across reboot. |
| `n` | Switch to **normal mode** (30s interval). Persists across reboot. |
| `c` | Start **calibration mode** (see below). |
| `s` | Print **current status** (mode, calibration values, uptime, free RAM). |
| `x` | **Abort calibration** (only works during calibration mode). |

## Calibration

Default calibration values (from DFRobot docs):
- **Air value:** 520 (sensor in dry air)
- **Water value:** 260 (sensor submerged to red line)

These work for most cases, but for best accuracy, recalibrate per sensor unit:

### Calibration procedure

1. Open serial monitor: `arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600`
2. Send `c` to start calibration (30 seconds total)
3. **First 15 seconds:** hold sensor in air — note the readings (~520)
4. **Next 15 seconds:** insert sensor into water up to the red line — note the readings (~260)
5. Calibration auto-saves the min (water) and max (air) values to EEPROM
6. Send `x` at any time to abort without saving

Calibrated values persist across reboots. To reset to defaults, re-flash the firmware.

## How It Works

### Measurement cycle

1. Take 10 analog reads from A0, 50ms apart (500ms window)
2. Sort all 10 values
3. Discard the lowest and highest (trimmed mean)
4. Average the remaining 8 values → `raw`
5. Convert: `moisture_pct = (airValue - raw) / (airValue - waterValue) * 100`
6. Clamp to 0–100%
7. Output JSON

### Why trimmed mean?

The Arduino Uno's 10-bit ADC has inherent noise (~5mV jitter). Single reads fluctuate. A simple average helps, but one outlier spike can skew it. Trimmed mean discards the extremes, giving a more stable reading with minimal RAM cost (10-element int array = 20 bytes).

## Memory Usage

Target: stay under 75% of both flash (32KB) and RAM (2KB).

| Resource | Budget | Typical Usage |
|----------|--------|---------------|
| Flash | 32,256 bytes | ~4,000 bytes (~12%) |
| RAM | 2,048 bytes | ~400 bytes (~20%) |

Use the `s` command to check `free_ram` at runtime. If free RAM drops below 200 bytes, investigate.

## Configuration

All constants are in `config.h`:

| Constant | Default | Description |
|----------|---------|-------------|
| `SENSOR_PIN` | A0 | Analog input pin |
| `DEFAULT_AIR_VALUE` | 520 | Dry calibration default |
| `DEFAULT_WATER_VALUE` | 260 | Wet calibration default |
| `NUM_SAMPLES` | 10 | Reads per measurement |
| `SAMPLE_DELAY_MS` | 50 | Delay between reads |
| `NORMAL_INTERVAL_MS` | 30000 | Reporting interval (normal) |
| `DEMO_INTERVAL_MS` | 5000 | Reporting interval (demo) |
| `SERIAL_BAUD` | 9600 | Serial baud rate |
| `SENSOR_DISCONNECT_THRESHOLD` | 1000 | ADC value indicating no sensor |

## File Structure

```
firmware/plant_companion/
  plant_companion.ino   — main sketch (all logic)
  config.h              — constants, pins, calibration defaults
  README.md             — this file
```

## Troubleshooting

### Readings are all 0% or 100%
- Calibration values may be wrong. Send `s` to check current `air` and `water` values.
- Re-run calibration with `c`.

### Readings are inverted (dry shows wet)
- The SEN0193 is inverse: higher raw = drier. If `airValue < waterValue`, your calibration is backwards. Re-calibrate with sensor in AIR first, then WATER.

### "no_sensor" error
- Check wiring: VCC → 5V, GND → GND, AOUT → A0
- Check that the sensor probe is not damaged
- Try a different analog pin (update `SENSOR_PIN` in config.h)

### Readings are noisy/jumping
- Ensure the sensor probe is firmly inserted into soil
- Check for loose wiring or breadboard connections
- The trimmed mean already filters most noise, but a 10uF capacitor between A0 and GND can help further
