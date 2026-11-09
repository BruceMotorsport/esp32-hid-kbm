# ESP32 Firmware — BLE HID Keyboard + Mouse

## Overview
This firmware runs on an ESP32 DevKit V1 and implements a **Bluetooth Low Energy (BLE) HID device** that appears to the host as a standard keyboard and mouse. It's controlled via:
- **HTTP API** (port 80) — main control interface
- **Serial commands** — fallback when WiFi is down

## Key Design Decisions

### Raw GATT Service (No NimBLEHIDDevice)
The NimBLE `NimBLEHIDDevice` class has a known bug where it doesn't properly register the HID service with the stack. This firmware **manually creates the entire GATT service** with correct handles, descriptors, and report references.

### Absolute Pointer Emulation
The mouse report uses **signed 8-bit deltas** (X, Y, Wheel). Since BLE HID mice only send relative movement, the firmware implements absolute positioning by:
1. **Homing**: Sending -127,-127 repeatedly (10×) to slam pointer to (0,0)
2. **Delta moves**: Sending relative deltas to reach target coordinates
3. **State tracking**: Maintaining `curX`, `curY` internally

### Security & Bonding
```cpp
NimBLEDevice::setSecurityAuth(true, true, true);  // MITM, SC, Bonding
NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
```
This enables **persistent bonding** — the ESP32 remembers paired hosts across reboots.

### Connection Parameters
```cpp
bleServer->setConnParams(6, 12, 0, 300);  // 7.5ms–15ms interval, 0 latency, 3s timeout
```
Optimized for low-latency HID (keyboard/mouse feel responsive).

## File Structure
```
firmware/
├── src/main.cpp      # Complete firmware (single file)
├── platformio.ini    # PlatformIO config
└── README.md         # This file
```

## Building & Flashing

### Prerequisites
- PlatformIO Core (`pip install platformio`)
- ESP32 DevKit V1 connected via USB

### Commands
```bash
cd firmware

# Build only
pio run

# Build and flash (auto-detect port)
pio run --target upload

# Flash with specific port
pio run --target upload --upload-port COM3       # Windows
pio run --target upload --upload-port /dev/ttyUSB0  # Linux/macOS

# Monitor serial (115200 baud)
pio device monitor

# Clean build
pio run --target clean
```

## Configuration

### WiFi Credentials (in `src/main.cpp`)
```cpp
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";
```
> **Change these before flashing!** The defaults are for Bruce's network.

### Device Name
```cpp
NimBLEDevice::init("ESP32-KBM");  // Bluetooth name
```

### Screen Dimensions (default)
```cpp
int screenW = 1080, screenH = 2400;  // Can be overridden via /screen endpoint
```

## GATT Service Structure

```
HID Service (0x1812)
├── HID Information (0x2A4C) — READ
├── Report Map (0x2A4B) — READ
├── Protocol Mode (0x2A4E) — READ/WRITE
├── Report (0x2A4D) — Keyboard Input — READ/NOTIFY + Report Reference (ID=1, Input)
├── Report (0x2A4D) — Mouse Input — READ/NOTIFY + Report Reference (ID=2, Input)
└── Battery Service (0x180F)
    └── Battery Level (0x2A19) — READ/NOTIFY
```

### Report Map (Keyboard ID=1, Mouse ID=2)
- **Keyboard**: 8 modifier bits + 1 padding + 6 key codes
- **Mouse**: 3 buttons + 5 padding + X (int8) + Y (int8) + Wheel (int8)

## HTTP Endpoints

| Endpoint | Method | Params | Response |
|----------|--------|--------|----------|
| `/status` | GET | — | `{ok,ip,ble,ptr:[x,y],uptime}` |
| `/tap` | GET | `x,y` | `{ok,action:"tap"}` |
| `/double_tap` | GET | `x,y` | `{ok,action:"double_tap"}` |
| `/long_press` | GET | `x,y,duration` | `{ok,action:"long_press"}` |
| `/swipe` | GET | `x1,y1,x2,y2,steps,delay` | `{ok,action:"swipe"}` |
| `/type` | GET | `text` | `{ok,action:"type"}` |
| `/key` | GET | `key` | `{ok,action:"key"}` |
| `/move` | GET | `dx,dy` | `{ok,action:"move"}` |
| `/scroll` | GET | `amount` | `{ok,action:"scroll"}` |
| `/screen` | GET | `width,height` | `{ok,screen:"WxH"}` |
| `/batch` | GET | `cmd` | `{ok,action:"batch"}` |

### Batch Command Syntax
```
tap:100,200;wait:100;type:hello;key:enter;swipe:0,0,100,100
```

## Serial Commands (115200 baud)
```
type HELLO WORLD
key enter
tap 500,300
move 100,50
swipe 0,0,200,200
home
status
diag
help
```

## Debugging

### Serial Output on Boot
```
=== ESP32 BLE HID v3 (raw GATT) ===
[1] NimBLE init done
[2] Security set
[3] Server created
[4] HID service created, uuid=0x1812
...
[12] BLE HID v3 up — raw GATT, no NimBLEHIDDevice
WiFi IP: 192.168.1.64
HTTP :80 up
Serial: 'type HELLO' works once paired. Waiting for host...
```

### Key Diagnostics
- `ble=true` in `/status` → HID link is live
- `ble=false` → Bluetooth dropped (re-pair needed)
- `ptr:[x,y]` → where OS thinks pointer is
- `diag` command → shows service registration status

### Common Issues
| Symptom | Cause | Fix |
|---------|-------|-----|
| `/status` returns `ble:false` | Host unpaired / Bluetooth off | Re-pair on host |
| Tap doesn't register | Pointer not homed | Send `/tap` (auto-homes) or `home` serial cmd |
| WiFi fails | Wrong credentials | Update `WIFI_SSID`/`WIFI_PASS` and reflash |
| Keys mangled | `/type` with special chars | Use clipboard + paste (see Python layer) |

## Memory & Performance
- **Flash**: ~350KB (of ~1.2MB available)
- **RAM**: ~40KB (of ~320KB available)
- **Loop latency**: ~2ms (HTTP + serial polling)

## Updating Firmware
1. Modify `src/main.cpp`
2. `pio run --target upload`
3. ESP32 reboots, re-pairs automatically (bonding persists)

## License
Internal use only.