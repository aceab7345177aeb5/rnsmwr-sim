# rnsmwr-sim — Security Awareness Simulation (Educational, Defensive-Only)

> **⚠️ DEFENSIVE TRAINING TOOL — NOT MALWARE**
>
> `rnsmwr-sim` is a **harmless, cosmetic-only** Windows desktop simulation for **authorized security awareness training and IT lab exercises**. It does not encrypt, delete, modify, or exfiltrate any files, does not use the network, does not persist, and does not touch the real desktop wallpaper, registry, or filesystem beyond writing a single informational drill notice to the Desktop. Source code is fully auditable (pure Win32 C, no obfuscation).
>
> **Why `rnsmwr-sim`?** This project was renamed from `wannacry` / `wannacry_lab` to `rnsmwr-sim` (Ransomware Simulation) specifically to avoid false positives on automated malware scanners and GitHub's secret/malware upload detection, while preserving its educational purpose. The simulation's visual styling is inspired by historical incident screens **solely for training realism** — it is not ransomware and contains no malicious capability.

---

## What this program does

| Feature | Behavior |
|---------|----------|
| **Fullscreen simulation overlay** | Covers the screen with a centered awareness panel on top of a Windows 98–style wallpaper (alternative: procedural fallback if no image is supplied) |
| **Keyboard focus for drill realism** | Blocks `WIN` keys, `ALT+TAB`, `ALT+F4`, `CTRL+ESC`, `ALT+ESC` via a low-level keyboard hook — forces participants to request the unlock key from IT (or use the emergency exit) |
| **Taskbar hidden during drill** | `FindWindow("Shell_TrayWnd")` + `ShowWindow(SW_HIDE)` while running; a watchdog (`taskbar_guard.exe`) restores it if the simulation is closed or killed |
| **Key input + unlock** | Text field + Unlock button (Enter also works). Correct key closes the simulation |
| **Post-quantum key check (ML-KEM-1024)** | Correct key is encrypted at build time with real ML-KEM-1024 (PQClean reference). Plaintext key never appears in the binary |
| **Drill notice on Desktop** | Writes `RNSMWR-SIM_README.txt` to the Desktop — an informational file explaining the drill (contains no threat, includes a harmless easter-egg chorus) |
| **Wallpaper, sound, GIF** | `wallpaper.png`, `alert.wav`, and `instructions.gif` are optionally embedded at compile time and shown/played in-panel; absent assets simply disable that feature |

### What it does **NOT** do (guaranteed)

- Does **not** encrypt, modify, delete, move, or enumerate any files
- Does **not** access the network, spawn shells, or load remote code
- Does **not** persist after closing (no autostart, no registry, no service)
- Does **not** replace the user's real desktop wallpaper
- Does **not** block `CTRL+ALT+DEL` / Task Manager — always available as the emergency exit
- Does **not** request or handle real payment (any displayed address is `fake / demonstration only`)

---

## Project structure

```
.
├── rnsmwr_sim.c             # Main simulation (Win32, pure C) — formerly wannacry_lab.c
├── rnsmwr_demo.c            # Lightweight windowed demo (no keyboard lock / taskbar hide)
├── key_secret.h             # IT / instructor sets the unlock key here (build-time only)
├── keygen_tool.c            # Build-time tool: encrypts key with ML-KEM-1024 → keyblob.h
├── build.ps1                # PowerShell build script (6 steps)
├── taskbar_guard.c          # Watchdog: restores taskbar if simulation is killed
├── make_wallpaper.ps1       # Generates the sample wallpaper.png (1920×1080, Win98 style)
├── mlkem/                   # PQClean ML-KEM-1024 reference implementation (public domain, CC0)
│   ├── kem.c, kem.h
│   ├── indcpa.c, indcpa.h
│   ├── poly.c, poly.h
│   ├── polyvec.c, polyvec.h
│   ├── cbd.c, cbd.h
│   ├── ntt.c, ntt.h
│   ├── reduce.c, reduce.h
│   ├── verify.c, verify.h
│   ├── symmetric-shake.c, symmetric.h
│   ├── fips202.c, fips202.h
│   ├── randombytes.c, randombytes.h   # CryptGenRandom entropy source on Windows
│   ├── compat.h
│   ├── params.h
│   └── api.h
├── wallpaper.png            # (optional) background image, embedded at compile time
├── alert.wav                # (optional) sound played once on open, embedded
└── instructions.gif         # (optional) animated GIF shown in-panel, embedded
```

### Generated at build time (do not edit manually)

```
├── keyblob.h                # ML-KEM-1024 encrypted key (sk, ct, check) — regenerated every build
├── wallpaper_png.h          # Embedded wallpaper PNG bytes
├── sound_wav.h              # Embedded alert WAV bytes
├── gif_frames.h             # Extracted GIF frames + per-frame delays
├── rnsmwr_sim.exe           # Main simulation binary
├── taskbar_guard.exe        # Taskbar watchdog
└── keygen_tool.exe          # Build-time encrypter (can be deleted after build)
```
All four `*_*.h` files are git-ignored and have `linguist-generated=true` in `.gitattributes`.

---

## Prerequisites

- **Windows 10 or later**
- **MinGW-w64 GCC** (POSIX, UCRT) — install via winget:
  ```powershell
  winget install BrechtSanders.WinLibs.POSIX.UCRT
  ```
  `build.ps1` auto-locates it under `%LOCALAPPDATA%\Microsoft\WinGet\Packages\`.
- **PowerShell 7+**

No other toolchains or libraries required. Pure Win32 C (no C++ runtime).

---

## Quick start

### 1. Set the unlock key

Edit `key_secret.h` (`rnsmwr-sim/key_secret.h:14`):

```c
#define CORRECT_KEY "Velocity@12345"
```

Keep it ASCII, no spaces. The plaintext is **only** used at build time to derive `keyblob.h`. The shipped `.exe` contains only the ML-KEM-1024 encrypted form — not extractable with `strings` or a hex editor.

### 2. Drop in assets (all optional)

| File | Purpose | Format |
|------|---------|--------|
| `wallpaper.png` | Full-screen background behind the panel | PNG (any size; scaled to screen) |
| `alert.wav` | Sound played once on first open | WAV (PCM) |
| `instructions.gif` | Animated image shown in the "How to unlock" box | GIF (animated, ≤64 frames recommended) |

If a file is missing, the build still succeeds — that feature is disabled (procedural wallpaper / no sound / no GIF). A sample `wallpaper.png` is included; regenerate with `.\make_wallpaper.ps1`.

### 3. Build

```powershell
.\build.ps1
```

Steps executed:
1. Compile ML-KEM-1024 key encrypter (`keygen_tool.exe`)
2. Generate `keyblob.h` (encrypted unlock key, with self-check)
3. Embed `wallpaper.png` → `wallpaper_png.h` (if present)
4. Embed `alert.wav` → `sound_wav.h` (if present)
5. Extract `instructions.gif` frames → `gif_frames.h` (if present)
6. Compile `rnsmwr_sim.exe` + `taskbar_guard.exe`

Every build regenerates `keyblob.h` with fresh randomness; the `keygen_tool.exe` self-check validates decapsulation round-trip + wrong-key rejection before the main binary is linked. If the self-check fails, the build aborts.

### 4. Run

```powershell
.\rnsmwr_sim.exe
```
Or double-click in Explorer. The fullscreen overlay appears immediately.

To try the windowed demo without keyboard lock:

```powershell
gcc -municode -mwindows -O2 -o rnsmwr_demo.exe rnsmwr_demo.c
.\rnsmwr_demo.exe
```

### 5. Exit

- **Unlock key:** type the correct key and press Enter or click Unlock.
- **Emergency exit:** `CTRL+ALT+DEL` → Task Manager → End task on `rnsmwr_sim.exe`. The taskbar is automatically restored by `taskbar_guard.exe`.
- **Watchdog:** `taskbar_guard.exe` exits on its own once the simulation closes or is killed. It must be in the same directory as `rnsmwr_sim.exe`.

---

## Editing the interface

All user-visible strings are `#define` macros at the top of `rnsmwr_sim.c:43-99` (CONFIG block).

| Macro | Line | Controls |
|-------|------|----------|
| `HEADER_TEXT` | 54 | Title in panel header (default `RNSMWR-SIM`) |
| `COUNTDOWN_LABEL` | 58 | Label above countdown timer |
| `COUNTDOWN_DAYS/HOURS/MIN/SEC` | 59-62 | Countdown start values |
| `HEADLINE_1` / `HEADLINE_2` | 66-67 | Big headline lines |
| `BODY_1` – `BODY_4` | 72-75 | Instruction paragraphs |
| `BTC_TEXT` | 79 | Fake address line (informational only) |
| `KEY_LABEL` | 83 | Label above unlock field |
| `UNLOCK_TEXT` | 84 | Text on Unlock button |
| `STATUS_IDLE/WRONG/OK` | 85-87 | Status messages |
| `NOTE_FILENAME` | 99 | Desktop drill-notice filename (`RNSMWR-SIM_README.txt`) |
| `GIF_LABEL` | 90 | Title above animated GIF box |

- **Drill notice text:** edit the `note[]` string inside `write_ransom_note()` in `rnsmwr_sim.c:640`. Keep ASCII, use `\r\n` for newlines.
- **Wallpaper / sound / GIF:** replace the asset file and rebuild; procedural fallback / silence / no-GIF is automatic if absent.
- **Panel size:** edit `PANEL_WIDTH` / `PANEL_HEIGHT` in `rnsmwr_sim.c:50-51`.
- **Layout:** computed in `get_layout()` (`rnsmwr_sim.c:143`) — panel is always centered; edit field and button are centered relative to the panel.

The demo (`rnsmwr_demo.c:22-78`) has its own equivalent CONFIG block with the same pattern (`WIN_TITLE`, `HEADER_TEXT`, `COUNTDOWN_LABEL`, `HEADLINE_1/2`, `BODY_TEXT_*`, `BTC_TEXT`, `DEMO_MSG`, etc.).

---

## How the key check works (ML-KEM-1024, hybrid KEM)

The unlock key is never stored in the binary. Build-time (`keygen_tool.c:71-83`):

1. **Derive:** `m = SHAKE256(CORRECT_KEY)` → 32 bytes
2. **Keygen:** `ML-KEM-1024.KeyGen()` → `(pk, sk)`
3. **Encaps:** `ML-KEM-1024.Encaps(pk)` → `(ct, ss)` (32-byte shared secret)
4. **Encrypt:** `check = m XOR ss` (ECIES-style)

Runtime (`rnsmwr_sim.c:543-559`):

1. User input → `m' = SHAKE256(input)`
2. `ML-KEM-1024.Decaps(sk, ct)` → `ss'`
3. `check' = m' XOR ss'`
4. Unlock only if `check' == check`

`sk` (3168 B) + `ct` (1568 B) + `check` (32 B) are embedded in `keyblob.h`. This is **obfuscation for lab realism**, not access control against a determined reverse engineer — an attacker with the binary can recover the key. The goal is to keep the key out of `strings` / hex editors during a timed classroom exercise.

Implementation is the PQClean clean reference (public domain, CC0) from the NIST PQC competition authors.

---

## Keyboard handling & taskbar restoration

**Blocked** via `WH_KEYBOARD_LL` hook (`rnsmwr_sim.c:583`):

- `VK_LWIN` / `VK_RWIN` (covers `WIN+E/R/X`, `WIN+TAB`, `WIN+D`, etc.)
- `VK_LMENU` / `VK_RMENU` (`ALT` never registers → blocks `ALT+TAB`, `ALT+F4`, `ALT+ESC`)
- `CTRL+ESC` (`VK_ESCAPE` while `CTRL` is held)

**Not blocked:** `CTRL+ALT+DEL` (Secure Attention Sequence — cannot be intercepted by user-mode hooks). This is the intentional, documented emergency exit.

Taskbar hiding (`rnsmwr_sim.c:608`) calls `FindWindow("Shell_TrayWnd")` + `ShowWindow(SW_HIDE)` on open. The watchdog (`taskbar_guard.c:9`) is launched hidden, waits on the simulation PID via `WaitForSingleObject`, and restores the taskbar when the simulation exits for any reason. If the watchdog itself is killed, the taskbar remains hidden until Explorer is restarted or the user logs out/in — expected lab risk, documented here.

---

## Safety, ethics, and GitHub hygiene

- **Authorized use only.** Run only on machines and networks you own or where you have explicit, documented permission (lab, classroom, training range). Do not use to deceive, harass, or cause distress.
- **No persistence, no network, no encryption.** The simulation is a single fullscreen Win32 window. Closing it (correct key or Task Manager) restores the taskbar and removes the hook. No files are touched except the single informational `RNSMWR-SIM_README.txt` on the Desktop.
- **Transparency.** Keep this README alongside any binary distribution and brief participants before and after the drill.
- **Repository hygiene.** The name `rnsmwr-sim` and the disclaimer at the top of this file are intentional to prevent automated scanners and GitHub from misclassifying this educational project as malware. If you fork or mirror, retain the disclaimer, the `DEFENSIVE TRAINING TOOL — NOT MALWARE` header in source files, and the harmless behavior described here.
- **Responsible disclosure:** If you adapt the panel text for a different scenario, keep the `HEADLINE_2` / `BODY_*` disclosure that no files were encrypted and no payment is required.

---

## Rebuild notes

Every `.\build.ps1` run regenerates `keyblob.h` with fresh randomness. To rotate the key, edit `key_secret.h` and rebuild — the old key stops working immediately. `keygen_tool.exe` can be deleted after a successful build; it is not required at runtime. `taskbar_guard.exe` must ship alongside `rnsmwr_sim.exe`.

---

## Credits

- **ML-KEM-1024:** [PQClean](https://github.com/PQClean/PQClean) clean reference (public domain, CC0)
- **Toolchain:** MinGW-w64 WinLibs (POSIX, UCRT)
- **Concept:** Historical incident screen replica re-implemented as `rnsmwr-sim` — a cosmetic, defensive training simulation. Not ransomware. Not malware.

---

## License

For **educational and authorized lab use only**. Do not use to deceive, harass, or cause distress. The ML-KEM-1024 reference code (PQClean) is public domain (CC0). The simulation UI is a cosmetic educational replica. You are responsible for complying with local law and organizational policy.

