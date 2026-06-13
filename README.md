# ESP32 Smart House

A small home-monitoring system built on a DOIT ESP32 DevKit V1. It watches an obstacle sensor, a water/rain sensor, and temperature, and raises an alarm (red LED + buzzer) when any of them trips. When everything is normal, a green LED shows all-clear. The whole system can be armed or disarmed with a physical button **or** from a live web dashboard served by the ESP32 itself.

Built with [PlatformIO](https://platformio.org/) and the Arduino framework.

## Features

- **Three triggers in one alarm** — IR obstacle detection, water/rain detection, and a high-temperature threshold.
- **Status LEDs + buzzer** — green when clear, red plus buzzer when any trigger fires.
- **Physical arm/disarm button** with software debouncing.
- **Live web dashboard** — view temperature, humidity, and each trigger's state from any browser on the same WiFi, and arm/disarm remotely. Updates once per second.
- **Named alarms** — the dashboard banner says exactly what tripped (e.g. *Rain detected*, *Obstacle detected*, *High temp detected*, or a combination).
- **Tunable in one place** — all thresholds and sensor polarities are constants at the top of the file.

## Hardware

| Component | Notes |
|---|---|
| DOIT ESP32 DevKit V1 | Main controller (240MHz, 4MB flash) |
| DHT11 | Temperature + humidity sensor |
| IR obstacle sensor (FC-51 or similar) | Digital obstacle detection |
| Water / rain sensor (3-pin: S / + / −) | Analog water-level detection |
| Green LED + 220Ω resistor | All-clear indicator |
| Red LED + 220Ω resistor | Alarm indicator |
| Active buzzer module | Audible alarm |
| Push button | Arm / disarm toggle |

## Wiring

All sensors run on **3.3V** in this build — no 5V rail required. Every component shares a common ground with the ESP32.

| Component | Module pin | ESP32 pin |
|---|---|---|
| DHT11 | DATA | GPIO4 |
| IR obstacle | OUT | GPIO18 |
| Water sensor | S (analog) | GPIO34 |
| Button | one leg → GPIO, other → GND | GPIO23 |
| Green LED | anode → 220Ω → GPIO, cathode → GND | GPIO25 |
| Red LED | anode → 220Ω → GPIO, cathode → GND | GPIO26 |
| Buzzer | signal | GPIO27 |

Power: DHT11 `VCC`, IR `VCC`, and water sensor `+` all go to **3V3**; all grounds to **GND**. The button uses the ESP32's internal pull-up, so no external resistor is needed.

> **Note on GPIO34:** it's an input-only ADC1 pin, which is exactly what the analog water sensor needs and is safe to use alongside WiFi.

### Hardware setup

![Hardware setup](images/hardware.jpg)

*Replace `images/hardware.jpg` with a photo of your assembled circuit.*

## Simulation

![Circuit simulation](images/simulation.png)

*Replace `images/simulation.png` with a screenshot of your Wokwi / Tinkercad simulation.*

## Software setup

### 1. Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode), or the PlatformIO Core CLI.

### 2. Dependencies

These are declared in `platformio.ini` and installed automatically on the first build:

- `adafruit/DHT sensor library`
- `adafruit/Adafruit Unified Sensor`

### 3. Configure WiFi

Open `src/main.cpp` and set your network credentials near the top:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

The ESP32 connects to **2.4GHz** networks only — it will not see a 5GHz-only SSID.

### 4. Build and upload

```bash
pio run --target upload
```

Then open the serial monitor at **115200 baud** to see the assigned IP address:

```bash
pio device monitor
```

You'll see something like:

```
Connecting to WiFi.....
Connected. Open http://192.168.1.42
```

## Usage

### Physical control

Press the button to arm or disarm the system. When disarmed, all outputs are off and the alarm logic is paused.

### Web dashboard

With your phone or computer on the same WiFi, open the IP shown in the serial monitor. The dashboard shows a status banner, live temperature and humidity, a chip for each trigger, and a button to arm/disarm. The physical button and the web button stay in sync.

### HTTP endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | The dashboard page |
| `/status` | GET | Current state as JSON |
| `/toggle` | GET | Flip the system on/off, returns new state |

Example `/status` response:

```json
{"on":true,"alarm":false,"ir":false,"rain":false,"tempHigh":false,"temp":29.4,"hum":63}
```

## How the alarm works

Once armed, each loop the system reads all three triggers:

```
alarm = obstacle OR rain OR (temperature >= TEMP_LIMIT_C)
```

If `alarm` is true → **red LED on, buzzer on**. Otherwise → **green LED on**. While disarmed, everything is off.

![Logic flowchart](images/flowchart.png)

*Replace `images/flowchart.png` with your flowchart of the alarm logic.*

## Configuration / tuning

All adjustable values live at the top of `src/main.cpp`:

| Constant | Default | Purpose |
|---|---|---|
| `TEMP_LIMIT_C` | `35.0` | Temperature (°C) that counts as too hot |
| `RAIN_THRESHOLD` | `1200` | Water-sensor reading (0–4095) above which it's "wet" |
| `IR_ACTIVE` | `LOW` | Logic level the IR sensor outputs on detection — flip if reversed |
| `DHT_INTERVAL` | `2000` | Minimum ms between DHT11 reads |
| `DEBOUNCE_MS` | `50` | Button debounce window |

**Tuning the water sensor:** raise `RAIN_THRESHOLD` toward 1800–2000 if it triggers too easily; lower it toward 800 if it won't fire until the pad is soaked. To find the exact value, print `analogRead(34)` and compare the dry vs. wet readings.

## Project structure

```
.
├── platformio.ini      # Board, framework, and library dependencies
├── src/
│   └── main.cpp         # All firmware logic
├── images/
│   ├── hardware.jpg     # Photo of the assembled circuit
│   ├── simulation.png   # Wokwi / Tinkercad screenshot
│   └── flowchart.png    # Alarm logic flowchart
└── README.md
```

## Troubleshooting

- **`DHT.h: No such file or directory`** — the dependencies haven't downloaded. Make sure `lib_deps` is under the `[env:...]` section of `platformio.ini`, save, and rebuild.
- **Upload fails with "Permission denied" on the serial port (Linux)** — add your user to the `dialout` group (`sudo usermod -aG dialout $USER`), install the PlatformIO udev rules, then log out and back in.
- **Board uploads to `/dev/ttyS0` or "No serial data received"** — that's not the ESP32 port. Confirm a `/dev/ttyUSB0` (CP2102) or `/dev/ttyACM0` device appears with `ls /dev/ttyUSB*`; if nothing shows up, try a data-capable USB cable.
- **WiFi won't connect** — check the password, confirm the network is 2.4GHz, and make sure the board is in range.
- **Water sensor always reads wet/dry** — the `S` pin is analog and must be read with `analogRead` on an ADC pin (GPIO34 here), not `digitalRead`. Then tune `RAIN_THRESHOLD`.

## License

Released under the MIT License. Feel free to use, modify, and share.
