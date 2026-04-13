# Plant Companion V1 — System Design Spec

## Overview

Plant Companion is a hardware + software product that helps houseplant owners understand when their plants need water. V1 is a Kickstarter-ready prototype proving the core loop: a physical sensor reads soil moisture, sends data to a cloud backend, and a web app displays plant status with simple watering guidance.

## Architecture

```
Arduino Uno R3 + SEN0193
        │ USB serial (JSON, 9600 baud)
        ▼
Bridge Script (Python, pyserial)
        │ HTTPS POST
        ▼
Backend (FastAPI + PostgreSQL) — hosted on Railway
        │ REST API
        ▼
Frontend (React + Vite + Tailwind) — hosted on Vercel
```

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| MCU | Arduino Uno R3 | Already selected, parts ordered |
| Sensor | DFRobot SEN0193 | Capacitive, corrosion-resistant, ~$4 |
| Backend | Python / FastAPI | Fast to build, good async support |
| Frontend | React + Vite + Tailwind | Fast iteration, calming UI direction |
| Database | PostgreSQL (TimescaleDB later) | Multi-user ready, easy TimescaleDB upgrade path |
| Frontend hosting | Vercel | Native React/Vite support |
| Backend hosting | Railway | Proper server, no cold starts, free tier |
| Auth | Email/password + JWT | Multi-user from day one |
| Data interval | 30s default, 5s demo mode | Configurable in firmware + bridge |
| Plant input | Freeform text | Ship fast, species DB later |
| Multi-plant | Yes | Small upfront cost, avoids painful refactor |
| Real-time | REST polling (10s) | Simplest. SSE upgrade path if needed |

---

## Component 1: Firmware

### Hardware wiring

```
SEN0193       Arduino Uno R3
VCC      →    5V
GND      →    GND
AOUT     →    A0
```

### Behavior

1. Initialize serial at 9600 baud
2. Read interval config from EEPROM (default 30s, 5s demo mode)
3. Main loop:
   - Take 10 analog reads from A0, 50ms apart (500ms sampling window)
   - Discard highest and lowest values (trimmed mean of 8)
   - Compute raw average (0–1023)
   - Convert to percentage: `moisture_pct = (AirValue - raw) / (AirValue - WaterValue) * 100`, clamped 0–100
   - Output JSON over serial: `{"m": 452, "p": 63.2, "ts": 84000}`
   - Wait remaining interval time
   - Repeat

### Calibration constants (V1 defaults)

- `AirValue = 520` (sensor in air = dry)
- `WaterValue = 260` (sensor in water = saturated)
- Inverse relationship: higher raw value = drier soil

### Error handling

- Raw value > 1000 → sensor disconnected, send `{"err": "no_sensor"}`
- Compact JSON keys (`m`, `p`, `ts`) to conserve 2KB RAM

### Demo mode

- Serial command `d` switches to 5s interval, `n` switches to 30s
- Stored in EEPROM, survives reboot

### Files

```
firmware/plant_companion/
  plant_companion.ino    — main sketch
  config.h               — pin defs, calibration constants, intervals
```

---

## Component 2: Bridge Script

### Purpose

Runs on the computer the Arduino is plugged into. Reads serial JSON, adds UTC timestamp, POSTs to backend API.

### Dependencies

- `pyserial` — serial port communication
- `requests` — HTTP client
- `pyyaml` — config parsing

### Config (`config.yaml`)

```yaml
serial:
  port: auto          # auto-detect via USB VID/PID (2341:0043)
  baud: 9600
api:
  url: https://api.plantcompanion.com
  device_key: "pk_abc123..."
mode: normal          # "normal" or "demo"
```

### Behavior

1. Load config
2. Auto-detect Arduino serial port by USB VID/PID, retry every 5s if not found
3. Open serial, send mode byte (`d` or `n`) to Arduino
4. Read loop:
   - Read line, parse JSON, validate fields
   - Enrich: add `device_key`, real UTC `timestamp`
   - POST to `POST /api/readings`
   - On success: log
   - On 401: bad device key, halt
   - On 5xx/timeout/network error: queue in memory (max 1000), retry with exponential backoff (2s → 4s → 8s → max 60s)
   - Flush queue in order when connection restores

### Error handling

- `SerialException` on disconnect → retry connection every 5s
- Malformed JSON from serial → log warning, skip line
- SSL errors → log and retry with backoff (same as 5xx handling)
- No disk persistence for queue — acceptable for V1

### Files

```
bridge/
  bridge.py
  config.yaml
  requirements.txt
```

---

## Component 3: Backend

### Data model

```sql
users
  id            UUID PK
  email         VARCHAR UNIQUE NOT NULL
  password_hash VARCHAR NOT NULL
  created_at    TIMESTAMPTZ DEFAULT NOW()

plants
  id            UUID PK
  user_id       UUID FK → users NOT NULL
  name          VARCHAR NOT NULL
  created_at    TIMESTAMPTZ DEFAULT NOW()

devices
  id            UUID PK
  user_id       UUID FK → users NOT NULL
  plant_id      UUID FK → plants (nullable, set when paired)
  device_key    VARCHAR UNIQUE NOT NULL
  label         VARCHAR NOT NULL
  last_seen_at  TIMESTAMPTZ
  created_at    TIMESTAMPTZ DEFAULT NOW()

sensor_readings
  id            BIGSERIAL PK
  device_id     UUID FK → devices NOT NULL
  plant_id      UUID FK → plants (nullable, NULL if device unpaired)
  moisture_raw  INTEGER NOT NULL
  moisture_pct  FLOAT NOT NULL
  recorded_at   TIMESTAMPTZ NOT NULL (from bridge)
  created_at    TIMESTAMPTZ DEFAULT NOW()

INDEX: sensor_readings (plant_id, recorded_at DESC)
```

`sensor_readings` uses BIGSERIAL (not UUID) for insert performance on high-volume table. This table converts to a TimescaleDB hypertable later.

### API endpoints

**Auth**
- `POST /api/auth/signup` — `{email, password}` → `{user, token}`
- `POST /api/auth/login` — `{email, password}` → `{token}`

**Plants** (JWT required)
- `GET /api/plants` — list user's plants with current status
- `POST /api/plants` — `{name}` → `{plant}`
- `GET /api/plants/{id}` — plant detail + latest reading + status
- `PUT /api/plants/{id}` — update plant name
- `DELETE /api/plants/{id}` — delete plant. Cascades: sets `devices.plant_id = NULL` for any paired devices. Orphaned readings remain in DB.

**Devices** (JWT required)
- `GET /api/devices` — list user's devices
- `POST /api/devices` — `{label}` → `{device, device_key}` (key shown once)
- `POST /api/devices/{id}/pair` — `{plant_id}` → pair device to plant
- `POST /api/devices/{id}/unpair` — remove plant association, set `plant_id = NULL`

### Device pairing rules

- `POST /api/devices/{id}/pair` accepts `{plant_id}`. Both the device and plant must be owned by the requesting user (403 otherwise).
- Pairing is idempotent — pairing to the same plant again is a no-op 200.
- Re-pairing to a different plant is allowed (updates `plant_id`). Previous readings retain their original `plant_id`.
- Unpaired devices can still receive readings via bridge, but readings are stored with `plant_id = NULL` and not shown on any dashboard until the device is paired.
- A device can only be paired to one plant at a time (1:1).

**Readings** (device_key auth via `X-Device-Key` header)
- `POST /api/readings` — `{moisture_raw, moisture_pct, timestamp}`

**Dashboard** (JWT required)
- `GET /api/plants/{id}/status` — `{moisture_pct, state, recommendation, last_updated}`
- `GET /api/plants/{id}/readings?hours=24` — `[{moisture_pct, recorded_at}]`

### Recommendation engine (rule-based)

| Moisture % | State | Label | Action |
|-----------|-------|-------|--------|
| < 20 | very_dry | Very Thirsty | Water now |
| 20–39 | dry | Getting Dry | Water soon |
| 40–69 | good | Happy | No action needed |
| 70–84 | wet | Well Watered | No action needed |
| >= 85 | very_wet | Too Wet | Let it dry out |

Generic thresholds for V1, derived from common houseplant moisture ranges. To be validated with 5+ plant species during testing. Species-specific overrides planned post-launch.

### Auth implementation

- JWT via `python-jose`, password hashing via `passlib[bcrypt]`
- Token expiry: 7 days, no refresh token for V1
- Bridge uses `device_key` in `X-Device-Key` header (separate from user JWT)
- Device key is revocable — user can regenerate in app

### Device offline detection

- Offline is computed at query time: if `devices.last_seen_at` is older than 3x the expected interval (90s normal, 15s demo), the device status returns `"offline"`.
- `last_seen_at` is updated only on successful reading ingestion (not on failed POSTs or bridge retries).
- Frontend shows "Device offline" warning on dashboard when status is offline.

### Duplicate reading protection

- Deduplication key: `(device_id, recorded_at)` using the bridge-supplied UTC timestamp with millisecond precision.
- If a reading with the same key arrives within a 1s window, it is silently ignored (200 response, no insert).
- This makes bridge retries safe — the same reading can be POSTed multiple times without creating duplicates.

### Project structure

```
backend/
  app/
    __init__.py
    main.py           — FastAPI app, CORS, lifespan
    config.py         — env vars (DATABASE_URL, JWT_SECRET)
    database.py       — async SQLAlchemy engine + session
    models/           — SQLAlchemy ORM (user, plant, device, reading)
    schemas/          — Pydantic models (request/response)
    routers/          — route handlers (auth, plants, devices, readings)
    services/
      auth.py         — JWT + password utils
      recommendation.py — rule-based engine
  alembic/            — DB migrations
  tests/
  requirements.txt
  Dockerfile
```

---

## Component 4: Frontend

### Pages

| Route | Purpose |
|-------|---------|
| `/login` | Email + password login |
| `/signup` | Create account |
| `/setup` | Add plant (name) + register device (shows device_key) |
| `/dashboard` | Plant list + selected plant status |
| `/plants/{id}` | Single plant detail with history chart |

### Dashboard layout

```
┌─────────────────────────────────────────┐
│  Plant Companion              [+ Add]    │
├──────────┬──────────────────────────────┤
│          │                              │
│ Monstera │    Happy                     │
│ ──────── │    Moisture: 63%             │
│ Fern     │    ████████░░  Good          │
│          │    "No action needed"        │
│          │    Device: Connected          │
│          │    Last updated: 2 min ago    │
│          │    ┌─── 24h History ────┐     │
│          │    │  chart             │     │
│          │    └────────────────────┘     │
└──────────┴──────────────────────────────┘
```

- Left: plant list (multi-plant selector)
- Main: status card, moisture bar, recommendation, device status, 24h chart
- Polls `/api/plants/{id}/status` every 10s
- Polls `/api/plants/{id}/readings?hours=24` every 60s

### Visual state mapping

| State | Visual | Color palette |
|-------|--------|--------------|
| very_dry | Wilting / sad | Red-orange |
| dry | Slightly droopy | Yellow |
| good | Happy / thriving | Green |
| wet | Content | Blue-green |
| very_wet | Drowning / worried | Blue |

Simple emoji or minimal illustrations for V1.

### Device setup flow

1. User clicks "Add Plant" → enters plant name
2. User clicks "Add Device" → backend returns `device_key`
3. App shows key once with copy button + setup instructions
4. User pairs device to plant
5. Dashboard shows data once first reading arrives

### Tech stack

- Vite + React + TypeScript
- React Router for page routing
- Recharts for moisture history chart
- Tailwind CSS with custom soft color palette
- Axios for API calls

### Project structure

```
frontend/src/
  main.tsx
  App.tsx
  api/client.ts         — axios wrapper, auth headers
  hooks/
    useAuth.ts
    usePlants.ts
    usePolling.ts        — reusable polling hook
  pages/
    Login.tsx
    Signup.tsx
    Setup.tsx
    Dashboard.tsx
  components/
    PlantList.tsx
    PlantStatus.tsx      — main status card
    MoistureBar.tsx      — visual gauge
    MoistureChart.tsx    — 24h history
    DeviceStatus.tsx
    PlantAvatar.tsx      — emoji/visual state
  context/
    AuthContext.tsx
  styles/
    globals.css
```

---

## Monorepo structure

```
plant-soil/                          (git root)
├── README.md
├── .gitignore
├── docker-compose.yml               — local dev: backend + postgres
├── docs/
├── firmware/plant_companion/
├── bridge/
├── backend/
└── frontend/
```

`docker-compose.yml` runs PostgreSQL + FastAPI for local dev. Frontend runs via `npm run dev` with Vite proxy to backend.

---

## Local dev workflow

```bash
# Terminal 1: DB + Backend
docker-compose up

# Terminal 2: Frontend
cd frontend && npm run dev

# Terminal 3: Bridge (Arduino plugged in)
cd bridge && python bridge.py
```

---

## Risk register

| Risk | Impact | Mitigation |
|------|--------|-----------|
| SEN0193 waterproofing — circuit board above red line is NOT waterproof | Sensor failure in wet soil | Document placement guidance, heat shrink for V2 |
| Calibration varies per sensor unit | Inaccurate moisture % | Hardcoded defaults are close enough for demo, add calibration flow later |
| Arduino Uno has no wireless | Requires bridge computer nearby | Known V1 constraint. ESP32 in V2 eliminates bridge |
| Serial port names vary by OS | Bridge fails to find Arduino | Auto-detect by VID/PID with manual fallback |
| Readings table grows unbounded | DB bloat over time | Add retention policy; TimescaleDB upgrade path planned |
| Device key in plaintext config | Security concern if leaked | Keys are scoped (readings only) and revocable |
