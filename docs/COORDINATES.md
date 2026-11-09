# Coordinate System & Mapping

## Native Resolution
- **Desktop**: 1920 × 1080 pixels
- **Coordinate origin**: Top-left = (0, 0)
- **X increases**: Left → Right
- **Y increases**: Top → Bottom

## 1:1 Mapping (Full-Size Screenshots)
When using `shot.py` (no `--small`):
```
Screenshot pixel (x, y)  =  Tap coordinate (x, y)
```
**Verified**: `/status` returns `ptr:[x,y]` matching exactly what you measure.

## 1.5× Mapping (Downscaled Screenshots)
When using `shot.py --small` (1280×720):
```
Measured pixel (x, y)  →  Tap coordinate (x × 1.5, y × 1.5)
```

Example:
```
Measured at (400, 300) on 1280×720
→ Tap at (600, 450) on native 1920×1080
```

## Coordinate Decision Tree
```
Are you looking at a FULL-SIZE (1920×1080) screenshot?
    YES → Coordinates are 1:1. Use as-is.
    NO  → You're looking at DOWNSIZED (1280×720). Multiply by 1.5.
```

> **Getting this wrong is the #1 cause of mis-taps.**

## Absolute vs Relative
- **All HTTP endpoints**: Absolute screen coordinates
- **Mouse report (BLE)**: Relative deltas (int8: -127 to +127)
- **Firmware internal**: Tracks absolute `curX`, `curY`; converts to deltas

## Homing Behavior
On first `/tap` (or after disconnect/reconnect):
1. Firmware sends -127,-127 ten times → slams pointer to (0,0)
2. Sets `homed = true`, `curX = 0`, `curY = 0`
3. Moves to target via delta steps
4. Updates `curX`, `curY` to target

Subsequent taps: moves from current position via deltas.

## Pointer Position Verification
After any `/tap`, call `/status`:
```json
{"ptr": [700, 400], ...}
```
If `ptr` matches your target → tap landed correctly.

## Common Reference Points (1920×1080)

| Location | Coordinates | Notes |
|----------|-------------|-------|
| Top-left | (0, 0) | |
| Center | (960, 540) | |
| Bottom-right | (1919, 1079) | |
| Chrome omnibox (focused) | ~(300, 60) | After Ctrl+L |
| Neutral gutter (scroll anchor) | (20, 500) | Works on LinkedIn, FB, IG |
| Taskbar (center) | (960, 1050) | Approximate |
| Start button | (30, 1050) | Approximate |

## Multi-Monitor
**Not supported.** The ESP32 only sees the primary virtual desktop. `ImageGrab.grab(all_screens=True)` captures all monitors stitched together, but the HID pointer only moves on the primary monitor's coordinate space.

If you have multiple monitors:
- Coordinates > primary monitor width will behave unpredictably
- Keep all targets on primary monitor (1920×1080)

## Scaling & DPI
- Windows display scaling **must be 100%** for 1:1 mapping
- If scaling is 125%, 150%, etc.:
  - Screenshot will be larger (e.g., 2400×1350 at 125%)
  - But HID coordinates still map to physical pixels
  - **Result: coordinates will be off**

**Fix**: Set display scaling to 100% in Windows Settings → Display → Scale.

## Coordinate Measurement Workflow

```bash
# 1. Take full-size screenshot (1:1)
python shot.py
# Opens: C:\Users\YOU\AppData\Local\Temp\hid\shot.png

# 2. Open in image viewer, measure target (x, y)

# 3. Tap
curl "http://192.168.1.64/tap?x=MEASURED_X&y=MEASURED_Y"

# 4. Verify
curl "http://192.168.1.64/status"
# Check ptr matches
```

## Quick Reference Card

| Screenshot Type | Command | Native Coords? | Conversion |
|-----------------|---------|----------------|------------|
| Full (1920×1080) | `shot.py` | **YES** | 1:1 |
| Small (1280×720) | `shot.py --small` | NO | ×1.5 |
| Custom | `shot.py --out x.png` | Depends on size | Measure & calculate |

## Troubleshooting Coordinates

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Tap lands far off | Used --small coords without ×1.5 | Multiply by 1.5 |
| Tap lands slightly off | Display scaling ≠ 100% | Set to 100% |
| Tap does nothing | `ble: false` | Re-pair Bluetooth |
| Scroll does nothing | Pointer over sidebar/ad | Tap (20,500) first |
| Double-tap acts as single | Too fast | Use `/double_tap` endpoint |