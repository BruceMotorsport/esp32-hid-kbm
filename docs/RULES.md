# Safety Rules & Procedures

> **These rules are non-negotiable.** They exist because this tool drives real input on your real machine with your real accounts. Violating them causes real damage.

---

## The Three Core Rules

### Rule 1: Explicit Handover
> **You must say the machine is yours to drive, and keep your hands off mouse and keyboard while it runs.**

- Two pointers fighting = unintended clicks
- Say "I'm handing over the machine" before starting
- Do not touch mouse/keyboard until you say "I'm taking back control"
- If you must intervene: say "PAUSE" — the bot will wait

### Rule 2: Explicit Approval for Outside Effects
> **Any action with an outside effect needs your explicit word for that single action.**

| Outside Effect (Needs Approval) | Free Actions (No Approval) |
|--------------------------------|----------------------------|
| Save / Submit / Publish | Navigate / Scroll / Read |
| Post / Comment / Message | Type into field (show me first) |
| Follow / Connect / Like | Hover / Move / Screenshot |
| Delete / Remove | Find element / Grid search |
| Pay / Purchase | Wait / Pause |
| Change password / security | Inspect DOM / Copy text |

**One approval ≠ blanket permission.** Each write action needs its own "yes."

### Rule 3: Screenshot After Every Action
> **Screenshot → Act → Screenshot. Never two blind actions in a row.**

- Read the PNG before the next tap
- Do not drive blind
- Do not assume a modal is still open/closed
- Use `shot.py` or `screen_bot.py` screenshot

---

## Standard Operating Procedure

### Before Every Session
1. [ ] Say "I'm handing over the machine"
2. [ ] Verify `curl http://192.168.1.64/status` → `ble: true`
3. [ ] Take baseline screenshot: `python shot.py`
4. [ ] Confirm target window title for `raise.ps1`

### Before Every Write Action (Save/Post/Delete/etc.)
1. [ ] Fill field via clipboard (`paste.ps1`) — **never `/type` for long text**
2. [ ] Show screenshot: "Here's what will be saved"
3. [ ] Get explicit "yes" for **that specific action**
4. [ ] Execute (tap Save, press Enter, etc.)
5. [ ] Screenshot result
6. [ ] **Verify from platform** — read back the saved field, diff against source

### After Every Session
1. [ ] Say "I'm taking back control"
2. [ ] Delete scratch screenshots:
   ```bash
   # Windows
   del %LOCALAPPDATA%\Temp\hid\*.png
   del %USERPROFILE%\Downloads\*.png
   ```
3. [ ] Update any changed source files (CV, profile text, etc.)

---

## Platform-Specific Rules

### LinkedIn
- **Never** post "Open to Work" publicly — Recruiters-only only
- **Never** create a Company Page — use existing org name
- **Headline/About/Experience**: Exact character counts from `linkedin-paste.txt`
- **Save verification**: Copy live field → diff in Python → confirm exact match

### Facebook
- **Inline editors**: Save button ignores HID taps
  - Workaround: Tab to Save (4×), then `key enter`
- **Three-dot menus**: Often ignore HID — hand-click for these
- **About page sidebar**: Links ignore taps — verify on saved edit page
- **Floating pencil**: May be Messenger, not Edit — check before clicking

### Instagram
- **No automation for posting** — only reading/setup
- **Deep links** beat icon tapping: `Ctrl+L` → paste URL → Enter

### General Web
- **Deep-link > Icon tap**: `Ctrl+L` (omnibox) → paste → Enter
- **Zoom out** for below-fold buttons: `zoomout.ps1` (Ctrl+- ×3), restore `^0`
- **Neutral scroll anchor**: Tap (20, 500) before scrolling

---

## Text Input Protocol

### For ANY text with:
- URLs (`https://`, `facebook.com`, etc.)
- Special chars (`·`, `—`, `…`, `•`)
- More than ~20 characters
- Exact character requirements

**Use clipboard, never `/type`:**

```powershell
# 1. Edit scripts/clip.txt with exact text
# 2. Bring target window to front
powershell -File scripts/raise.ps1 "Window Title"

# 3. Focus field (tap or tab), then:
powershell -File scripts/paste.ps1
```

### `paste.ps1` Safety Guard
- Refuses to run if `clip.txt` starts with `EDIT ME`
- Prevents accidental Ctrl+A + paste wiping your data

### `go.ps1` Navigation
```powershell
# 1. Edit scripts/url.txt with target URL
# 2. Bring Chrome to front
powershell -File scripts/raise.ps1 "Chrome"
# 3. Navigate
powershell -File scripts/go.ps1
# Does: Ctrl+L → Ctrl+V → Enter
```

---

## Verification Standard

> **A screenshot of a filled form is NOT proof of a saved field.**

### For Text Fields
1. After save: select all in field → Copy
2. In Python: `copied == intended_text` → must be `True`
3. Example: LinkedIn About confirmed `lw==ll` was `True`, not eyeballed

### For URLs
```bash
curl -sI "https://example.com/profile"  # Must return 200
```

### For Visibility Settings
- Look for globe icon (public) / lock icon (private) on saved row
- Screenshot the indicator

---

## Emergency Procedures

### "PAUSE" — Human Taking Control
1. Say "PAUSE"
2. Bot stops immediately (current action completes)
3. You use mouse/keyboard freely
4. Say "RESUME" to continue

### "ABORT" — Something Went Wrong
1. Say "ABORT"
2. Bot stops
3. You assess: wrong click? wrong field? modal appeared?
4. Screenshot current state
5. Decide: fix manually, or resume with corrected plan

### Bluetooth Drops Mid-Session
- `/status` shows `ble: false`
- **Stop immediately** — clicks won't land
- Re-pair Bluetooth
- Verify `ble: true` before continuing

### Window Lost Focus
- Keystrokes going to wrong app (e.g., your spreadsheet)
- **Stop** — `raise.ps1` to restore focus
- Verify with screenshot

---

## Cleanup Checklist (Every Session)

- [ ] Delete `%LOCALAPPDATA%\Temp\hid\*.png`
- [ ] Delete `%USERPROFILE%\Downloads\shot*.png`, `screen_capture*.jpg`, etc.
- [ ] No screenshots left in home folder
- [ ] Source files updated if profile text changed
- [ ] `sweep_pack.py` runs clean (if job pack modified)

---

## What This Tool Is NOT For

| ❌ Don't Use For | ✅ Use For |
|------------------|------------|
| Volume posting / scheduling | Reading profiles/pages |
| Bulk messaging / connecting | One-off setup forms |
| Automated job applications | Filling fields you review |
| Bypassing platform API limits | Platforms with no API |
| Any "set and forget" | Human-in-the-loop tasks |

---

## Accountability

- Every action logged in session history
- You approve each write — you own the outcome
- If it posts the wrong thing: you said "yes" to that specific action
- No "the bot did it" — the bot only does what you authorized

---

## Quick Reference Card

| Situation | Response |
|-----------|----------|
| Starting session | "I'm handing over" + `status` check + screenshot |
| Before Save/Post | Fill via clipboard → show screenshot → get "yes" → execute → verify |
| Tap missed | Screenshot → re-measure → retry (don't guess) |
| Scroll not working | Tap (20,500) → retry scroll |
| Window wrong | `raise.ps1` → verify → continue |
| Bluetooth drops | STOP → re-pair → verify `ble:true` → resume |
| Unsure | PAUSE → screenshot → assess → decide |
| Done | "Taking back" + delete screenshots + update sources |

---

**Remember**: The account is the asset being sold. One wrong click can cost months of work. Slow is smooth. Smooth is fast.