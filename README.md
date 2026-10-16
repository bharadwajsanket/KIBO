# Kibo ✨

*A tiny desktop companion with expressive OLED eyes, capacitive touch interaction, and a persistent mood engine running entirely on an ESP32-C3.*

---

## Overview

**Kibo** is an open-source, interactive desktop companion. Built around a compact 128×64 monochrome OLED display, a capacitive touch sensor, and an ESP32-C3 Super Mini microcontroller, Kibo blends expressive facial animations with practical desk companion utilities:

- **Expressive Eye Physics:** Spring-mass-damper physics engine simulating saccades, dynamic pupil dilation, blinking, breathing cycles, and emotion states.
- **Dynamic Mood & Personality Engine:** Tracks energy, curiosity, contentment, and irritability. Remembers familiarity, total touches, and mood history across restarts using non-volatile storage (`Preferences`).
- **Interactive Touch Gestures:** Single tap, multi-tap bursts, long presses, and continuous "petting" detection with squish reactions and floating particles.
- **Carousel of Desk Utilities:**
  - **Face Page:** Kibo's animated resting face with micro-gestures (yawns, winks, looks around, stretches).
  - **Local Clock & World Clock:** NTP-synchronized digital or analog clock with support for configurable secondary timezones.
  - **Live Weather & Multi-Day Forecast:** Real-time temperature, condition icons (sun, clouds, rain, snow, storm), humidity, and forecasts via OpenWeatherMap. Supports multiple saved cities.
  - **Tiny Arcade:** Built-in mini-games played directly via the touch sensor — *Flappy Kibo* and *Blink Reflex*.
- **Onboard Captive Portal & Web UI:** On first boot (or when Wi-Fi is unavailable), Kibo launches an Access Point (`Kibo-Setup`) and renders a QR code on the OLED. Connect via any phone or browser to configure Wi-Fi, timezones, weather cities, API keys, and birthdays.
- **3D Printable Enclosure:** Includes a custom STL model ready for 3D printing.

---

## Hardware Architecture

### Bill of Materials

| Component | Specification | Notes |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32-C3 Super Mini (HW-466AB) | RISC-V core with 2.4 GHz Wi-Fi & BLE |
| **Display** | 0.96" SSD1306 I²C OLED (128×64) | Monochrome blue or white, I²C address `0x3C` |
| **Input Sensor** | TTP223 Capacitive Touch Switch | Set to active HIGH / momentary mode |
| **Power** | 5V via USB-C | Powered directly through ESP32-C3 USB port |
| **Case** | Custom 3D-printed enclosure | [Kibo.stl](file:///Users/sanketbharadwaj/Developer/Kibo/Kibo.stl) |

### Wiring Diagram

```text
       ESP32-C3 Super Mini
      ┌───────────────────┐
      │                   │
      │        3V3 ───────┼───────► VCC  (OLED & TTP223)
      │        GND ───────┼───────► GND  (OLED & TTP223)
      │                   │
      │     GPIO 0 (SDA) ─┼───────► SDA  (SSD1306 OLED)
      │     GPIO 1 (SCL) ─┼───────► SCL  (SSD1306 OLED)
      │                   │
      │     GPIO 4 ───────┼───────► I/O  (TTP223 Touch Out)
      │                   │
      └───────────────────┘
```

> **Note:** The firmware also falls back to check alternate I²C pins (GPIO 8 as SDA, GPIO 9 as SCL) if your board variant differs. Always confirm the pin labeling on your specific ESP32-C3 breakout.

---

## Touch Controls & Gestures

| Gesture | Where | Action |
| :--- | :--- | :--- |
| **Single Tap** | Face Page | Cycles to the next page (Clock → Weather → Forecast → Arcade → Face) |
| **Double Tap** | Face Page | Triggers an interactive reaction / emote |
| **Triple Tap** | Face Page | Puts Kibo to sleep / wake up |
| **Long Press / Hold (>600ms)** | Face Page | "Petting" mode — eyes squish happily, spawns heart particles |
| **Rapid Tapping (Burst)** | Face Page | Overstimulates Kibo — triggers dizzy or annoyed state |
| **Long Press (>800ms)** | Clock Page | Toggles between Local Clock and World Clock |
| **Long Press (>800ms)** | Weather / Forecast | Cycles through configured multi-city weather list |
| **Single Tap** | Arcade Menu | Starts selected mini-game / jumps or taps during gameplay |
| **Double Tap** | Arcade Menu | Toggles between *Flappy Kibo* and *Blink Reflex* |
| **Long Press (>1200ms)** | In-Game | Exits game back to Arcade menu with a reaction |

---

## Software & Firmware Setup

### Prerequisites

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended) or [PlatformIO](https://platformio.org/).
2. Add ESP32 board support in Arduino IDE:
   - Navigate to **Preferences** → **Additional Boards Manager URLs**:
     ```text
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Open **Boards Manager** and install **esp32** by Espressif Systems.
3. Install required libraries via the Arduino Library Manager:
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `Arduino_JSON`

### Flashing the Firmware

1. Clone or download this repository:
   ```bash
   git clone https://github.com/bharadwajsanket/KIBO.git
   ```
2. Ensure the following files remain in the same folder named `Kibo`:
   - [Kibo.ino](file:///Users/sanketbharadwaj/Developer/Kibo/Kibo.ino)
   - [qrcode.c](file:///Users/sanketbharadwaj/Developer/Kibo/qrcode.c)
   - [qrcode.h](file:///Users/sanketbharadwaj/Developer/Kibo/qrcode.h)
3. Open `Kibo.ino` in Arduino IDE.
4. Select board: **ESP32C3 Dev Module** (or your specific ESP32-C3 Super Mini profile).
5. Set flash settings (typical defaults):
   - **Flash Mode**: DIO / QIO
   - **Flash Size**: 4MB
   - **USB CDC On Boot**: Enabled (if using hardware USB-CDC)
6. Connect your ESP32-C3 via USB, select the correct COM/Serial port, and click **Upload**.

---

## Initial Configuration & Web Portal

1. On first power-on (or whenever configured Wi-Fi is unreachable), Kibo starts an access point:
   - **SSID:** `Kibo-Setup`
   - **Default Password:** `12345678`
2. Kibo will display a scannable **QR code** on her OLED screen for quick phone connection.
3. Once connected to `Kibo-Setup`, open your browser and navigate to:
   ```text
   http://192.168.4.1
   ```
4. Configure your preferences:
   - **Wi-Fi Settings:** Network SSID and Password.
   - **Timezone:** POSIX timezone string (e.g. `UTC0`, `EST5EDT`, `IST-5:30`) and optional World Clock offset.
   - **Weather (Optional):** Enter your free [OpenWeatherMap](https://openweathermap.org/api) API key and city/country codes (format: `London|GB; Tokyo|JP; New York|US`).
   - **Birthdays & Dates:** Store special dates for custom celebration animations.
   - **Security:** Update the AP setup password.
5. Save and restart Kibo. Kibo connects to your home network, syncs time via NTP, and pulls weather updates in the background.

---

## Repository Structure

```text
Kibo/
├── Kibo.ino          # Complete firmware: state machine, graphics engine, web portal
├── qrcode.c          # Lightweight QR code generator engine
├── qrcode.h          # Header definitions for QR code generation
├── Kibo.stl          # 3D printable desktop enclosure
├── README.md         # Documentation & setup guide
└── LICENSE           # MIT License
```

---

## 3D Printing the Case

- File: [Kibo.stl](file:///Users/sanketbharadwaj/Developer/Kibo/Kibo.stl)
- Recommended print settings:
  - **Material:** PLA or PETG
  - **Layer Height:** 0.16mm – 0.20mm
  - **Infill:** 15% – 20%
  - **Supports:** As needed depending on print orientation

---

## Contributing

Contributions, bug reports, and ideas are welcome! Feel free to:
1. Fork the repository.
2. Create your feature branch (`git checkout -b feature/cool-animation`).
3. Commit your changes (`git commit -m 'Add new micro-gesture'`).
4. Push to the branch (`git push origin feature/cool-animation`).
5. Open a Pull Request.

---

## License & Credits

- **Project:** Licensed under the [MIT License](file:///Users/sanketbharadwaj/Developer/Kibo/LICENSE) — Copyright (c) 2025 Sanket Bharadwaj.
- **QR Code Engine:** `qrcode.c` / `qrcode.h` are copyright (c) Richard Moore and licensed under the MIT License (inspired by Nayuki's QR code generator).
