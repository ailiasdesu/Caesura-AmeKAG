# First-VN Cross-Platform Behavioral Parity Survey Report (Task 02 / R2)

**Author**: Survey Explorer 2  
**Date**: 2026-08-25  
**Target Milestone**: Caesura (AmeKAG) 1.x Release Candidate  
**Working Directory**: `.agents/explorer_survey_5`  
**Focus**: Requirement R2 (Task 02: First-VN Cross-Platform Behavioral Parity)

---

## 1. Executive Summary

This survey provides a comprehensive architectural and verification assessment of **First-VN Cross-Platform Behavioral Parity** across all six Caesura (AmeKAG) engine target platforms (**Windows, Linux, Web, Android, macOS, iOS**).

Where `golden_vn` regresses the full runtime feature surface, `first_vn` (`tests/projects/first_vn/`) acts as the **complete user creation journey acceptance fixture** (Template → Project Creation → Scripting → Asset Resolution → Headless Driving → Branching Choices → Save/Load → Packaging → Launch).

### Key Survey Findings:
1. **Fixture Architecture**: `tests/projects/first_vn/story.ks` (135 lines) exercises 10 key engine subsystems in a compact, self-contained flow: Scene progression (`*start` → `*choice_moment` → `*branch_sun`/`*branch_rain` → `*ending`), dialogue narration, character sprite layering (`Aina`), audio BGM/SE triggers, dynamic i18n locale switching (`en` → `ja` → `zh`), autosave (`[save slot=7]`), user branching, variable expression evaluation (`[if exp="f.is_sun == 1"]`), and graceful load miss probing (`[load slot=8]`).
2. **Existing Platform Harnesses**:
   - **Desktop (Windows/Linux)**: `tests/scripts/first_vn_headless.lua` and `scripts/verify_first_vn.sh` (13/13 PASS checks). Full C++ doctest suite (1028+ tests pass).
   - **Web (Wasm)**: `web/save-choice-regression.integration.test.js`, `web/layout.parity.integration.test.js`, `web/flow.integration.test.js`, and `scripts/web_browser_smoke.mjs` running via Vitest + Wasmoon Lua Wasm engine.
   - **Android**: `scripts/verify_bundle_boot.sh` (native bundle layout boot) + `scripts/android_device_smoke.sh` (ADB automated launch, tap, lifecycle, orientation, crash check) + Xiaomi 11 real-device full walkthrough (`docs/plans/2026-08-24-028-android-full-closure.md`).
   - **iOS / macOS**: Toolchain, Metal shader compilation (`scripts/verify_metal_shaders.py`), and Xcode project definitions verified. Physical execution accurately marked `hardware-gated`.
3. **Parity State Snapshot Design**: Formulated a lightweight, platform-independent `FirstVNStateSnapshot` specification for `artifacts/parity/<platform>.json`. Strictly filters out all OS/GPU leakage (no paths, no window handles, no GPU vendor strings, no dynamic timestamps, no frame counts).
4. **Comparison Tool Design**: Designed `scripts/compare_platform_parity.py` with multi-platform parity assertions (`desktop == web == android == ios`), leak-detection guards, and explicit `hardware-gated` handling without falsifying results.
5. **Architectural Parity Mechanism**: Verified how parity is preserved by design across platforms through coordinate virtualization (1920x1080 logical viewport), unified Lua runtime VM, abstract VFS asset resolution, and standardized SaveManager JSON serialization—enforcing **zero platform if/else branches** in game scripts.

---

## 2. Comprehensive Survey of `tests/projects/first_vn/`

### 2.1 Directory & Asset Structure
```
tests/projects/first_vn/
├── README.md               # User creation flow fixture specification & contracts
├── entry.lua               # Generic KAG runner entry point with fallback discovery
├── story.ks                # 135-line KAG Neo-Genesis story script
└── assets/                 # Self-contained minimal asset pool
    ├── bg/
    │   └── classroom.png   # 1920x1080 classroom background
    ├── fg/
    │   └── girl_uniform.png# Aina character sprite
    ├── bgm/
    │   └── daily.wav       # BGM audio fixture (silent wav placeholder)
    └── se/
        └── click.wav       # SE audio fixture (silent wav placeholder)
```

### 2.2 Story Script Lifecycle & Functional Breakdown (`story.ks`)

| Stage / Label | Lines | KAG Directives / Commands | Subsystem Exercised | Cross-Platform Behavior |
|:---|:---:|:---|:---|:---|
| **Header Directives** | L27–28 | `[font face="default" size=22]`, `[pt speed=40]` | Text / Font layout | Identical font metrics and page speed |
| **Scene 1 (`*start`)** | L33–49 | `[cl]`, `[bg storage="..."]`, `[wait time=400]`, `[playbgm]`, `[ch]`, `[playse]`, `[p]` | Layering, BG rendering, Sprite composition, Audio | Clears layer stack, binds textures, starts BGM, triggers SE |
| **i18n Hot-Switch** | L50–60 | `[i18n language=en]`, `[i18n language=ja]`, `[i18n language=zh]` | Localization / i18n subsystem | Immediate runtime locale switch without scene reload |
| **Autosave** | L61–64 | `[save slot=7]`, `[notify msg="..."]` | Storage / SaveManager | Serializes context to JSON (slot 7); headless records success |
| **Choice Moment** | L68–75 | `[select]`, `[sel target=*branch_sun]`, `[sel target=*branch_rain]`, `[endselect]` | Input router, UI button layout, branching | Presents 2 interactive choices with normalized coordinate hitboxes |
| **Branch Sun** | L77–88 | `[set var="f.route" value="sun"]`, `[set var="f.is_sun" value="1"]`, `[trans method=dissolve]`, `[jump *ending]` | Variable engine, Visual transition, Jump | Sets flag variables, runs dissolve transition, jumps to `*ending` |
| **Branch Rain** | L89–100 | `[set var="f.route" value="rain"]`, `[set var="f.is_sun" value="0"]`, `[trans method=dissolve]`, `[jump *ending]` | Variable engine, Visual transition, Jump | Sets alternate flag variables, runs dissolve transition, jumps to `*ending` |
| **Scene 2 (`*ending`)** | L104–109 | `[cl]`, `[trans method=dissolve]`, `[bg storage="..."]`, `[wait time=300]` | Layering & Transitions | Resets scene backdrop smoothly |
| **Conditional Expression** | L111–116 | `[if exp="f.is_sun == 1"] ... [else] ... [endif]` | Expression parser & evaluator | Evaluates runtime variable `f.is_sun` and routes dialogue epilogue |
| **Graceful Load Probe** | L118–124 | `[load slot=8]` | SaveManager empty-slot error handler | Handles empty slot miss gracefully without looping or crashing |
| **Story Conclusion** | L125–135 | `[playse]`, `[stopbgm fadeout=800]`, `[wait time=800]`, `[end]` | Audio fadeout, Runner lifecycle | Fades out BGM and signals runner termination (`[end]`) |

### 2.3 Variables & State Semantics
- Global Flags (`f`):
  - `f.route`: `"sun"` (Option 1) or `"rain"` (Option 2).
  - `f.is_sun`: `"1"` (Option 1) or `"0"` (Option 2).
- System Flags / Settings:
  - `language`: Hot-switched between `en`, `ja`, and finally `zh`.
- Save / Load Semantics:
  - `save slot=7`: Valid save write (persisted to disk/storage).
  - `load slot=8`: Empty slot read probe (verified graceful non-destructive miss).

---

## 3. Survey of Cross-Platform Test Harnesses & Runners

| Platform | Test Runner / Harness | Execution Command / Script | Verification Evidence & Scope | Parity Coverage |
|:---|:---|:---|:---|:---:|
| **Windows** | `verify_first_vn.sh`<br>`first_vn_headless.lua`<br>`CaesuraTests.exe` | `bash scripts/verify_first_vn.sh`<br>`FIRST_VN_CHOICE=1 lua tests/scripts/first_vn_headless.lua` | 13/13 E2E gate checks pass; C++ 1028+ doctests pass; choice A & B branching verified; save/load verified. | **100% Verified** |
| **Linux** | `verify_first_vn.sh`<br>WSL / Ubuntu C++ suite | `bash scripts/verify_first_vn.sh`<br>`ctest --output-on-failure` | WSL Linux headless regression (`docs/plans/2026-08-22-025-delivery-handoff.md`); identical Lua VM behavior. | **100% Verified** |
| **Web (Wasm)** | Vitest + Wasmoon Wasm<br>`save-choice-regression`<br>`web_browser_smoke.mjs` | `npm --prefix web test`<br>`node scripts/web_browser_smoke.mjs` | Lua 5.1/5.4 Wasm VM (`bridge.js` + `AdapterCore`); text persistence across save/choice verified; DOM rendering verified. | **100% Verified** |
| **Android** | `verify_bundle_boot.sh`<br>`android_device_smoke.sh`<br>Real Device Walkthrough | `bash scripts/verify_bundle_boot.sh`<br>`bash scripts/android_device_smoke.sh` | Real Xiaomi 11 (Android 14) walkthrough (`docs/plans/2026-08-24-028-android-full-closure.md`); save_7.json written, CJK font atlas, touch coordinates verified. | **100% Verified** |
| **macOS** | Native Clang build probe | `cmake -B build-macos && cmake --build build-macos` | Headless compilation probe. | **Pending / Probe** |
| **iOS** | Xcode toolchain + Metal<br>`verify_metal_shaders.py` | `python scripts/verify_metal_shaders.py`<br>`cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS` | Track I compilation & Metal shader embedder verified. Real hardware execution gated. | **Hardware-Gated** |

---

## 4. `FirstVNStateSnapshot` Specification Design

To achieve deterministic cross-platform comparison, the state snapshot must capture semantic game execution state while strictly isolating platform-dependent runtime artifacts.

### 4.1 Anti-Leakage Policy (Forbidden Data)
The snapshot serializer MUST NEVER include:
- ❌ OS filesystem paths (e.g. `d:\...`, `/data/data/...`, `/tmp/...`)
- ❌ Dynamic execution timestamps (e.g. `1724540200`, `12:34:56`)
- ❌ GPU hardware strings or backends (`Adreno 660`, `Metal`, `OpenGL ES 3.0`, `bgfx::RendererType`)
- ❌ Native memory pointers, window handles (`HWND`, `SDL_Window*`), thread IDs, or PIDs
- ❌ Frame counts, tick counts, or timing jitter (`clicks=2811`, `frames=2958`)

### 4.2 Snapshot JSON Schema (`artifacts/parity/<platform>.json`)

```json
{
  "$schema": "https://caesura.engine/schemas/first_vn_state_snapshot.v1.json",
  "platform": "windows",
  "story": "first_vn",
  "status": "verified",
  "evidence": {
    "runner": "scripts/verify_first_vn.sh",
    "commit": "head",
    "verification_type": "automated_headless"
  },
  "state": {
    "route": "sun",
    "flag_is_sun": true,
    "language": "zh",
    "final_label": "*ending",
    "ending": "sunset",
    "completed": true,
    "save_roundtrip": true,
    "audio_bgm_played": true,
    "audio_se_played": true,
    "i18n_locales_exercised": ["en", "ja", "zh"]
  }
}
```

### 4.3 Alternate Route Branch Snapshot (Rain Branch)
```json
{
  "platform": "windows",
  "story": "first_vn",
  "status": "verified",
  "evidence": {
    "runner": "FIRST_VN_CHOICE=2 tests/scripts/first_vn_headless.lua",
    "commit": "head",
    "verification_type": "automated_headless"
  },
  "state": {
    "route": "rain",
    "flag_is_sun": false,
    "language": "zh",
    "final_label": "*ending",
    "ending": "rain_shelter",
    "completed": true,
    "save_roundtrip": true,
    "audio_bgm_played": true,
    "audio_se_played": true,
    "i18n_locales_exercised": ["en", "ja", "zh"]
  }
}
```

---

## 5. Design of Parity Verification Tool `scripts/compare_platform_parity.py`

### 5.1 Tool Requirements & Features
1. **Platform Auto-Discovery**: Reads all snapshot files matching `artifacts/parity/*.json`.
2. **Schema & Leak Validation**: Validates that all required fields are present and ensures no forbidden keys/values exist.
3. **Status Classification**:
   - `verified`: Must strictly match the canonical baseline state.
   - `hardware-gated`: Checked for honest documentation; does not fail CI.
   - `pending` / `credential-gated`: Reported with status notes.
4. **Deep Semantic Assertion**:
   - `state.route == "sun"`
   - `state.flag_is_sun == True`
   - `state.language == "zh"`
   - `state.final_label == "*ending"`
   - `state.completed == True`
   - `state.save_roundtrip == True`
5. **Output Matrix**: Prints a structured terminal table and generates `artifacts/parity/parity_summary.json`.

### 5.2 Implementation Blueprint (`compare_platform_parity.py`)

```python
#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Cross-Platform Parity Verification Tool
Compares FirstVNStateSnapshot across Windows, Linux, Web, Android, iOS, macOS.
"""

import sys
import json
import re
from pathlib import Path

REQUIRED_PLATFORMS = ["windows", "linux", "web", "android"]
GATED_PLATFORMS = ["ios", "macos"]

FORBIDDEN_KEY_PATTERNS = [
    r"gpu", r"vram", r"fps", r"frame_count", r"timestamp",
    r"pointer", r"hwnd", r"pid", r"thread_id", r"path"
]

CANONICAL_STATE_SUN = {
    "route": "sun",
    "flag_is_sun": True,
    "language": "zh",
    "final_label": "*ending",
    "ending": "sunset",
    "completed": True,
    "save_roundtrip": True,
    "audio_bgm_played": True,
    "audio_se_played": True,
    "i18n_locales_exercised": ["en", "ja", "zh"]
}

def validate_leakage(obj, path=""):
    errors = []
    if isinstance(obj, dict):
        for k, v in obj.items():
            current_path = f"{path}.{k}" if path else k
            if current_path != "evidence.runner" and current_path != "evidence.commit":
                for pat in FORBIDDEN_KEY_PATTERNS:
                    if re.search(pat, k, re.IGNORECASE):
                        errors.append(f"Forbidden leaked key detected: {current_path}")
            errors.extend(validate_leakage(v, current_path))
    elif isinstance(obj, list):
        for idx, item in enumerate(obj):
            errors.extend(validate_leakage(item, f"{path}[{idx}]"))
    return errors

def verify_parity(parity_dir: Path) -> bool:
    print("=" * 70)
    print("  Caesura (AmeKAG) — First-VN Cross-Platform Parity Verification")
    print("=" * 70)

    snapshots = {}
    for p_file in parity_dir.glob("*.json"):
        if p_file.name == "parity_summary.json":
            continue
        try:
            with open(p_file, "r", encoding="utf-8") as f:
                data = json.load(f)
                snapshots[data.get("platform", p_file.stem)] = data
        except Exception as e:
            print(f"[ERROR] Failed to load {p_file}: {e}")
            return False

    all_pass = True
    print(f"{'Platform':<12} | {'Status':<15} | {'Route':<6} | {'SunFlag':<8} | {'Lang':<5} | {'Save':<5} | {'Result':<10}")
    print("-" * 70)

    for plat in REQUIRED_PLATFORMS + GATED_PLATFORMS:
        if plat not in snapshots:
            if plat in REQUIRED_PLATFORMS:
                print(f"{plat:<12} | {'MISSING':<15} | {'-':<6} | {'-':<8} | {'-':<5} | {'-':<5} | FAIL (missing)")
                all_pass = False
            else:
                print(f"{plat:<12} | {'UNCONFIGURED':<15} | {'-':<6} | {'-':<8} | {'-':<5} | {'-':<5} | GATED")
            continue

        snap = snapshots[plat]
        status = snap.get("status", "unknown")
        
        # Leakage check
        leaks = validate_leakage(snap.get("state", {}))
        if leaks:
            print(f"{plat:<12} | {status:<15} | LEAK DETECTED: {leaks[0]}")
            all_pass = False
            continue

        if status == "hardware-gated" or status == "pending":
            print(f"{plat:<12} | {status:<15} | {'-':<6} | {'-':<8} | {'-':<5} | {'-':<5} | GATED (honest)")
            continue

        st = snap.get("state", {})
        route = st.get("route", "")
        sun = str(st.get("flag_is_sun", ""))
        lang = st.get("language", "")
        save = str(st.get("save_roundtrip", ""))

        match = True
        for key, expected_val in CANONICAL_STATE_SUN.items():
            if st.get(key) != expected_val:
                match = False
                break

        res_str = "PASS" if match else "FAIL (mismatch)"
        if not match:
            all_pass = False
        print(f"{plat:<12} | {status:<15} | {route:<6} | {sun:<8} | {lang:<5} | {save:<5} | {res_str}")

    print("=" * 70)
    print(f"Overall Parity Status: {'PASS (Parity Confirmed)' if all_pass else 'FAIL (Parity Regression)'}")
    return all_pass
```

---

## 6. How Script Parity Is Preserved Without Platform If/Else

A critical architectural principle of Caesura (AmeKAG) (Rule 4 in `07_AGENT_RULES.md` and `AGENTS.md`) is:
> **"Platform Service → stable interface → shared game logic"**
> Games must NEVER branch on `if platform == "android"` or `#ifdef ANDROID` inside story scripts.

### 6.1 Architectural Decoupling Layers

```
+-------------------------------------------------------------+
|                story.ks (100% Platform-Agnostic)            |
|       [bg] [ch] [playbgm] [select] [save] [i18n] [if]       |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                KAG Runner & Lua Runtime Engine              |
|        kag_runner.lua / scheduler.lua / tokenizer.lua       |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|               BackendRegistry & Platform Abstractions        |
|    IPlatformBackend | IRenderDevice | IAudioBackend | ISave |
+-------------------------------------------------------------+
         |                     |                    |
         v                     v                    v
+-----------------+   +------------------+   +----------------+
|  SDL3 (Desktop) |   | SDL3 + JNI (And) |   | Web/Wasmoon JS |
|  - Mouse coords |   | - Touch remap    |   | - DOM/Pointer  |
|  - Local FS     |   | - APK VFS root   |   | - Fetch VFS    |
|  - SoLoud       |   | - OpenSL/SoLoud  |   | - WebAudio     |
|  - Win32/X11    |   | - Activity/EGL   |   | - LocalStorage |
+-----------------+   +------------------+   +----------------+
```

### 6.2 Key Parity Guarantees
1. **Logical Coordinate Space (1920×1080)**:
   - On Desktop: Window mouse pixels scale directly to 1920×1080.
   - On Mobile (Android/iOS): Physical screen coordinates (e.g. 2320×956 on Xiaomi 11) are remapped in `src/entry/Engine.cpp` to the virtual 1920×1080 viewport before passing to `_GAME_MOUSE_X` / `_GAME_MOUSE_Y`. `story.ks` and `select.lua` never need device-specific coordinate tweaks.
2. **Unified Virtual File System (VFS)**:
   - Relative paths like `assets/bg/classroom.png` resolve transparently across local folders (Desktop), APK asset extraction directories (Android `--resource-root`), and HTTP fetch bridges (Web).
3. **Deterministic Variable & Expression Evaluation**:
   - `[set var="f.is_sun" value="1"]` and `[if exp="f.is_sun == 1"]` run in the standard Lua VM / Wasmoon with identical parsing and evaluation logic.
4. **Storage & Serialization Parity**:
   - `SaveManager` serialization encodes the exact same schema structure across `LocalFileSaveProvider`, Android private files, and Web `localStorage`.

---

## 7. Survey Conclusion & Recommendations

1. **First-VN Acceptance Ready**: `tests/projects/first_vn/` is completely verified across Desktop (Windows/Linux), Web, and Android.
2. **Parity Snapshot Ready for Implementation**: The `FirstVNStateSnapshot` specification is ready to be instantiated in `artifacts/parity/` for Windows, Linux, Web, Android, and iOS (hardware-gated).
3. **Parity Comparison Tooling Ready**: `scripts/compare_platform_parity.py` can immediately be authored to provide CI-grade automated parity checks.
