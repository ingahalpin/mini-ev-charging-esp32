# Mini EV Charging Station Simulator

A playful, building block based EV charging station that **simulates** an EV charging session (it does **not** charge a real battery). Kids "plug in" a car using a hidden button (or optional magnetic switch), and the station responds with clear feedback:

- **LED status** — colour and blink pattern reflect every state of the session
- **Live LCD readout** — state, session timer, kWh delivered, and cost
- **Wi-Fi web dashboard** — full control and live data in any browser, no app needed

The goal is instant understanding: **Plug → light change → numbers go up**.

## What this project is (and isn't)

### It is
A safe demo that shows how charging *looks* and *feels* using a state machine and a simulated charging meter.

### It is not
A real EV charger and not a battery charger. It only simulates energy and cost.

---

## State Machine

The charger moves through five states, triggered by the physical button or the web UI:

| State | LED green | LED red | kWh / cost |
|---|---|---|---|
| **Idle** | Steady on | Off | 0 |
| **Ready!** | Off | Blinking | 0 |
| **Charging…** | Off | Steady on | Counting up |
| **Charging stopped** | Blinking | Off | Frozen at stop value |
| **Charging complete** | Blinking | Off | Frozen at cap value |

Button press cycle: `Idle → Ready → Charging → Stopped → Idle`

Auto-transition: `Charging → Charging complete` when a kWh or cost cap is reached.

---

## Features

### Physical controls
- Hidden push button for state transitions ("plug in" the car to start the cycle)
- Debounced button input with `INPUT_PULLUP`

### LED feedback
- Green LED: steady on (Idle), blinking (session ended)
- Red LED: blinking (Ready), steady on (Charging)
- All blink/steady patterns driven without `delay()` using `millis()`

### LCD display (16×2 I2C)
- Row 0: current state + live session timer (`Chg 0:42`)
- Row 1: kWh delivered and cost (`0.42 kWh E0.15`)

### Web dashboard (Wi-Fi AP)
- **Live updates** — key values refresh every second via JavaScript polling, no page flash
- **State-aware action button** — shows exactly the right next action for the current state
- **Resume charging** — if a session was manually stopped (before any cap was hit), a "Resume Charging" button lets you continue from where you left off; kWh and session timer carry on seamlessly
- **Session history** — last 5 completed sessions (kWh, cost, duration) stored in ESP32 NVS flash; survives power cycles; viewable at `/history` with a clear button
- **Settings panel** (visible in Idle) — configure:
  - Charge limit (kWh cap)
  - Price per kWh (EUR)
  - Cost cap (EUR, 0 = off)

### Charging simulation
- kWh increments by `0.01` every 500 ms while in Charging state
- Session auto-completes when the kWh limit **or** cost cap is reached (whichever triggers first)
- Cost = `kWh × pricePerKWh`

### JSON status API
`GET /api/status` returns the current machine state as JSON — useful for integrations, dashboards, or future companion apps:
```json
{ "state": "Charging...", "kwh": "0.42", "cost": "0.15", "duration": "0:42" }
```

---

## Hardware

| Component | Notes |
|---|---|
| ESP32 dev board | Any standard 38-pin board |
| Push button | Hidden under the building block "plug" |
| Red LED + resistor | State indicator |
| Green LED + resistor | State indicator |
| I2C LCD 16×2 | `LCD_ST7032` library |

---

## Wiring

**Power rails**
- ESP32 **GND** → ground rail
- ESP32 **3V3** → 3.3 V rail

**Button**
- **GPIO18** → button → **GND** (`INPUT_PULLUP`)

**LEDs**
- **GPIO23** → Red LED (with resistor) → GND
- **GPIO4** → Green LED (with resistor) → GND

**LCD (I2C)**
- LCD **GND** → ESP32 **GND**
- LCD **VCC** → **5 V** (or 3.3 V, depending on module)
- LCD **SDA** → **GPIO21**
- LCD **SCL** → **GPIO22**

---

## Wi-Fi & Web UI

The ESP32 runs as a Wi-Fi Access Point:
- **SSID:** `EV-Charger`
- **Password:** `12345678`
- Open `http://192.168.4.1` in any browser once connected

### Routes

| Route | Description |
|---|---|
| `GET /` | Main dashboard |
| `GET /ready` | Idle → Ready |
| `GET /start` | Ready → Charging |
| `GET /stop` | Charging → Charging stopped |
| `GET /resume` | Charging stopped → Charging (resumes session) |
| `GET /reset` | Charging stopped / complete → Idle |
| `GET /setlimit?kwh=` | Set kWh charge limit |
| `GET /setprice?eur=` | Set price per kWh |
| `GET /setcostcap?eur=` | Set cost cap (0 = off) |
| `GET /api/status` | JSON status endpoint |
| `GET /history` | Session history page |
| `GET /clearhistory` | Clear stored session history |

---

## User flow

1. Place the toy car at the station — the station is in **Idle** (green LED steady).
2. Press the hidden button (or tap **Ready!** in the browser) — red LED starts blinking.
3. Press again (or tap **Start Charging**) — red LED goes steady, kWh and cost count up.
4. Press again (or tap **Stop Charging**) — green LED blinks, session is paused.
   - Tap **Resume Charging** to continue the session from where it left off.
   - Tap **Back to Idle** to end the session and reset.
5. If a kWh or cost cap is reached, charging stops automatically → **Charging complete**.
6. Tap **Back to Idle** (or press the button) to reset for the next car.

---

## Dependencies

Install via Arduino Library Manager:

- `LCD_ST7032` — I2C LCD driver
- `WiFi` / `WebServer` — bundled with the ESP32 Arduino core
- `Preferences` — bundled with the ESP32 Arduino core (used for NVS session history)

---

## Safety note

This is a **simulation toy**. Do not connect it to any real EV, real charging cable, or any battery charging circuitry.

---

## License

Licensed under MIT License.
