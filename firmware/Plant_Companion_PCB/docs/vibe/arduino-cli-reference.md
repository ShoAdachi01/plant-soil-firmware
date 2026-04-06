# Arduino CLI Reference — Plant Companion

Quick reference for the hardware team to compile, upload, and test firmware using Arduino CLI.

## Install Arduino CLI

### macOS (Homebrew)
```bash
brew install arduino-cli
```

### Linux
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv bin/arduino-cli /usr/local/bin/
```

### Windows
```bash
# Via Scoop
scoop install arduino-cli

# Or download from https://arduino.github.io/arduino-cli/installation/
```

## First-Time Setup

```bash
# Create config file
arduino-cli config init

# Update board index
arduino-cli core update-index

# Install Arduino AVR core (needed for Uno)
arduino-cli core install arduino:avr

# Verify it installed
arduino-cli core list
# Should show: arduino:avr
```

## Connect Arduino Uno

```bash
# List connected boards — find your port and FQBN
arduino-cli board list
```

Expected output:
```
Port         Protocol Type   Board Name    FQBN             Core
/dev/ttyACM0 serial   Serial Arduino Uno   arduino:avr:uno  arduino:avr
```

- **macOS port**: `/dev/tty.usbmodem*` or `/dev/cu.usbmodem*`
- **Linux port**: `/dev/ttyACM0` or `/dev/ttyUSB0`
- **Windows port**: `COM3`, `COM4`, etc.
- **FQBN for Uno**: `arduino:avr:uno`

## Compile Firmware

```bash
# From the plant-soil repo root
arduino-cli compile --fqbn arduino:avr:uno firmware/plant_companion
```

This compiles the sketch at `firmware/plant_companion/plant_companion.ino`.

On success you'll see:
```
Sketch uses XXXX bytes (XX%) of program storage space. Maximum is 32256 bytes.
Global variables use XXX bytes (XX%) of dynamic memory. Maximum is 2048 bytes.
```

**Watch the memory usage** — Uno has 32KB flash and 2KB RAM. If dynamic memory exceeds ~75%, you may see instability.

## Upload Firmware

```bash
# Replace port with your actual port from `board list`
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:uno firmware/plant_companion
```

Or compile + upload in one step:
```bash
arduino-cli compile --upload -p /dev/ttyACM0 --fqbn arduino:avr:uno firmware/plant_companion
```

## Monitor Serial Output

```bash
# Open serial monitor at 9600 baud
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600
```

You should see JSON lines like:
```json
{"m":452,"p":63.08,"ts":30500}
{"m":448,"p":64.62,"ts":61000}
```

Press `Ctrl+C` to exit the monitor.

## Send Serial Commands

While the monitor is open, you can type:
- `d` — switch to demo mode (5s interval)
- `n` — switch to normal mode (30s interval)
- `c` — enter calibration mode (prints raw values at 100ms for 30s)
- `s` — print current status (config, last reading, uptime)

## Troubleshooting

### "No board found on port"
- Check USB cable is data-capable (not charge-only)
- Try a different USB port
- Run `arduino-cli board list` to find the correct port
- On Linux: add user to `dialout` group: `sudo usermod -a -G dialout $USER`

### "avrdude: stk500_recv(): programmer is not responding"
- Press the reset button on the Uno right before uploading
- Close any other serial monitor (only one program can use the port)
- Try a different USB cable

### Compilation errors
- Ensure `arduino:avr` core is installed: `arduino-cli core list`
- Ensure you're compiling from repo root (path should be `firmware/plant_companion`)

### Serial monitor shows garbage
- Confirm baud rate matches firmware (9600): `--config baudrate=9600`
- Check wiring — loose connections cause intermittent garbage

## Useful Commands

```bash
# List all installed cores
arduino-cli core list

# Search for libraries
arduino-cli lib search "soil moisture"

# Install a library
arduino-cli lib install "LibraryName"

# List installed libraries
arduino-cli lib list

# Export compiled binary to sketch folder
arduino-cli compile --fqbn arduino:avr:uno --export-binaries firmware/plant_companion

# Get board details
arduino-cli board details --fqbn arduino:avr:uno
```

## Sources

- [Arduino CLI Official Docs](https://docs.arduino.cc/arduino-cli/)
- [Arduino CLI Getting Started](https://arduino.github.io/arduino-cli/0.35/getting-started/)
- [Arduino CLI GitHub](https://github.com/arduino/arduino-cli)
- [DFRobot SEN0193 Wiki](https://wiki.dfrobot.com/Capacitive_Soil_Moisture_Sensor_SKU_SEN0193)
- [DFRobot SEN0193 Calibration Code](https://wiki.dfrobot.com/sen0193/docs/18037)
