# ESP32 HID Keyboard+Mouse Bridge

An ESP32 paired as a Bluetooth keyboard+mouse (**ESP32-KBM**, MAC `68:FE:71:8A:B5:A6`) that serves an HTTP control API at **http://192.168.1.64** (port 80). Sending it a click or keystroke makes the operating system deliver it to whatever window has focus — your real browser, with your real logins. No password to hold, no profile copy, no `--remote-debugging-port` (Chrome has blocked that on the default profile since v136).

> **First verified working:** 2026-11-03 — drove the whole LinkedIn and Facebook profile rebuild.

## Repository Structure

```
esp32-hid-kbm/
├── firmware/                    # ESP32 Arduino/PlatformIO firmware
│   ├── src/main.cpp             # Main firmware (BLE HID + HTTP API)
│   ├── platformio.ini           # PlatformIO config
│   └── README.md                # Firmware-specific docs
├── python/                      # Python control scripts
│   ├── screen_bot.py            # Main bot: screen + webcam + ESP32 control
│   ├── shot.py                  # Screenshot utility (PIL-based)
│   └── requirements.txt         # Python dependencies
├── scripts/                     # PowerShell helper scripts
│   ├── raise.ps1                # Bring Chrome window to foreground
│   ├── go.ps1                   # Navigate to URL in clipboard
│   ├── paste.ps1                # Paste clipboard to focused field
│   ├── keys.ps1                 # Send arbitrary keystrokes
│   ├── zoomout.ps1              # Ctrl+- x3 (zoom out)
│   ├── clip.txt                 # Clipboard content for paste.ps1
│   └── url.txt                  # Target URL for go.ps1
├── docs/                        # Documentation
│   ├── SETUP.md                 # Complete setup guide
│   ├── API.md                   # HTTP API reference
│   ├── COORDINATES.md           # Screen coordinate system
│   ├── TROUBLESHOOTING.md       # Common issues
│   └── RULES.md                 # Safety rules & procedures
└── .github/
    └── workflows/               # CI/CD (optional)
```

## Quick Start

### 1. Hardware Requirements
- ESP32 DevKit V1 (or compatible)
- USB cable for flashing
- Your ASUS computer with Bluetooth

### 2. Flash the Firmware
```bash
# Install PlatformIO
pip install platformio

# Navigate to firmware directory
cd firmware

# Compile and flash (adjust port as needed)
pio run --target upload --upload-port /dev/ttyUSB0  # Linux/macOS
pio run --target upload --upload-port COM3          # Windows
```

### 3. Pair via Bluetooth
1. Power on the ESP32 — it broadcasts as `ESP32-KBM`
2. Pair via Bluetooth on your ASUS computer
3. Note the IP address from serial monitor (default: `192.168.1.64`)

### 4. Install Python Dependencies
```bash
cd python
pip install -r requirements.txt
```

### 5. Verify Connection
```bash
curl -s "http://192.168.1.64/status"
# Should return: {"ok":true,"ip":"192.168.1.64","ble":true,"ptr":[0,0],"uptime":123}
```

### 6. Run the Bot
```bash
cd python
python screen_bot.py
```

## The Three Rules (Non-Negotiable)

1. **You must say the machine is yours to drive, and keep your hands off mouse and keyboard while it runs.** Two pointers fighting causes unintended clicks.

2. **Any action with an outside effect — submit, save, post, publish, delete, follow, message, pay, change a password — needs your explicit word for that single action.** Navigating, scrolling, reading, and typing into a field and showing you first are free. There is no undo on a physical HID click.

3. **Screenshot after every action.** Read the PNG before the next tap. Do not drive blind and do not assume a modal is still open or closed.

## Key Features

| Feature | Description |
|---------|-------------|
| **BLE HID Keyboard+Mouse** | Acts as real Bluetooth keyboard/mouse — OS has no idea it's automated |
| **HTTP API** | Control via simple REST calls (`/tap`, `/type`, `/key`, `/scroll`, `/batch`) |
| **Absolute Pointer** | Coordinates are absolute screen pixels (1920×1080 native) |
| **Human-like Movement** | Bezier curves, random speed variation via Python layer |
| **Screen Capture** | Full desktop screenshots via PIL (1:1 coordinate mapping) |
| **Webcam Integration** | Face detection, user presence checks |
| **Batch Commands** | Multiple actions in one HTTP call |
| **Auto-reconnect** | WiFi reconnects if dropped |

## HTTP API Endpoints

| Endpoint | Params | Notes |
|----------|--------|-------|
| `/status` | — | Returns `ok`, `ble`, `ptr`, `uptime` |
| `/tap` | `x,y` | Absolute move + left click (workhorse) |
| `/double_tap` | `x,y` | Double click |
| `/long_press` | `x,y,duration` | Hold click for duration ms |
| `/swipe` | `x1,y1,x2,y2,steps,delay` | Drag gesture |
| `/type` | `text` | ASCII only; shift auto for caps/symbols |
| `/key` | `key` | Special keys: enter, tab, backspace, escape, delete, arrows, space, home, end, pageup, pagedown |
| `/scroll` | `amount` | **Negative = down page**, positive = up |
| `/screen` | `width,height` | Tell board your resolution |
| `/batch` | `cmd` | `tap:100,200;wait:100;type:hello` |

Example:
```bash
curl -s "http://192.168.1.64/tap?x=700&y=400"
curl -s "http://192.168.1.64/scroll?amount=-70"
curl -s "http://192.168.1.64/key?key=escape"
```

## Screenshot Utility

```bash
# Full 1920x1080 (coords 1:1)
python python/shot.py

# 1280x720 (multiply coords by 1.5)
python python/shot.py --small

# Custom output
python python/shot.py --out my.png

# Sequential (shot_0.png, shot_1.png...)
python python/shot.py --seq
```

## PowerShell Helpers

```powershell
# Bring Chrome window to front (run BEFORE every SendKeys sequence)
powershell -File scripts/raise.ps1 "LinkedIn"

# Navigate to URL (edit scripts/url.txt first)
powershell -File scripts/go.ps1

# Paste clipboard to focused field (edit scripts/clip.txt first)
powershell -File scripts/paste.ps1

# Arbitrary keystrokes
powershell -File scripts/keys.ps1 "^a"      # Ctrl+A
powershell -File scripts/zoomout.ps1        # Ctrl+- x3
```

> **Important:** PowerShell gotcha — inline `-Command` strings with `$vars` or backticks get eaten by Git Bash. Always write a `.ps1` and run it with `-File`.

## Coordinate System

- Desktop resolution: **1920×1080** (native)
- Every endpoint takes **absolute screen pixels**
- Reading full-size screenshot: **1:1** mapping (proven by `/status` echoing `ptr`)
- Reading 1280×720 (`--small`): **multiply by 1.5**

## What This Does Not Do

- **No volume automation** — scheduled/bulk posting risks the account
- **No password entry** — surfaces reached through your existing session
- **No API for platforms** — this is for reading, one-off setup forms, and fields you review

## Safety & Cleanup

- Delete scratch screenshots when done: `%LOCALAPPDATA%\Temp\hid\` and Downloads
- Platform rules still apply (LinkedIn/Instagram/TikTok ToS)
- The account is the asset being sold — use responsibly

## License

Internal use only. Don't sell my toys.

---

Built by **Crab 🦀** — Bruce Autobots