# HTTP API Reference

Base URL: `http://192.168.1.64` (port 80)

All endpoints return JSON. All coordinates are **absolute screen pixels** (1920×1080 native).

---

## GET /status

**Check device status and BLE connection**

### Response
```json
{
  "ok": true,
  "ip": "192.168.1.64",
  "ble": true,
  "ptr": [700, 400],
  "uptime": 1234
}
```

| Field | Type | Description |
|-------|------|-------------|
| `ok` | bool | Always true if HTTP responds |
| `ip` | string | ESP32 IP address |
| `ble` | bool | **Critical** — `true` = HID link live, `false` = Bluetooth dropped |
| `ptr` | [int, int] | Where OS thinks pointer is (x, y) |
| `uptime` | int | Seconds since boot |

### Usage
```bash
curl -s "http://192.168.1.64/status"
```

> **Always check this first.** If `ble: false`, nothing will type or click.

---

## GET /tap

**Move pointer to absolute coordinates and left-click**

### Parameters
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `x` | int | Yes | Target X (0–1919) |
| `y` | int | Yes | Target Y (0–1079) |

### Response
```json
{"ok": true, "action": "tap"}
```

### Usage
```bash
curl -s "http://192.168.1.64/tap?x=700&y=400"
```

### Notes
- **Auto-homes** if not already homed (slams to 0,0 then moves)
- Updates internal `curX`, `curY`
- `/status` after tap should echo the same coordinates

---

## GET /double_tap

**Move to coordinates and double-click**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `x` | int | Yes |
| `y` | int | Yes |

### Response
```json
{"ok": true, "action": "double_tap"}
```

### Usage
```bash
curl -s "http://192.168.1.64/double_tap?x=700&y=400"
```

---

## GET /long_press

**Move to coordinates, hold left button for duration**

### Parameters
| Param | Type | Required | Default |
|-------|------|----------|---------|
| `x` | int | Yes | — |
| `y` | int | Yes | — |
| `duration` | int | No | 1000 (ms) |

### Response
```json
{"ok": true, "action": "long_press"}
```

### Usage
```bash
curl -s "http://192.168.1.64/long_press?x=700&y=400&duration=2000"
```

---

## GET /swipe

**Drag from (x1,y1) to (x2,y2)**

### Parameters
| Param | Type | Required | Default |
|-------|------|----------|---------|
| `x1` | int | Yes | — |
| `y1` | int | Yes | — |
| `x2` | int | Yes | — |
| `y2` | int | Yes | — |
| `steps` | int | No | 20 |
| `delay` | int | No | 10 (ms) |

### Response
```json
{"ok": true, "action": "swipe"}
```

### Usage
```bash
curl -s "http://192.168.1.64/swipe?x1=100&y1=100&x2=500&y2=500&steps=30&delay=15"
```

---

## GET /type

**Type a text string (ASCII only)**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `text` | string | Yes |

### Response
```json
{"ok": true, "action": "type"}
```

### Usage
```bash
curl -s "http://192.168.1.64/type?text=hello%20world"
```

### Supported Characters
- `a-z`, `A-Z` (shift auto-applied)
- `0-9`
- Space
- Symbols: `!@#$%^&*()_+{}:"<>?|~` (shift auto-applied)
- **NOT supported**: Unicode, emoji, non-ASCII

### ⚠️ Warning: Long Text Through `/type`
- Slow (one HTTP request per character effectively)
- Mangled punctuation: `facebook.com/bruce` → `facebook com-bruce`
- Dots, slashes, dashes often dropped

**Use clipboard + paste instead** (see PowerShell scripts).

---

## GET /key

**Press a special key**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `key` | string | Yes |

### Supported Keys
| Key | HID Code | Description |
|-----|----------|-------------|
| `enter` | 0x28 | Return/Enter |
| `tab` | 0x2B | Tab |
| `backspace` | 0x2A | Backspace |
| `escape` | 0x29 | Escape |
| `delete` | 0x4C | Delete (forward) |
| `up` | 0x52 | Arrow Up |
| `down` | 0x51 | Arrow Down |
| `left` | 0x50 | Arrow Left |
| `right` | 0x4F | Arrow Right |
| `space` | 0x2C | Spacebar |
| `home` | 0x4A | Home |
| `end` | 0x4D | End |
| `pageup` | 0x4B | Page Up |
| `pagedown` | 0x4E | Page Down |
| *any single char* | — | Falls back to `/type` |

### Response
```json
{"ok": true, "action": "key"}
```

### Usage
```bash
curl -s "http://192.168.1.64/key?key=enter"
curl -s "http://192.168.1.64/key?key=tab"
curl -s "http://192.168.1.64/key?key=escape"
```

---

## GET /move

**Move pointer by relative delta (no click)**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `dx` | int | Yes |
| `dy` | int | Yes |

### Response
```json
{"ok": true, "action": "move"}
```

### Usage
```bash
curl -s "http://192.168.1.64/move?dx=100&dy=50"
```

> **Note:** `/move` is a no-op on this firmware — it updates internal coords but doesn't actually move the pointer on screen. Only `/tap` (which homes and moves absolutely) repositions the visible pointer.

---

## GET /scroll

**Scroll wheel**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `amount` | int | Yes |

### Response
```json
{"ok": true, "action": "scroll"}
```

### Usage
```bash
# Scroll DOWN (negative = down page)
curl -s "http://192.168.1.64/scroll?amount=-70"

# Scroll UP
curl -s "http://192.168.1.64/scroll?amount=70"
```

### Scroll Behavior
- **Nonlinear & amplified**: ±140 = big jump, ±70 = fine-grained
- Overshooting to footer happens constantly
- Correct with small `+70` steps back up
- **Only scrolls page under pointer** — if pointer over sidebar/ad/dropdown, appears to do nothing
- **Tap neutral gutter first**: native `(20, 500)` works on LinkedIn, Facebook, Instagram

---

## GET /screen

**Tell the ESP32 your screen resolution**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `width` | int | No |
| `height` | int | No |

### Response
```json
{"ok": true, "screen": "1920x1080"}
```

### Usage
```bash
curl -s "http://192.168.1.64/screen?width=1920&height=1080"
```

> Python `ScreenBot` calls this automatically on startup.

---

## GET /batch

**Execute multiple commands in one HTTP call**

### Parameters
| Param | Type | Required |
|-------|------|----------|
| `cmd` | string | Yes |

### Command Syntax
```
action:param1,param2;action:param1,param2;...
```

### Supported Actions
| Action | Params | Example |
|--------|--------|---------|
| `tap` | `x,y` | `tap:700,400` |
| `wait` | `ms` | `wait:100` |
| `type` | `text` | `type:hello` |
| `key` | `key` | `key:enter` |
| `swipe` | `x1,y1,x2,y2` | `swipe:0,0,100,100` |

### Response
```json
{"ok": true, "action": "batch"}
```

### Usage
```bash
curl -s "http://192.168.1.64/batch?cmd=tap:700,400;wait:100;type:hello;key:enter"
```

---

## Error Responses

All endpoints return 200 OK with JSON on success. On error (invalid params, etc.):
```json
{"ok": false, "error": "description"}
```

HTTP 500 if firmware crashes (unlikely).

---

## Rate Limits & Timing

| Operation | Typical Time | Notes |
|-----------|--------------|-------|
| `/tap` | 50–100ms | Includes homing if needed |
| `/type` (per char) | ~30ms | Slow for long strings |
| `/key` | 25–40ms | |
| `/scroll` | 20–50ms per chunk | Chunked at ±127 |
| `/batch` | Sum of parts | Sequential, 10ms between |

---

## Python Wrapper Example

```python
import requests

ESP32 = "http://192.168.1.64"

def tap(x, y):
    requests.get(f"{ESP32}/tap", params={"x": x, "y": y}, timeout=3)

def type_text(text):
    requests.get(f"{ESP32}/type", params={"text": text}, timeout=5)

def key(name):
    requests.get(f"{ESP32}/key", params={"key": name}, timeout=3)

def scroll(amount):
    requests.get(f"{ESP32}/scroll", params={"amount": amount}, timeout=3)

def status():
    return requests.get(f"{ESP32}/status", timeout=3).json()

# Usage
if status()["ble"]:
    tap(700, 400)
    type_text("hello")
    key("enter")
    scroll(-70)
```

---

## PowerShell Wrapper Example

```powershell
$ESP32 = "http://192.168.1.64"

function Tap($x, $y) {
    Invoke-RestMethod "$ESP32/tap?x=$x&y=$y"
}

function TypeText($text) {
    Invoke-RestMethod "$ESP32/type?text=$([uri]::EscapeDataString($text))"
}

function Key($name) {
    Invoke-RestMethod "$ESP32/key?key=$name"
}

function Scroll($amount) {
    Invoke-RestMethod "$ESP32/scroll?amount=$amount"
}

function Status {
    Invoke-RestMethod "$ESP32/status"
}

# Usage
if ((Status).ble) {
    Tap 700 400
    TypeText "hello"
    Key "enter"
    Scroll -70
}
```