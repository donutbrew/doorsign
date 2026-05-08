# ESP32 E‑Paper Door Sign (BLE + Battery Mode)

A Bluetooth-controlled e‑paper door sign powered by an ESP32‑C3 with optional battery-optimized deep sleep mode.

---

## ✨ Features

### 📟 Display
- Tri-color e‑paper (black / white / red)
- Rich text formatting:
  - `[big]`, `[med]`, `[small]`
  - `{red text}`
  - `{{red boxed text}}`
  - `\n` line breaks
- Auto layout:
  - centers text
  - wraps intelligently
  - auto-shrinks if too large

---

### 🔵 BLE Control
- Custom BLE service (UART-style)
- Send messages from:
  - Web app (Web Bluetooth / Bluefy)
  - nRF Connect or similar apps
- Commands:
  - `1–N` → recall preset
  - `SETn:<message>` → save preset
  - `RESETPRESETS` → clear saved presets

---

### 🧠 Smart Metadata (NEW)
Device advertises:
- available icons
- preset labels

Used by the web app to dynamically build UI.

Format:
```
Icons: available|meeting|no|out|soon|remote|cranky
Presets: 1|Available|2|Meeting|...
```

---

### 🧾 Presets
- Stored in flash (Preferences / NVS)
- Configurable in `presets.h`
- Now supports arbitrary number of slots (`NUM_PRESETS`)
- Updated dynamically over BLE

---

### 🎨 Icons
- Optional left-side icon area
- Vector + bitmap support
- Example icons:
  - available (smiley)
  - meeting (phone)
  - no / do-not-disturb
  - out of office
  - telework (wifi)
  - cranky (angry face)

---

### 🖲 Button Control
- Single button (GPIO21)
- Cycles presets
- Debounced and delayed commit

---

### 🔋 Power Modes (NEW)

Controlled at compile time:

```cpp
#define ENABLE_DEEP_SLEEP false
```

---

#### 🔌 Always-On Mode (default)
- BLE always advertising
- instant connection
- higher power draw (~30–60mA)

---

#### 🔋 Battery Mode
```cpp
#define ENABLE_DEEP_SLEEP true
```

Behavior:
- ESP sleeps most of the time
- press button → wake device
- BLE available for ~60 seconds
- returns to deep sleep

Benefits:
- weeks to months battery life
- screen retains image while powered off

---

### ⚡ Power Recommendations

#### Best:
- USB power (always-on mode)

#### Battery:
- 3×AA or 3×AAA (NiMH recommended)
- add 100–470µF capacitor across 3.3V/GND

---

## 📱 Web App

- Hosted (e.g., GitHub Pages)
- Uses Web Bluetooth
- Features:
  - dynamic presets/icons from device
  - message editor + formatting tools
  - history (last 20 messages)
  - save/recall presets
  - backup/restore

---

## 🔧 Wiring

### Display (example)
| Pin | ESP32 |
|-----|------|
| CS  | 7 |
| DC  | 2 |
| RST | 1 |
| BUSY| 10 |
| SCK | 4 |
| MOSI| 3 |

---

### Button
```
GPIO21 ---- button ---- GND
```

Uses internal pull-up.

---

## 🧪 Development Notes

- Uses GxEPD2 library
- Uses BLEDevice (ESP-IDF/Arduino)
- Avoid unnecessary redraws to reduce flashing
- Metadata updates only occur on connect or change (low power impact)

---

## 🚀 Future Ideas

- Full bitmap icon set
- NFC wake/trigger
- ultra-low-power timed advertising
- OTA updates

---

## 🏁 Summary

This project is now:

- 🔵 BLE-controlled
- 🧠 self-describing (metadata)
- 🔋 battery-capable
- 🎨 visually polished
- 📱 app-integrated

---

