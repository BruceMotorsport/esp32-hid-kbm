# Complete Setup Guide

## Hardware Requirements

| Component | Specification |
|-----------|---------------|
| ESP32 Board | ESP32 DevKit V1 (or compatible ESP32-WROOM-32) |
| USB Cable | Micro-USB data cable (for flashing) |
| Host Computer | Your ASUS computer with Bluetooth 4.0+ |
| Power | USB power (computer port or 5V adapter) |

## Software Prerequisites

### On Your ASUS Computer

1. **Python 3.8+**
   ```bash
   python --version
   # Should show 3.8 or higher
   ```

2. **PlatformIO Core**
   ```bash
   pip install platformio
   # Verify: pio --version
   ```

3. **Git** (for cloning this repo)
   ```bash
   git --version
   ```

4. **Bluetooth Stack** — Windows 10/11 has this built-in

### On the ESP32 (flashed via PlatformIO)

The firmware includes:
- NimBLE-Arduino library (BLE stack)
- Arduino WiFi/WebServer libraries
- Custom HID GATT service implementation

## Step-by-Step Setup

### 1. Clone/Extract This Repository
```bash
# If you have git:
git clone <this-repo-url> esp32-hid-kbm
cd esp32-hid-kbm

# Or extract the zip to a folder
```

### 2. Configure WiFi Credentials
Edit `firmware/src/main.cpp`:
```cpp
// ===== CONFIG =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```
> **Important:** The ESP32 needs WiFi to serve the HTTP API. Use a 2.4GHz network (ESP32 doesn't support 5GHz).

### 3. Flash the Firmware
```bash
cd firmware

# Connect ESP32 via USB
# Find your port:
pio device list

# Flash (adjust port):
pio run --target upload --upload-port COM3        # Windows
pio run --target upload --upload-port /dev/ttyUSB0  # Linux/macOS

# Monitor to verify:
pio device monitor --baud 115200
```

You should see:
```
=== ESP32 BLE HID v3 (raw GATT) ===
[1] NimBLE init done
[2] Security set
...
[12] BLE HID v3 up — raw GATT, no NimBLEHIDDevice
WiFi IP: 192.168.1.XXX
HTTP :80 up
Serial: 'type HELLO' works once paired. Waiting for host...
```

### 4. Pair via Bluetooth

**On your ASUS computer:**
1. Open **Settings → Bluetooth & devices**
2. Click **Add device** → **Bluetooth**
3. Wait for **ESP32-KBM** to appear
4. Click to pair — it should show "Connected" as Keyboard & Mouse

**Verify in Device Manager:**
- Keyboards → "HID Keyboard Device" (or similar)
- Mice → "HID-compliant mouse"

### 5. Find the ESP32 IP Address

**Option A: Serial Monitor**
```bash
pio device monitor --baud 115200
# Look for: "WiFi IP: 192.168.1.64"
```

**Option B: Router Admin Page**
- Check DHCP client list for "ESP32-KBM"

**Option C: Network Scan**
```bash
# Windows
arp -a | findstr "68-fe-71-8a-b5-a6"

# Or use Advanced IP Scanner, Angry IP Scanner, etc.
```

### 6. Install Python Dependencies
```bash
cd python
pip install -r requirements.txt
```

### 7. Test the Connection
```bash
# From python directory
curl -s "http://192.168.1.64/status"
# Or use Python:
python -c "import requests; print(requests.get('http://192.168.1.64/status').json())"
```

Expected response:
```json
{
  "ok": true,
  "ip": "192.168.1.64",
  "ble": true,
  "ptr": [0, 0],
  "uptime": 42
}
```

> **Critical:** `"ble": true` means the HID link is live. If `false`, the Bluetooth connection dropped — re-pair.

### 8. Take a Test Screenshot
```bash
python shot.py
# Should print path like: C:\Users\YOU\AppData\Local\Temp\hid\shot.png
# And: native=1920x1080  saved=1920x1080  coords 1:1
```

### 9. Run the Screen Bot
```bash
python screen_bot.py
```

You'll see an interactive prompt:
```
=== Screen Bot v2 Ready ===
Commands:
  screenshot       - Capture screen
  webcam           - Capture webcam
  user             - Check if user visible
  tap x,y          - Tap at coordinates
  type text        - Type text
  find template    - Find image on screen
  grid template    - Grid search
  look             - See + describe
  quit             - Exit
>
```

Try:
```
> tap 500,500
> screenshot
> quit
```

## Configuration for Your ASUS

### Screen Resolution
The firmware defaults to `1080x2400` but your ASUS is likely `1920x1080`. The Python bot auto-detects and sends the correct size via `/screen` endpoint on startup.

To manually set:
```bash
curl "http://192.168.1.64/screen?width=1920&height=1080"
```

### Webcam Index
If you have multiple cameras, edit `screen_bot.py`:
```python
bot = ScreenBot("192.168.1.64", webcam_index=1)  # 0=default, 1=second, etc.
```

### PowerShell Script Paths
The scripts assume they're run from the repo root. If you move them, update paths in:
- `scripts/raise.ps1` — no external dependencies
- `scripts/go.ps1` — reads `scripts/url.txt`
- `scripts/paste.ps1` — reads `scripts/clip.txt`
- `scripts/keys.ps1` — takes key as argument
- `scripts/zoomout.ps1` — no dependencies

## Verification Checklist

- [ ] ESP32 flashes without errors
- [ ] Serial monitor shows WiFi IP
- [ ] Bluetooth pairs as "ESP32-KBM" (Keyboard + Mouse)
- [ ] `/status` returns `ble: true`
- [ ] `shot.py` captures 1920x1080 screenshot
- [ ] `tap 100,100` moves mouse and clicks
- [ ] `type hello` types in focused window
- [ ] `key enter` presses Enter
- [ ] `scroll -70` scrolls down

## Troubleshooting Quick Links

| Issue | See |
|-------|-----|
| WiFi won't connect | [TROUBLESHOOTING.md#wifi] |
| Bluetooth won't pair | [TROUBLESHOOTING.md#ble-pair] |
| `ble: false` in status | [TROUBLESHOOTING.md#ble-false] |
| Tap doesn't click | [TROUBLESHOOTING.md#tap-fail] |
| Coordinates wrong | [COORDINATES.md] |
| PowerShell scripts fail | [TROUBLESHOOTING.md#powershell] |

## Next Steps

- Read [API.md](API.md) for full HTTP reference
- Read [COORDINATES.md](COORDINATES.md) for coordinate system
- Read [RULES.md](RULES.md) for safety procedures
- Check [docs/](.) for more guides