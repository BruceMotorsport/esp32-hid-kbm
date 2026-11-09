# Troubleshooting Guide

## Quick Diagnostic

```bash
# 1. Check status
curl -s "http://192.168.1.64/status"

# 2. Check WiFi
pio device monitor --baud 115200
# Look for "WiFi IP:" line

# 3. Check Bluetooth
# Windows: Settings → Bluetooth & devices → ESP32-KBM
```

---

## WiFi Issues

### WiFi FAILED on boot
```
WiFi FAILED — serial commands still work: type 'help'
```

**Causes:**
- Wrong SSID/password in `firmware/src/main.cpp`
- 5GHz network (ESP32 only supports 2.4GHz)
- Router MAC filtering
- Too far from AP

**Fixes:**
1. Edit `firmware/src/main.cpp`:
   ```cpp
   const char* WIFI_SSID = "YOUR_2GHZ_SSID";
   const char* WIFI_PASS = "YOUR_PASSWORD";
   ```
2. Reflash: `pio run --target upload`
3. Ensure 2.4GHz band enabled on router

### WiFi drops after working
- Firmware auto-reconnects every 20s if disconnected
- Check serial monitor for `WiFi FAILED` → `WiFi IP:` cycles
- Weak signal? Move ESP32 closer to router

---

## Bluetooth (BLE) Issues

### `ble: false` in /status
```json
{"ok":true,"ip":"192.168.1.64","ble":false,"ptr":[0,0],"uptime":123}
```

**Causes:**
- Host computer Bluetooth off
- ESP32 not paired / pairing removed
- Bluetooth stack glitch on host
- ESP32 rebooted (loses connection, not bonding)

**Fixes:**
1. **Windows**: Settings → Bluetooth & devices → ESP32-KBM → Connect
2. If "Connected" but `ble: false`: Remove device → Re-pair
3. Restart Bluetooth on host: Disable/Enable in Device Manager
4. Power cycle ESP32 (unplug/replug USB)

### Won't pair / pairing fails
1. Remove old "ESP32-KBM" from Bluetooth devices
2. Put ESP32 in pairing mode (power cycle)
3. On host: Add Bluetooth device → wait for ESP32-KBM
4. If PIN requested: try `0000` or `1234` (usually no PIN for HID)

### Paired but not showing as Keyboard+Mouse
**Device Manager check:**
- Keyboards → Should show "HID Keyboard Device"
- Mice → Should show "HID-compliant mouse"

If missing:
1. Remove device in Bluetooth settings
2. Re-pair
3. Check Windows Update for HID drivers

---

## Tap/Click Issues

### Tap does nothing (no click registered)
**Checklist:**
1. `ble: true` in `/status`? → If false, see BLE issues
2. Target window focused? → Use `raise.ps1` first
3. Coordinates correct? → Verify with full-size screenshot
4. `/status` after tap shows `ptr` at target?

**Common cause**: Window not in foreground. Keystrokes/clicks go to whatever has focus.

**Fix**: Always run `raise.ps1 "Window Title"` before actions.

### Tap lands at wrong position
**Causes:**
| Symptom | Cause | Fix |
|---------|-------|-----|
| Consistently offset | Used `--small` coords without ×1.5 | Multiply by 1.5 |
| Random offset | Not homed | First tap auto-homes; wait for it |
| Scaled wrong | Display scaling ≠ 100% | Set Windows scaling to 100% |
| Multi-monitor | Coords on secondary monitor | Keep targets on primary |

### Double-tap acts as single tap
- Use `/double_tap` endpoint (has 80ms delay built in)
- Don't send two `/tap` calls rapidly

---

## Typing Issues

### Special characters mangled
```
/type?text=facebook.com/bruce → types "facebook com-bruce"
```

**Cause**: `/type` sends raw HID keycodes; punctuation mapping is imperfect.

**Fix**: Use clipboard + paste:
```powershell
# Edit scripts/clip.txt with your text
powershell -File scripts/paste.ps1
```

### Long text is slow
- `/type` sends ~30ms per character
- 100 chars = 3+ seconds

**Fix**: Clipboard paste is instant.

### Non-ASCII not supported
- Unicode, emoji, accented chars → won't work
- **Fix**: Don't use them, or paste from clipboard

---

## Scroll Issues

### Scroll does nothing
**Cause**: Pointer over non-scrollable element (sidebar, ad, dropdown)

**Fix**: Tap neutral gutter first:
```bash
curl "http://192.168.1.64/tap?x=20&y=500"  # Neutral left gutter
curl "http://192.168.1.64/scroll?amount=-70"
```

### Scroll overshoots to footer
- Scroll is nonlinear: ±140 = big jump
- **Fix**: Use small steps: `-70`, wait, `-70`, etc.
- Correct overshoot: `+70` (scroll up slightly)

---

## PowerShell Script Issues

### "Running scripts is disabled"
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### "Command not found" / path errors
- Run from repo root: `cd C:\path\to\esp32-hid-kbm-export`
- Use full paths: `powershell -File "C:\path\scripts\raise.ps1" "LinkedIn"`

### Variables/backticks eaten in Git Bash
**Wrong:**
```bash
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $ws.SendKeys('^a')"
```

**Right:**
```bash
powershell -File scripts/keys.ps1 "^a"
```

### `raise.ps1` prints "NO MATCH"
- Chrome window title doesn't contain the search string
- Script prints all Chrome window titles — use exact substring
- Example: `raise.ps1 "LinkedIn"` matches "LinkedIn | Bruce Nigel"

---

## Screen Capture Issues

### `shot.py` fails / black image
- PIL `ImageGrab` requires desktop session (not RDP/headless)
- Run locally on the ASUS, not over SSH/RDP

### `--small` coords wrong
- Remember: **multiply by 1.5**
- Full-size: `shot.py` → 1:1
- Small: `shot.py --small` → ×1.5

### `screen_bot.py` screenshot fails
- `pyautogui` and `cv2` are broken in this Python environment
- Use `shot.py` instead (PIL-based, works)

---

## Firmware Issues

### Serial shows "HID svc: NULL" on `diag`
- HID service not registered
- Reflash firmware
- Check PlatformIO lib_deps: `h2zero/NimBLE-Arduino@^1.4.1`

### ESP32 reboots randomly
- Power supply issue (USB port can't deliver enough current)
- Use powered hub or better USB cable
- Check for brownout: add `setCpuFrequencyMhz(160)` in setup()

### "WiFi FAILED" but serial commands work
- Firmware runs without WiFi
- Serial commands (`type`, `tap`, `key`) still work
- Fix WiFi to enable HTTP API

---

## Platform-Specific

### Windows
- Bluetooth stack sometimes holds stale connections → Remove device, re-pair
- Display scaling default 125%/150% → **Must set to 100%**
- PowerShell execution policy → `Set-ExecutionPolicy RemoteSigned`

### Linux
- May need `sudo` for Bluetooth pairing
- `ImageGrab` works on X11, may need `gnome-screenshot` on Wayland
- Serial port: `usermod -a -G dialout $USER` then logout/login

### macOS
- Bluetooth pairing in System Settings
- May need accessibility permissions for screen capture
- Serial: `ls /dev/tty.*` → usually `/dev/ttyUSB0` or `/dev/tty.wchusbserial*`

---

## Still Stuck?

### Enable debug logging
In `firmware/src/main.cpp`, add:
```cpp
#define DEBUG_HTTP
#define DEBUG_BLE
```
Reflash and monitor serial.

### Capture serial log
```bash
pio device monitor --baud 115200 | tee log.txt
```

### Minimal test sequence
```bash
# 1. Status
curl "http://192.168.1.64/status"

# 2. Home pointer (serial)
# In serial monitor: type "home"

# 3. Simple tap
curl "http://192.168.1.64/tap?x=100&y=100"

# 4. Verify
curl "http://192.168.1.64/status"
# ptr should be ~[100,100]
```

### Factory reset ESP32
1. Hold BOOT button
2. Press and release EN (reset)
3. Release BOOT
4. Reflash firmware

---

## Known Limitations

| Limitation | Workaround |
|------------|------------|
| No multi-monitor support | Keep targets on primary |
| `/move` doesn't move visible pointer | Use `/tap` to reposition |
| `/type` mangles punctuation | Use clipboard paste |
| Scroll only works under pointer | Tap gutter first |
| No Unicode/emoji | Avoid or paste |
| Windows scaling ≠ 100% breaks coords | Set to 100% |
| RDP/headless breaks screenshots | Run locally |