# Mini EV Charging Station Simulator

A playful, building block based EV charging station that **simulates** an EV charging session (it does **not** charge a real battery). Kids “plug in” a car using a hidden button (or optional magnetic switch), and the station responds with clear feedback:

- **LED status** (Idle / Connected / Charging / Finished)
- **Live LCD readout** (state, kWh delivered, cost)
- Optional **ESP32 Wi‑Fi access point + web dashboard** that mirrors the same values

The goal is instant understanding: **Plug → light change → numbers go up**.

## What this project is (and isn’t)

### It is
A safe demo that shows how charging *looks* and *feels* using a simple state machine and a charging “meter”.

### It is not
A real EV charger and not a battery charger. It only simulates energy and cost.

---

## Features

- ESP32-based controller
- Hidden push button for “plug detection”
- Status LED output (example code uses one LED pin)
- I2C LCD output (example uses `LCD_ST7032`)
- Wi‑Fi AP + web UI at `/` with live status/energy/cost

---

## Hardware

Core parts:
- ESP32 dev board
- Push button (hidden under a building block “plug”)
- LED + resistor
- I2C LCD (16x2 style)

---

## Wiring (reference)

Power rails:
- ESP32 **GND** → ground rail
- ESP32 **3V** → 3.3V rail

Button:
- **GPIO18** → button → **GND** (use `INPUT_PULLUP`)

LED:
- **GPIO23** → LED (red) (with resistor) → supply/ground as appropriate (check your LED orientation)
- **GPIO4** → LED (green) (with resistor) → supply/ground as appropriate (check your LED orientation)
- Both LED + resistor tied to 3V and GPIO driving the other side.

LCD (I2C):
- LCD **GND** → ESP32 **GND**
- LCD **VCC** → **5V** (or **3.3V**, depending on your module)
- LCD **SDA** → **GPIO21**
- LCD **SCL** → **GPIO22**

---

## How it works (user flow)

1. Place the toy car at the station.
2. “Plug in” the car (simulated by pressing the hidden button under the plug).
3. Watch the station respond:
   - LED changes to show state
   - LCD shows live charging numbers (kWh and cost)
4. Optional: open the Wi‑Fi dashboard to see the same values in a browser.

---

## Firmware / Code

This repo includes an Arduino-style sketch that demonstrates:

- ESP32 Wi‑Fi Access Point:
  - SSID: `EV-Charger`
  - Password: `12345678`
- Web server on port 80:
  - `/` shows status, kWh, and cost
  - `/start` and `/stop` endpoints toggle charging
- Button debounce + toggle charging on press
- Simple charging simulation:
  - When charging, `kWh` increases by `0.01` per loop step in the example
  - Cost is computed as `kWh * pricePerKWh` (example uses `0.35`)

---

## Safety note

This is a **simulation toy**. Do not connect it to any real EV, real charging cable, or any battery charging circuitry.

---

## License

Licensed under MIT License.
