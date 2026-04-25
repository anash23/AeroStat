# AeroStat

A Wi-Fi–enabled, temperature-controlled smart fan built on the ESP32. Once the room temperature crosses a configurable threshold the fan turns on automatically. A built-in web dashboard lets you monitor readings in real time, switch between **Auto** and **Manual** modes, and adjust the trigger temperature from any device on the same network.

---

## Hardware Components

| Component | Purpose |
|-----------|---------|
| ESP32 (30-pin DevKit) | Main microcontroller + Wi-Fi + web server |
| LM35 temperature sensor | Primary analog temperature measurement |
| LM358 op-amp | Unity-gain buffer (voltage follower) for the LM35 output |
| DHT11 sensor | Ambient humidity + secondary temperature reading |
| P-channel MOSFET | High-side switch for the 12 V fan |
| 12 V DC fan | Controlled load |
| 10 kΩ resistor | Pull-up on DHT11 data line |

---

## Pin Mapping

| ESP32 GPIO | Connected to |
|------------|-------------|
| GPIO 4     | DHT11 data (10 kΩ pull-up to 3.3 V) |
| GPIO 26    | DAC output → P-channel MOSFET gate |
| GPIO 34    | ADC input ← LM358 output (buffered LM35 signal) |

> **GPIO 34** is an input-only pin on ESP32 — ideal for ADC use with no risk of accidental output drive.

---

## Circuit Notes

```
3.3 V ──┬── 10 kΩ ──┐
        │           ├──> DHT11 VCC
        │           └──> DHT11 DATA ──> GPIO 4
GND ───────────────────> DHT11 GND

LM35 VS → 3.3 V
LM35 GND → GND
LM35 OUT → LM358 IN+ (pin 3)
LM358 IN- (pin 2) → LM358 OUT (pin 1)   ← voltage follower
LM358 OUT ──────────────────────────────> GPIO 34 (ADC)

GPIO 26 (DAC) ──> MOSFET gate (P-ch, e.g. IRF9540)
12 V ──────────> MOSFET source
MOSFET drain ──> Fan (+)
Fan (−) ────────> GND
```

**Fan control logic (P-channel MOSFET):**
- DAC ≈ 0 V → MOSFET **ON** → Fan **ON**
- DAC ≈ 2.6 V → MOSFET **OFF** → Fan **OFF**

---

## Software Dependencies

Install via the Arduino Library Manager:

| Library | Version |
|---------|---------|
| DHT sensor library (Adafruit) | ≥ 1.4.4 |
| Adafruit Unified Sensor | ≥ 1.1.9 |

Board support: **ESP32 by Espressif** (via Boards Manager URL `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`)

---

## Setup & Flash

1. Open `AeroStat/AeroStat.ino` in the Arduino IDE (or VS Code + Arduino extension).
2. Edit the Wi-Fi credentials near the top of the file:
   ```cpp
   const char* ssid     = "YOUR_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
3. Select board **ESP32 Dev Module** and the correct COM port.
4. Upload. Open the Serial Monitor at **115200 baud** to see the assigned IP address.
5. Open a browser on the same network and navigate to `http://<IP_ADDRESS>/`.

---

## Web Dashboard

The dashboard auto-refreshes every 10 seconds and provides:

- **LM35 temperature** — large hero display (primary control sensor)
- **DHT11 temperature** — secondary reading for cross-reference
- **Humidity** — from DHT11
- **Fan status** — ON / OFF with colour coding
- **Mode toggle** — Auto (threshold-based) / Manual
- **Threshold slider** — 20 °C – 50 °C, applied immediately
- **Manual fan buttons** — Force fan ON or OFF (switches to Manual mode)

---

## How It Works

In **Auto** mode the main loop reads the LM35 temperature once per second. If the temperature exceeds `threshold`, the fan turns on; otherwise it turns off. The DAC on GPIO 26 drives the P-channel MOSFET gate accordingly.

In **Manual** mode the auto-control is suspended and the fan state is set only through the web dashboard buttons.

