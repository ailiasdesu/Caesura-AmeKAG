# Comprehensive Survey Report: Requirement R1 (Unified Platform Status Matrix & Generator)

> **Author**: Survey Explorer 1  
> **Date**: 2026-08-25T02:22:00+08:00  
> **Target Repo**: Caesura (AmeKAG) — 1.x Release Candidate  
> **Working Directory**: `.agents/explorer_survey_4`  
> **Current HEAD Commit**: `62132e783dd238752659d4227ff26b0235258ea9`  

---

## 1. Survey of Existing Status Documents & Platform Drift Analysis

### 1.1 Existing Status Document Inventory
A thorough investigation of all documentation across `docs/status/`, `docs/plans/`, `docs/platform/`, `docs/release/`, `docs/guides/`, `docs/design/`, `README.md`, and `ROADMAP-200.md` identified the following status-holding artifacts:

| # | Document Path | Primary Scope | Last Stated Status | Notes / Limitations |
|---|---|---|---|---|
| 1 | `docs/status/mobile-platform-status.md` | Mobile (Android & iOS) Track M/I | Android: CI build-verified, Device pending; iOS: CI build-verified, Device pending | Authored 2026-08-24 r38; precedes the 028 real-device closure. |
| 2 | `docs/status/web-release-status.md` | Web (Track W) | **RC-READY (W8)** | Comprehensive CDP real-browser evidence (Chrome/Edge), 318 vitest tests, stress, audio unlock. |
| 3 | `docs/release/cross-platform-matrix.md` | All 6 platforms summary | Windows/Linux/Web: Verified; Android: Build verified, runtime `?`; macOS/iOS: Probe / `?` | Draft from 2026-08-23; out-of-date regarding Android real-device status. |
| 4 | `docs/guides/release-qa-matrix.md` | Release QA checklist & tiers | Windows/Linux/Web: Verified; macOS/Steam: Pending; Android: Not fully split | Focuses on creator journey and QA gating. |
| 5 | `docs/platform/android-device-validation.md` | Android Track M A4 | **device-verified** (Core items 2026-08-24) | Xiaomi 11 (alioth, SD888, Adreno 660, Android 13/14) E2E verification. |
| 6 | `docs/platform/android-release-signing.md` | Android Track A5 | **release-ready** (Signing & AAB pipeline) | Keytool PKCS12, env vars, gradle assembleRelease/bundleRelease, apksigner v1/v2/v3, zipalign 4B. |
| 7 | `docs/platform/ios-build-and-validation.md` | iOS Track I (I0–I3) | **I0 probe verified, I1–I3 pending hardware** | Toolchain, CMake iOS Xcode generator, Metal shaders, CI ios-compile. |
| 8 | `docs/platform/ios-device-validation.md` | iOS Track I I4 | **device-unverified / hardware-gated** | Structured test checklist for future physical Mac + iPhone/simulator testing. |
| 9 | `docs/plans/2026-08-24-028-android-full-closure.md` | 028 Hand-off document | **Android 100% full closure** | Authoritative record of Android GLES TTF RGBA8, multi-texture batching, RTT ID fix, touch scaling. |
| 10 | `docs/plans/2026-08-24-027-antigravity-handoff.md` | Antigravity engine status | Baseline counts, iron rules | 1028 C++ doctests, 133+24 Lua tests, 16 modules, 34 interfaces. |
| 11 | `docs/plans/audit/ROADMAP-200.md` | Iteration roadmap rounds 101–143 | Productization sprints 1–5c | Tracks all PR closures and test counts. |
| 12 | `README.md` | Top-level project entry | Capability overview | Lists 6 platforms, but lacks machine-readable status synchronization. |

---

### 1.2 Identified Cross-Platform Status Inconsistencies & Drifts

1. **Android Status Inconsistency (Critical Drift)**:
   - **Historical documents** (`docs/release/cross-platform-matrix.md`, `docs/status/mobile-platform-status.md`): Marked Android runtime capabilities (Boot, Text, CJK, Image, Input, Audio, Save/Load, Lifecycle) as `?` or `⏳ 待设备` (Hardware-gated).
   - **Current HEAD state** (`docs/plans/2026-08-24-028-android-full-closure.md`, `docs/platform/android-device-validation.md`, commit `1f054039`): Real device closure on Xiaomi 11 (M2012K11AC, Snapdragon 888, Adreno 660, Android 13/14) verified Boot, Touch Tap, Long Press, Orientation, Lifecycle, CJK/TTF RGBA8 2048x2048 font rendering, Save/Load (slot 7 + autosave), SoLoud audio, and Release signing.
   - **Remediation**: Establish `docs/status/platform-matrix.yaml` as the single source of truth, updating Android build, runtime, first_vn, real_device, signing, and aab to `verified`, while keeping release as `pending` (credential-gated for official production keys).

2. **macOS & iOS Status Granularity**:
   - `README.md` lists `macOS` and `iOS (Track I)` in the platforms header without qualification.
   - `docs/guides/release-qa-matrix.md` marks macOS Package as `⏳ pending (需 Mac 真机)`.
   - `docs/Caesura_AmeKAG_Agent_Pack/01_STATUS_MATRIX.md` dictates that CI compilation probe MUST NOT be conflated with real-device verification.
   - **Remediation**: In `platform-matrix.yaml`, mark macOS and iOS `build` as `probe` (CI Clang / Xcode compilation verified), iOS `metal` as `probe` (verified by `scripts/verify_metal_shaders.py`), and `real_device` as `hardware-gated`.

3. **Linux Packaging & xvfb Runtime Verification**:
   - `docs/release/cross-platform-matrix.md` marked Linux Stress as `◐` and Build as `●`.
   - `docs/plans/2026-08-22-025-delivery-handoff.md` and CI workflow `.github/workflows/ci.yml` verify that Linux passes full CTest 11/11, 1028 doctests, and xvfb-run bundled boot for both `first_vn` and `demo` (`scripts/verify_bundle_boot.sh`).
   - **Remediation**: Clarify Linux capabilities in `platform-matrix.yaml` with concrete test commands.

---

## 2. Specification and Schema for `docs/status/platform-matrix.yaml`

### 2.1 Allowed Status Enums & Strict Gating Rules
The schema strictly forbids ambiguous terms (`almost done`, `basically done`, `ready-ish`, `complete` without evidence, etc.) and enforces one of the following 7 standard enums:

1. `verified`: Capability is fully implemented, with automated CI or physical device/browser test execution evidence attached.
2. `probe`: Capability is verified at the compilation, static analysis, or shader asset level (e.g. CI compiler probe), but lacking windowed/device runtime verification.
3. `pending`: Capability implementation or full test coverage is currently planned or in-progress.
4. `hardware-gated`: Capability is blocked exclusively by the physical absence of required hardware (e.g., Apple Silicon Mac, physical iPhone/iPad).
5. `credential-gated`: Capability is blocked exclusively by the absence of private deployment keys/accounts (e.g., Apple Developer Program certificate, Steamworks developer account, Production Google Play keystore).
6. `blocked`: Capability is broken or halted due to an unresolved defect.
7. `not-applicable`: Capability is not relevant to the target platform (e.g., Safe Area on desktop Windows/Linux).

---

### 2.2 Complete YAML Schema Specification

```yaml
version: 1
schema_version: "1.0.0"
generated_document: "docs/status/platform-status.md"
last_updated: "2026-08-25T02:00:00Z"
head_commit: "62132e783dd238752659d4227ff26b0235258ea9"

allowed_status_enums:
  - verified
  - probe
  - pending
  - hardware-gated
  - credential-gated
  - blocked
  - not-applicable

platforms:
  windows:
    display_name: "Windows (x64)"
    tier: 1
    summary_status: verified
    environment:
      os: "Windows 11 / Windows 10 x64"
      compiler: "MSVC 19.38+ (Visual Studio 2022)"
      graphics_api: "Direct3D 11 / OpenGL 4.3 (bgfx)"
      audio_backend: "SoLoud (WASAPI / WinMM)"
      toolchain: "CMake 3.25+ / vcpkg"
    capabilities:
      build:
        status: verified
        evidence:
          commit: "62132e78"
          document: "README.md"
          test: "cmake --build build --config Debug --parallel"
          verified_at: "2026-08-25T01:57:33Z"
      runtime:
        status: verified
        evidence:
          commit: "62132e78"
          document: "docs/plans/2026-08-24-027-antigravity-handoff.md"
          test: "build/tests/Debug/CaesuraTests.exe && external/lua/lua.exe tests/scripts/run_lua_tests.lua"
          verified_at: "2026-08-25T01:57:33Z"
          notes: "1028 doctest cases (0 failed), 133 main Lua suites (0 failed), 24 orphan suites (0 failed)"
      first_vn:
        status: verified
        evidence:
          commit: "62132e78"
          document: "scripts/verify_first_vn.sh"
          test: "bash scripts/verify_first_vn.sh"
          verified_at: "2026-08-25T01:57:33Z"
          notes: "13/13 user journey checks passed (template, creation, metadata, validation, headless run, choices A/B, save/load, package)"
      packaging:
        status: verified
        evidence:
          commit: "62132e78"
          document: "docs/plans/audit/ROADMAP-200.md"
          test: "cd build && cpack -C Release -G ZIP"
          verified_at: "2026-08-25T01:57:33Z"
          notes: "Standalone ZIP artifact (CaesuraAmeKAG-1.0.1-Windows-AMD64.zip)"
      release:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/Caesura_AmeKAG_Agent_Pack/05_RELEASE_CANDIDATE.md"
          test: "RC-GO decision gate"
          verified_at: "2026-08-25T01:57:33Z"

  linux:
    display_name: "Linux (x64 / Ubuntu 24.04 / WSL)"
    tier: 1
    summary_status: verified
    environment:
      os: "Ubuntu 24.04 LTS / WSL2 Linux x86_64"
      compiler: "GCC 13.2+ / Clang 18+"
      graphics_api: "OpenGL 4.3 (bgfx / Mesa GLX)"
      audio_backend: "SoLoud (ALSA / PulseAudio / Null device in CI)"
      toolchain: "CMake 3.25+ / Ninja"
    capabilities:
      build:
        status: verified
        evidence:
          commit: "806275cf"
          document: "docs/plans/2026-08-22-025-delivery-handoff.md"
          test: "cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)"
          verified_at: "2026-08-22T20:00:00Z"
      runtime:
        status: verified
        evidence:
          commit: "806275cf"
          document: "docs/plans/2026-08-22-025-delivery-handoff.md"
          test: "ctest --test-dir build --output-on-failure"
          verified_at: "2026-08-22T20:00:00Z"
          notes: "11/11 CTest test targets passed on WSL and GitHub Actions Linux CI"
      first_vn:
        status: verified
        evidence:
          commit: "806275cf"
          document: ".github/workflows/ci.yml"
          test: "xvfb-run -a -s '-screen 0 1280x720x24' bash scripts/verify_bundle_boot.sh build/CaesuraAmeKAG first_vn"
          verified_at: "2026-08-22T20:00:00Z"
      packaging:
        status: verified
        evidence:
          commit: "806275cf"
          document: "scripts/verify_bundle_boot.sh"
          test: "bash scripts/verify_bundle_boot.sh build/CaesuraAmeKAG demo"
          verified_at: "2026-08-22T20:00:00Z"
      release:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/Caesura_AmeKAG_Agent_Pack/05_RELEASE_CANDIDATE.md"
          test: "RC-GO decision gate"
          verified_at: "2026-08-25T01:57:33Z"

  web:
    display_name: "Web Player (WASM / DOM)"
    tier: 1
    summary_status: verified
    environment:
      runtime: "Wasmoon Lua 5.4 VM + Vite 5 + Vanilla ES Modules"
      renderer: "DOM / CSS Layered Compositor"
      audio: "WebAudio API (gain, crossfade, unlock gesture)"
      storage: "localStorage (JSON envelope)"
      tested_browsers: "Google Chrome 128+ / Microsoft Edge 128+ (Headless-new CDP)"
    capabilities:
      build:
        status: verified
        evidence:
          commit: "6e90d7df"
          document: "docs/status/web-release-status.md"
          test: "cd web && npm run build"
          verified_at: "2026-08-24T12:00:00Z"
          notes: "Zero-dependency dist bundle (159KB, gzip 46KB) with vendored glue.wasm"
      runtime:
        status: verified
        evidence:
          commit: "6e90d7df"
          document: "docs/status/web-release-status.md"
          test: "cd web && npm test"
          verified_at: "2026-08-24T12:00:00Z"
          notes: "318 vitest tests across 21 test files (100% passed)"
      first_vn:
        status: verified
        evidence:
          commit: "6e90d7df"
          document: "scripts/verify_first_vn.sh"
          test: "bash scripts/package_game.sh tests/projects/first_vn"
          verified_at: "2026-08-24T12:00:00Z"
      browser:
        status: verified
        evidence:
          commit: "6e90d7df"
          document: "docs/status/web-release-status.md"
          test: "node scripts/web_browser_smoke.mjs --root dist/first_vn --unlock"
          verified_at: "2026-08-24T12:00:00Z"
          notes: "Real-browser CDP assertions: boot (parked), CJK text, images, input, audio unlock (suspended->running), reload save persistence"
      release_candidate:
        status: verified
        evidence:
          commit: "6e90d7df"
          document: "docs/status/web-release-status.md"
          test: "Track W W8 release checklist (14/14 PASS)"
          verified_at: "2026-08-24T12:00:00Z"

  android:
    display_name: "Android (ARM64)"
    tier: 1
    summary_status: verified
    environment:
      device: "Xiaomi M2012K11AC (alioth, 小米 11)"
      soc_gpu: "Snapdragon 888 / Adreno 660"
      android_os: "Android 13 / 14 (API level 33/34)"
      abi: "arm64-v8a"
      ndk: "NDK r27.3.13750724"
      graphics_api: "OpenGL ES 3.2 (bgfx GLES)"
      audio_backend: "SoLoud (OpenSLES, 3 buses)"
    capabilities:
      build:
        status: verified
        evidence:
          commit: "1f054039"
          document: ".github/workflows/ci.yml"
          test: "scripts/build_android.sh --release --abi arm64-v8a"
          verified_at: "2026-08-24T22:00:00Z"
          notes: "libCaesuraAmeKAG.so MODULE target + SDL3 3.2.4 & OpenSSL 3.3.2 slices"
      runtime:
        status: verified
        evidence:
          commit: "1f054039"
          document: "docs/plans/2026-08-24-028-android-full-closure.md"
          test: "bash scripts/android_device_smoke.sh"
          verified_at: "2026-08-24T22:00:00Z"
          notes: "FreeType 2048x2048 RGBA8 CJK font atlas (8074 glyphs), transient buffer fix, RTT viewport fix"
      first_vn:
        status: verified
        evidence:
          commit: "1f054039"
          document: "docs/platform/android-device-validation.md"
          test: "Full E2E walkthrough on device (story.ks -> choice 1 sunset -> ending pass)"
          verified_at: "2026-08-24T22:00:00Z"
      real_device:
        status: verified
        evidence:
          commit: "1f054039"
          document: "docs/platform/android-device-validation.md"
          test: "Physical Xiaomi 11 adb/su logcat + screencap suite"
          verified_at: "2026-08-24T22:00:00Z"
          notes: "Launch, touch tap, long press (GestureDetector), landscape orientation lock, sleep/wake power cycle, save slot 7 + autosave"
      signing:
        status: verified
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/android-release-signing.md"
          test: "apksigner verify --verbose --print-certs android/app/build/outputs/apk/release/app-release.apk"
          verified_at: "2026-08-24T23:30:00Z"
          notes: "V1/V2/V3 signing verified true, zipalign 4-byte check passed, zero hardcoded credentials"
      aab:
        status: verified
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/android-release-signing.md"
          test: "cd android && gradle bundleRelease"
          verified_at: "2026-08-24T23:30:00Z"
          notes: "app-release.aab built with language/density/abi splits disabled for visual novel assets"
      release:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/Caesura_AmeKAG_Agent_Pack/05_RELEASE_CANDIDATE.md"
          test: "Official keystore signing & store publishing gate"
          verified_at: "2026-08-25T01:57:33Z"

  macos:
    display_name: "macOS (Apple Silicon / Intel)"
    tier: 2
    summary_status: probe
    environment:
      os: "macOS 14 Sonoma (GitHub Actions macos-latest runner)"
      compiler: "Apple Clang 15+"
      graphics_api: "Metal / OpenGL (bgfx)"
      audio_backend: "SoLoud (CoreAudio)"
      toolchain: "CMake 3.25+ / Homebrew"
    gates:
      hardware: "Physical Apple Silicon Mac is not present in local dev loop; CI covers compilation and unit tests."
    capabilities:
      build:
        status: probe
        evidence:
          commit: "62132e78"
          document: ".github/workflows/ci.yml"
          test: "cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCAESURA_ENABLE_FFMPEG=OFF && cmake --build build"
          verified_at: "2026-08-25T01:57:33Z"
          notes: "CI macos-latest compiles full module graph in ~2m12s"
      runtime:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/guides/release-qa-matrix.md"
          test: "ctest --test-dir build --output-on-failure"
          verified_at: "2026-08-25T01:57:33Z"
          notes: "Non-GUI test targets pass; full windowed Cocoa/Metal runtime pending physical device"
      first_vn:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/release/cross-platform-matrix.md"
          test: "Interactive / windowed first_vn execution pending hardware"
          verified_at: "2026-08-25T01:57:33Z"
      real_device:
        status: hardware-gated
        evidence:
          commit: "62132e78"
          document: "docs/platform/ios-device-validation.md"
          test: "Physical Mac interactive smoke testing"
          verified_at: "2026-08-25T01:57:33Z"
      release:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/Caesura_AmeKAG_Agent_Pack/05_RELEASE_CANDIDATE.md"
          test: "Apple Developer ID signing and notarization"
          verified_at: "2026-08-25T01:57:33Z"

  ios:
    display_name: "iOS (Track I / Metal)"
    tier: 2
    summary_status: probe
    environment:
      target_devices: "iPhone / iPad (iOS 16+ / arm64)"
      graphics_api: "Metal (BGFX_RENDERER_TYPE_METAL)"
      audio_backend: "SoLoud (CoreAudio / AVAudioSession)"
      toolchain: "CMake 3.25+ / Xcode 15+ (ios-compile probe)"
    gates:
      hardware: "Physical iPhone/iPad or local Xcode simulator hardware gated."
      credential: "Apple Developer Program signing certificate and provisioning profile required for device install / TestFlight."
    capabilities:
      build:
        status: probe
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/ios-build-and-validation.md"
          test: "cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 && cmake --build build-ios --config Release"
          verified_at: "2026-08-24T23:30:00Z"
          notes: "CI ios-compile probe compiles full module graph with iOS SDL3 & OpenSSL slices"
      metal:
        status: probe
        evidence:
          commit: "8aa51c36"
          document: "scripts/verify_metal_shaders.py"
          test: "python3 scripts/verify_metal_shaders.py"
          verified_at: "2026-08-24T23:30:00Z"
          notes: "10 2D render shaders + 2 3D MSL minigame shaders + postfx & SMA compute fallbacks verified"
      runtime:
        status: pending
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/ios-device-validation.md"
          test: "UIKit event loop & AudioSession interruption handling"
          verified_at: "2026-08-24T23:30:00Z"
      real_device:
        status: hardware-gated
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/ios-device-validation.md"
          test: "Physical iPhone 15/16 or iOS Simulator interactive run"
          verified_at: "2026-08-24T23:30:00Z"
      signing:
        status: credential-gated
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/ios-build-and-validation.md"
          test: "Apple Developer code signing"
          verified_at: "2026-08-24T23:30:00Z"
      testflight:
        status: credential-gated
        evidence:
          commit: "8aa51c36"
          document: "docs/platform/ios-device-validation.md"
          test: "App Store Connect / TestFlight distribution pipeline"
          verified_at: "2026-08-24T23:30:00Z"
      release:
        status: pending
        evidence:
          commit: "62132e78"
          document: "docs/Caesura_AmeKAG_Agent_Pack/05_RELEASE_CANDIDATE.md"
          test: "App Store submission gate"
          verified_at: "2026-08-25T01:57:33Z"
```

---

## 3. Architecture for `scripts/generate_platform_status.py`

### 3.1 Design Principles & Requirements
1. **Zero Third-Party Dependencies**: Uses standard Python 3 (`json`, `re`, `os`, `sys`, `pathlib`, `argparse`, `datetime`). If PyYAML is available, it is utilized; otherwise, a robust YAML subset parser / validator is included so it executes seamlessly on Windows, Linux, and macOS without extra `pip install`.
2. **Strict Schema & Consistency Validation**:
   - Every platform capability status must belong to `allowed_status_enums`.
   - Every `verified` status must have non-empty `evidence.commit`, `evidence.document`, `evidence.test`, and `evidence.verified_at`.
   - Document paths referenced in evidence must exist on disk.
   - Commit SHAs must match valid hexadecimal string format (7 to 40 characters).
   - Any `hardware-gated` or `credential-gated` entry must document its specific gating conditions.
3. **Deterministic Markdown Generation**:
   - Generates `docs/status/platform-status.md` with:
     - Header block and last updated metadata.
     - Global Platform Status Summary Table with unicode status badges (`● Verified`, `◐ Probe`, `⏳ Pending`, `🔒 Hardware-gated`, `🔑 Credential-gated`).
     - Capability breakdown across all 6 target platforms.
     - Concrete evidence anchor index with clickable file paths and runnable test commands.
     - Clear Hardware & Credential gating checklists for release managers.
4. **CI Freshness Enforcement (`--check` mode)**:
   - Compares existing `docs/status/platform-status.md` with newly generated output. If diff is detected, exits with non-zero code (1), preventing silent documentation drift in GitHub Actions CI.
5. **Machine-Readable Export (`--format json`)**:
   - Can emit `artifacts/release/platform-status.json` for consumption by the Release Candidate bundler (Task 05 / R5).

---

### 3.2 Component Architecture of the Generator

```
┌─────────────────────────────────────────────────────────────┐
│              scripts/generate_platform_status.py            │
├─────────────────────────────────────────────────────────────┤
│ 1. CLI Handler (argparse: --check, --write, --format, etc.) │
│ 2. YAML Loader & Parser (PyYAML or stdlib YAML reader)      │
│ 3. Schema Validator (Enums, Commit SHA, Docs exist, Ev)     │
│ 4. Markdown Generator (docs/status/platform-status.md)       │
│ 5. JSON Generator (artifacts/release/platform-status.json)  │
│ 6. Freshness Comparator (git diff / byte compare)           │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Comprehensive Repository Evidence Catalog

The following table enumerates all concrete test commands, evidence documents, and commit references currently in the repository for each target platform:

| Platform | Capability | Status | Evidence Document | Test Command | Commit SHA |
|---|---|---|---|---|---|
| **Windows** | Build | `verified` | `README.md` | `cmake --build build --config Debug --parallel` | `62132e78` |
| **Windows** | Runtime | `verified` | `docs/plans/2026-08-24-027-antigravity-handoff.md` | `build/tests/Debug/CaesuraTests.exe` | `62132e78` |
| **Windows** | Lua Suites | `verified` | `docs/plans/2026-08-24-027-antigravity-handoff.md` | `external/lua/lua.exe tests/scripts/run_lua_tests.lua` | `62132e78` |
| **Windows** | First-VN | `verified` | `scripts/verify_first_vn.sh` | `bash scripts/verify_first_vn.sh` (13/13 PASS) | `62132e78` |
| **Windows** | Golden Project | `verified` | `scripts/verify_golden_vn.sh` | `bash scripts/verify_golden_vn.sh` (18/18 PASS) | `62132e78` |
| **Windows** | Packaging | `verified` | `docs/plans/audit/ROADMAP-200.md` | `cd build && cpack -C Release -G ZIP` | `62132e78` |
| **Linux** | Build | `verified` | `docs/plans/2026-08-22-025-delivery-handoff.md` | `cmake --build build -j$(nproc)` | `806275cf` |
| **Linux** | Runtime | `verified` | `docs/plans/2026-08-22-025-delivery-handoff.md` | `ctest --test-dir build --output-on-failure` (11/11 PASS) | `806275cf` |
| **Linux** | Bundle Boot | `verified` | `scripts/verify_bundle_boot.sh` | `xvfb-run -a bash scripts/verify_bundle_boot.sh build/CaesuraAmeKAG first_vn` | `806275cf` |
| **Web** | Build | `verified` | `docs/status/web-release-status.md` | `cd web && npm run build` | `6e90d7df` |
| **Web** | Runtime | `verified` | `docs/status/web-release-status.md` | `cd web && npm test` (318/318 PASS) | `6e90d7df` |
| **Web** | Browser Smoke | `verified` | `scripts/web_browser_smoke.mjs` | `node scripts/web_browser_smoke.mjs --root dist/first_vn --unlock` | `6e90d7df` |
| **Web** | CJK Smoke | `verified` | `scripts/web_browser_smoke.mjs` | `node scripts/web_browser_smoke.mjs --root dist/cjk_smoke --cjk` | `6e90d7df` |
| **Web** | Stress | `verified` | `scripts/web_browser_smoke.mjs` | `node scripts/web_browser_smoke.mjs --root dist/web_stress_vn --stress` | `6e90d7df` |
| **Android** | Build | `verified` | `.github/workflows/ci.yml` | `scripts/build_android.sh --release --abi arm64-v8a` | `1f054039` |
| **Android** | Real Device | `verified` | `docs/platform/android-device-validation.md` | `bash scripts/android_device_smoke.sh` (Xiaomi 11) | `1f054039` |
| **Android** | Signing | `verified` | `docs/platform/android-release-signing.md` | `apksigner verify --verbose android/.../app-release.apk` | `8aa51c36` |
| **Android** | AAB | `verified` | `docs/platform/android-release-signing.md` | `cd android && gradle bundleRelease` | `8aa51c36` |
| **macOS** | Build | `probe` | `.github/workflows/ci.yml` | `cmake --build build --config Debug` (CI macos-latest) | `62132e78` |
| **macOS** | Device | `hardware-gated`| `docs/guides/release-qa-matrix.md` | Physical Apple Silicon Mac required | `62132e78` |
| **iOS** | Build | `probe` | `docs/platform/ios-build-and-validation.md`| `cmake --build build-ios --config Release` (CI probe) | `8aa51c36` |
| **iOS** | Metal Shaders | `probe` | `scripts/verify_metal_shaders.py` | `python3 scripts/verify_metal_shaders.py` (12/12 PASS) | `8aa51c36` |
| **iOS** | Device | `hardware-gated`| `docs/platform/ios-device-validation.md` | Physical iPhone/iPad + Mac Xcode required | `8aa51c36` |
| **iOS** | Signing | `credential-gated`| `docs/platform/ios-build-and-validation.md`| Apple Developer Certificate required | `8aa51c36` |

---

## 5. Summary & Implementation Roadmap for Implementer

1. **Step 1**: Implement `docs/status/platform-matrix.yaml` according to the exact schema defined in Section 2.
2. **Step 2**: Create `scripts/generate_platform_status.py` implementing validation, Markdown generation, JSON export, and `--check` CLI functionality.
3. **Step 3**: Generate initial `docs/status/platform-status.md` and verify zero schema errors.
4. **Step 4**: Integrate `python scripts/generate_platform_status.py --check` into `.github/workflows/ci.yml` under the "Generated docs freshness" step.
5. **Step 5**: Align references in `README.md` and `docs/plans/audit/ROADMAP-200.md` to reference `docs/status/platform-matrix.yaml` as the sole authoritative status source.
