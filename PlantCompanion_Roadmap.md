# PlantCompanion — Full Project Roadmap

**Last updated:** April 6, 2026
**Author:** Jonah + Claude
**Project phase:** V1 schematic in progress

---

## V1 — Soil Moisture Sensing (Current)

### V1.1 — Finish Schematic

- [ ] **Sensing block page:** Place `Conn_01x03` connector for DIYables capacitive moisture sensor
  - Pin 1 → `+3.3V`
  - Pin 2 → `GND`
  - Pin 3 → `SOIL_AOUT` (connects to ESP32 IO4 / ADC1_CH4)
- [ ] **MCU page typo fix:** Rename net labels `TXDO` → `TXD0` and `RXDO` → `RXD0` (letter O → number zero)
- [ ] **Sys_Pwr net label audit across all pages:**
  - `Sys_Pwr` is the net between the load-sharing circuit output (Q1 drain / D3 cathodes) and TPS63020 VIN. It should only appear on the **Power System page** — no other page should need it.
  - Check each page for any leftover `VBAT` labels that were supposed to become `Sys_Pwr`:
    - **Page 2 (Power System):** Already updated — VINA, VIN (pins 10/11), EN (pin 12), C7, C8 all on `Sys_Pwr` ✓
    - **Page 3 (MCU):** Uses `+3.3V` (TPS63020 output) and `GND` only — no `Sys_Pwr` needed ✓
    - **Page 4 (Programming Interface):** Uses `VBUS` and `USB_D+`/`USB_D-` only — no `Sys_Pwr` needed ✓
    - **Page 5 (Sensing Block):** Uses `+3.3V`, `GND`, `SOIL_AOUT` only — no `Sys_Pwr` needed ✓
  - If ERC flags a `Sys_Pwr` conflict or missing power flag, add a `PWR_FLAG` symbol on the `Sys_Pwr` net on the Power System page
- [ ] **Run ERC (Electrical Rules Check):** Fix all errors and warnings
  - Pay attention to: unconnected pins, net label conflicts, power flag issues
- [ ] **Assign footprints:** Verify every component has a valid footprint assigned
  - Especially check easyeda2kicad parts (SI2301, BAT54A, ESP32-C3-MINI-1-N4, MCP73831, TPS63020)
  - Confirm footprint library paths in Preferences → Manage Footprint Libraries
- [ ] **Generate BOM (Bill of Materials):** Export from KiCad, cross-reference with LCSC/JLCPCB parts availability
- [ ] **Final schematic review:** Verify all inter-page net labels match across all 5 pages

### V1.2 — PCB Layout

- [ ] **Import netlist** into PCB editor
- [ ] **Define board outline:** Spike/stake form factor — narrow PCB that fits inside a waterproof enclosure
  - Consider: width constraints for soil insertion, length for component placement
  - Decide on enclosure strategy (conformal coat vs. potting vs. sealed housing)
- [ ] **Component placement priorities:**
  - USB-C connector at top (accessible end)
  - ESP32-C3 module near top (antenna clearance — keep ground plane away from antenna area per datasheet)
  - Soil sensor connector at bottom (closest to soil)
  - Battery connector accessible for replacement
  - TPS63020 inductor close to IC (minimize loop area)
  - MCP73831 near USB-C connector
  - Decoupling caps as close as possible to their respective IC power pins
- [ ] **Routing guidelines:**
  - 2-layer board (top + bottom)
  - Ground pour on both layers
  - Keep analog trace (SOIL_AOUT) away from switching regulator traces
  - USB D+/D- differential pair: keep traces equal length, ~90Ω impedance if possible
  - Power traces: minimum 0.3mm for signal, 0.5mm+ for power rails (VBUS, VBAT, SYS_PWR, 3.3V)
  - TPS63020 switching loop: VIN caps → VIN pins → L1 → inductor → L2 → VOUT → VOUT caps — keep this loop as tight as possible
- [ ] **Design rule check (DRC):** Fix all violations
- [ ] **Generate Gerbers** for fabrication
- [ ] **Order PCBs** (JLCPCB, PCBWay, or OSH Park)
- [ ] **Order components** from LCSC/Mouser/Digikey (match BOM)

### V1.3 — Assembly & Bring-Up

- [ ] **Solder components** (recommend: reflow with solder paste for SMD parts)
- [ ] **Power-on test (no ESP32 yet):**
  - Verify 3.3V output from TPS63020 with battery connected
  - Verify USB charging: plug USB, confirm LED behavior, measure VBAT rising
  - Measure voltage drop across Q1 (VBUS to SYS_PWR) with USB plugged in
  - Verify load-sharing: system should run from VBUS when USB present, battery when absent
- [ ] **ESP32 bring-up:**
  - Flash test firmware via USB (confirm USB D+/D- wiring works)
  - Verify boot mode (GPIO9 high = normal boot)
  - Test serial output on TXD0/RXD0
- [ ] **Sensor test:**
  - Read ADC values from SOIL_AOUT
  - Test in air (dry), water (saturated), and actual soil at various moisture levels
  - Verify ADC range and linearity

### V1.4 — Firmware (Soil Moisture)

- [ ] **Development environment setup:**
  - ESP-IDF or Arduino framework (recommend ESP-IDF for production, Arduino for fast prototyping)
  - PlatformIO or ESP-IDF toolchain
- [ ] **Core firmware modules:**
  - [ ] ADC driver: Read soil moisture sensor on IO4 (ADC1_CH4)
    - Configure ADC attenuation (probably ADC_ATTEN_DB_11 for 0–3.3V range)
    - Implement averaging (e.g., 10 samples, discard outliers)
  - [ ] WiFi manager: Connect to home WiFi, handle reconnection
    - Store credentials in NVS (non-volatile storage)
    - WiFi provisioning via BLE (SmartConfig or custom BLE service)
  - [ ] BLE service: Expose soil moisture data as a BLE characteristic
    - Define custom GATT service + characteristic UUIDs
    - Notify on value change
  - [ ] Deep sleep manager: Wake on timer, read sensor, transmit, sleep again
    - Target: read every 15–30 minutes
    - Measure current draw in sleep vs. active
  - [ ] Battery monitor: Read battery voltage via ADC (need voltage divider on VBAT → spare ADC pin)
    - Note: V1 hardware may not have this — consider adding a voltage divider in V1.2 PCB layout
  - [ ] OTA updates: Allow firmware updates over WiFi without USB
- [ ] **Data transmission:**
  - MQTT to a local broker (e.g., Mosquitto on Raspberry Pi) or cloud (AWS IoT, Adafruit IO, ThingSpeak)
  - JSON payload: `{ "moisture_raw": 1823, "moisture_pct": 45.2, "battery_v": 3.82, "timestamp": ... }`
  - Or BLE to phone app for direct display

### V1.5 — Phone App (Basic)

- [ ] **Choose framework:** React Native, Flutter, or native (Swift/Kotlin)
- [ ] **BLE connection:** Scan for PlantCompanion devices, pair, read moisture data
- [ ] **Dashboard:** Display current moisture level, battery status
- [ ] **Notifications:** "Water your plant!" when moisture drops below threshold
- [ ] **Threshold configuration:** Let user set wet/dry thresholds per plant type

### V1.6 — Data Collection & Storage

- [ ] **Local logging:** Store readings on ESP32 SPIFFS/LittleFS as CSV (backup if WiFi is down)
- [ ] **Cloud database:** Time-series storage for moisture readings
  - Options: InfluxDB, TimescaleDB, Firebase, or simple SQLite on a server
  - Schema: `device_id, timestamp, moisture_raw, moisture_pct, battery_v, temperature (future)`
- [ ] **Data export:** CSV/JSON download from app or web dashboard
- [ ] **Target:** Collect at least 2–4 weeks of continuous data per plant before V2

---

## V2 — Nutrition Sensing

### V2.1 — Sensor Research & Selection

- [ ] **Evaluate sensor options:**

  | Sensor Type | What It Measures | Interface | Pros | Cons | Approx Cost |
  |---|---|---|---|---|---|
  | **RS485 NPK probe** (e.g., JXBS-3001) | Nitrogen, Phosphorus, Potassium directly | RS485 Modbus RTU | Direct NPK values, industrial grade | Bulky, needs RS485 transceiver, higher power, $15–40 | $$$ |
  | **EC/TDS sensor** (e.g., DFRobot SEN0244) | Electrical conductivity (proxy for total dissolved nutrients) | Analog voltage | Simple, cheap, small, low power | Doesn't distinguish N/P/K, affected by temperature | $ |
  | **Ion-selective electrode (ISE)** | Individual ions (NO3⁻, K⁺, PO4³⁻) | Analog | Specific to individual nutrients | Expensive, short lifespan, drift, needs calibration | $$$$ |
  | **Colorimetric / optical** | Nutrient levels via light absorption | I2C/SPI | Can be very accurate | Complex, needs reagents or optical components | $$$ |

- [ ] **Recommended starting point for V2:** EC/TDS sensor (simplest integration, good enough for "is the soil nutrient-rich or depleted?" use case)
  - If more granularity needed, add RS485 NPK probe as a second sensor
- [ ] **Order evaluation sensors** and test on bench before PCB integration

### V2.2 — Hardware Changes

- [ ] **Schematic updates:**
  - **If EC/TDS sensor (analog):** Add second analog input on ESP32 (e.g., IO5 / ADC1_CH5)
    - Add `Conn_01x03` connector: +3.3V, GND, EC_AOUT
    - Add 0.1µF filter cap on EC_AOUT trace
    - Net label: `EC_AOUT`
  - **If RS485 NPK sensor:** Add RS485 transceiver (e.g., MAX485 or SP3485)
    - UART TX/RX from ESP32 (IO6/IO7 or remap) → transceiver → RS485 A/B lines
    - Add `Conn_01x04` connector: VCC (5V or 12V — check sensor requirements), GND, A, B
    - Note: RS485 sensor may need 5V or even 12V — if 12V, need separate boost converter or external supply
    - Direction control pin (DE/RE) from ESP32 GPIO
  - **If both:** Include both connectors for maximum flexibility
- [ ] **Power budget update:**
  - Recalculate total current draw with new sensor(s)
  - EC sensor: typically < 5mA (negligible)
  - RS485 NPK probe: typically 20–50mA during read (duty cycle matters for battery life)
  - Update deep sleep budget and battery life estimate
- [ ] **PCB layout V2:**
  - Add new connector(s) and any supporting circuitry
  - If RS485: route differential pair for A/B lines
  - Consider board size increase or daughter-board approach
- [ ] **Battery voltage monitoring:** Add voltage divider (2x 100kΩ) from VBAT to spare ADC pin if not already in V1

### V2.3 — Firmware Updates

- [ ] **EC/TDS driver:**
  - ADC read on new analog pin
  - Temperature compensation (EC readings vary ~2%/°C — use thermistor or DS18B20 if available)
  - Convert raw ADC → EC (µS/cm) → TDS (ppm) using calibration coefficients
- [ ] **RS485 NPK driver (if applicable):**
  - Modbus RTU master implementation
  - Send read command, parse N/P/K response registers
  - Handle sensor wake-up timing and power cycling
- [ ] **Updated data payload:**
  ```json
  {
    "device_id": "plant01",
    "timestamp": 1712444800,
    "moisture_raw": 1823,
    "moisture_pct": 45.2,
    "ec_us_cm": 850,
    "tds_ppm": 425,
    "npk_n_mg_kg": 120,
    "npk_p_mg_kg": 45,
    "npk_k_mg_kg": 200,
    "battery_v": 3.82,
    "soil_temp_c": 22.5
  }
  ```
- [ ] **Sensor fusion logic:** Correlate moisture + nutrition readings
  - Wet soil reads differently for EC than dry soil — normalize
  - Flag unreliable readings (e.g., sensor disconnected, out-of-range values)

### V2.4 — Calibration

- [ ] **Soil moisture calibration:**
  - Measure ADC in completely dry soil (air-dry) → record as 0% moisture
  - Measure ADC in fully saturated soil → record as 100% moisture
  - Create linear (or polynomial) mapping: `moisture_pct = f(adc_raw)`
  - Test with 3–5 known moisture levels using gravimetric method (weigh soil before/after drying)
- [ ] **EC/TDS calibration:**
  - Use standard calibration solutions (e.g., 1413 µS/cm and 12880 µS/cm)
  - Two-point calibration: measure ADC at both solutions, compute slope and offset
  - Store calibration coefficients in NVS on ESP32
  - Recalibrate every few months (electrode drift)
- [ ] **NPK calibration (if RS485):**
  - Factory-calibrated (usually), but verify against known soil samples
  - Send soil sample to lab, compare lab results vs. sensor readings
  - Apply correction factors if needed
- [ ] **Temperature compensation:**
  - EC varies ~2%/°C from 25°C reference
  - Either add a temperature sensor (DS18B20 waterproof probe — shares the soil spike) or apply fixed offset
- [ ] **Store calibration data:** NVS partition on ESP32, with ability to recalibrate via app/BLE

### V2.5 — App Updates

- [ ] **Nutrition dashboard:** Display EC/TDS and/or NPK values alongside moisture
- [ ] **Plant profiles:** Presets for common plants (tomato, basil, succulent, etc.) with ideal moisture + nutrient ranges
- [ ] **Alerts:** "Soil nutrients low — time to fertilize!" based on thresholds
- [ ] **Historical charts:** Plot moisture and nutrition over time (daily/weekly/monthly)
- [ ] **Multi-device support:** Manage multiple PlantCompanion spikes across different plants

---

## V3 (Future) — Machine Learning Integration

### V3.1 — Data Pipeline

- [ ] **Centralized data store:** All devices push to a single time-series database
- [ ] **Data quality checks:** Flag outliers, missing readings, sensor drift
- [ ] **Labeling system:** Record ground-truth events
  - When user waters the plant (manual log in app or detected via moisture spike)
  - When user fertilizes
  - Plant health observations (wilting, browning, growth rate)
  - Environmental factors (indoor/outdoor, sunlight, season)
- [ ] **Target dataset:** At least 3–6 months of continuous multi-sensor data across 5+ plants of different types

### V3.2 — Feature Engineering

- [ ] **Time-series features:**
  - Rate of moisture change (drying speed)
  - Time since last watering
  - Moisture at time of watering (user's current habit)
  - Nutrient depletion rate
  - Diurnal patterns (day/night moisture variation from transpiration)
- [ ] **Derived features:**
  - Soil water retention capacity (how fast does moisture drop after watering?)
  - Nutrient uptake rate (how fast do EC/TDS values drop?)
  - Seasonal trends

### V3.3 — Model Development

- [ ] **Watering prediction model:**
  - Input: moisture history, nutrient levels, plant type, environmental context
  - Output: "Water in X hours" or "Water now" confidence score
  - Start simple: linear regression or decision tree, then upgrade to LSTM/RNN for time-series
- [ ] **Nutrient recommendation model:**
  - Input: EC/TDS trends, NPK levels (if available), plant type, growth stage
  - Output: "Fertilize with N-heavy mix" or "Nutrients adequate"
- [ ] **Anomaly detection:**
  - Detect sensor failures, unusual patterns (overwatering, root rot indicators)
- [ ] **On-device vs. cloud inference:**
  - ESP32-C3 has limited RAM (~400KB) — lightweight models only (TinyML with TensorFlow Lite Micro)
  - Or run inference in the cloud/phone app and push recommendations to device

### V3.4 — Model Deployment

- [ ] **TFLite Micro on ESP32** (if on-device): Convert trained model, optimize for size
- [ ] **Cloud inference** (if cloud): API endpoint that receives sensor data, returns recommendation
- [ ] **A/B testing:** Compare ML recommendations vs. simple threshold-based alerts
- [ ] **Feedback loop:** User confirms or rejects recommendations → improves model over time

---

## Hardware Summary (All Versions)

| Component | V1 | V2 | Notes |
|---|---|---|---|
| ESP32-C3-MINI-1-N4X | ✓ | ✓ | MCU, WiFi+BLE |
| TPS63020DSJR | ✓ | ✓ | 3.3V buck-boost |
| MCP73831T-2ACI/OT | ✓ | ✓ | LiPo charger |
| SI2301 P-FET | ✓ | ✓ | Load sharing |
| BAT54A Schottky | ✓ | ✓ | Load sharing |
| USB-C connector | ✓ | ✓ | Charging + programming |
| USBLC6-2SC6 | ✓ | ✓ | USB ESD protection |
| Capacitive soil moisture sensor | ✓ | ✓ | Analog output |
| EC/TDS sensor | — | ✓ | Analog output, nutrition proxy |
| RS485 NPK probe (optional) | — | ✓ | Modbus RTU, direct NPK |
| DS18B20 temp sensor (optional) | — | ✓ | Temp compensation for EC |
| RS485 transceiver (if NPK) | — | ✓ | MAX485 / SP3485 |

---

## Known Issues & Decisions to Revisit

- **Load-sharing gate bias (R12 → VBAT):** Works at low current but VGS is marginal (~-1.3V). If V2 draws more current, consider: lower-threshold FET, gate-to-GND with anti-backfeed resistor, or charger IC with built-in power-path management (e.g., BQ24092)
- **No battery voltage monitoring in V1:** Need to add voltage divider for battery level reporting
- **ESP32-C3-MINI-1-N4 vs N4X:** Design targets N4X (newer revision). Verify LCSC availability before ordering
- **Antenna keepout:** Must maintain ground-plane clearance around ESP32 module antenna per datasheet
- **Waterproofing:** Critical for a soil-inserted device. Plan enclosure/conformal coating strategy before V1 board order
