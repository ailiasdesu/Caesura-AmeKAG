# Project: Caesura (AmeKAG) 1.x Release Candidate

## Architecture
Caesura (AmeKAG) is a cross-platform Visual Novel engine with a strict 16-module modular architecture.
Module boundaries are enforced via `src/<module>/api/I<ModuleName>.h`. Direct implementation inclusion between modules is forbidden.
All backends are registered and accessed via `BackendRegistry`. `src/main.cpp` and `src/entry/` form the sole composition root.

### Data Flow & Release Candidate Architecture
- **Unified Status Source**: `docs/status/platform-matrix.yaml` acts as the single machine-readable source of truth across all 6 target platforms (Windows, Linux, Web, Android, macOS, iOS).
- **Generator & CI Validation**: `scripts/generate_platform_status.py` generates `docs/status/platform-status.md`, validates YAML schema, and prevents status drift.
- **Cross-Platform Behavioral Parity**: `tests/projects/first_vn/` is executed across all platforms. Standard `FirstVNStateSnapshot` JSONs in `artifacts/parity/` record deterministic game progression and choice outcomes, asserted by `scripts/compare_platform_parity.py`.
- **Platform Verification & Hardware Gates**: Android latest HEAD (`62132e78`) verified in `docs/platform/android-latest-head-validation.md`. iOS toolchain/Metal verified and marked `hardware-gated` in `docs/platform/ios-device-validation.md`.
- **Release Evidence Bundle**: `artifacts/release/` contains `manifest.json`, `platform-status.json`, parity reports, checksums, test logs, and the final `RC-GO` decision in `docs/status/release-candidate-report.md`.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Platform Matrix YAML Schema | `docs/status/platform-matrix.yaml` tracking 6 platforms with 7 strict enums and evidence anchors | M1 (Task 01) | Survey 1 [DONE] |
| 2 | Platform Status Generator Script | `scripts/generate_platform_status.py` with markdown generation, JSON export, and `--check` CI freshness guard | M1 (Task 01) | Survey 1 [DONE] |
| 3 | Generated Platform Status Markdown | `docs/status/platform-status.md` auto-generated from YAML with zero document drift | M1 (Task 01) | Survey 1 [DONE] |
| 4 | First-VN Fixture & Test Verification | Validate `tests/projects/first_vn/` progression, branching, variables, save/load, i18n, and audio across platforms | M2 (Task 02) | Survey 2 [DONE] |
| 5 | FirstVNStateSnapshot Parity JSONs | `artifacts/parity/<platform>.json` snapshots for Windows, Linux, Web, Android, and iOS (hardware-gated) | M2 (Task 02) | Survey 2 [DONE] |
| 6 | Cross-Platform Parity Assertion Tool | `scripts/compare_platform_parity.py` asserting `desktop == web == android == ios` | M2 (Task 02) | Survey 2 [DONE] |
| 7 | Android Latest HEAD Validation | Verify latest commit HEAD (`62132e78`) on Xiaomi 11 / test harness across boot, CJK RGBA8 atlas, touch, save, and IME | M3 (Task 03) | Survey 3 [DONE] |
| 8 | Android Latest HEAD Report | `docs/platform/android-latest-head-validation.md` documenting SHA256 hashes, device info, logs, and test evidence | M3 (Task 03) | Survey 3 [DONE] |
| 9 | iOS Toolchain & Metal Shader Audit | Audit Xcode build, 12 embedded Metal shaders, Post-FX identity fallback, and SMA CPU soft-skinning | M4 (Task 04) | Survey 3 [DONE] |
| 10 | iOS Device Validation & Hardware Gate Report | `docs/platform/ios-device-validation.md` with prerequisite matrix, build procedures, and explicit `hardware-gated` boundary | M4 (Task 04) | Survey 3 [DONE] |
| 11 | Release Evidence Bundle Assembly | `artifacts/release/` containing `manifest.json`, `platform-status.json`, `parity/`, `checksums/`, and `reports/` | M5 (Task 05) | Survey 3 [DONE] |
| 12 | Zero-Regression Baseline Test Pass | 100% C++ doctests (1052 passed), 100% Lua suites (158 passed), 16/16 module coupling pass, 100% coverage pass | M5 (Task 05) | Survey 3 [DONE] |
| 13 | Authoritative Release Candidate Report | `docs/status/release-candidate-report.md` declaring definitive `RC-GO` status with release blocker checklist | M5 (Task 05) | Survey 3 [DONE] |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Unified Platform Status Matrix & Generator | Features 1-3: `platform-matrix.yaml`, `generate_platform_status.py`, `platform-status.md` | none | DONE |
| M2 | First-VN Cross-Platform Behavioral Parity | Features 4-6: `FirstVNStateSnapshot`, `artifacts/parity/`, `compare_platform_parity.py` | M1 | DONE |
| M3 | Android Latest HEAD Real-Device Regression | Features 7-8: Android latest HEAD verification, `android-latest-head-validation.md` | M1 | DONE |
| M4 | iOS Real-Device Track & Hardware Gate Audit | Features 9-10: iOS toolchain audit, Metal shader validation, `ios-device-validation.md` | M1 | DONE |
| M5 | Release Candidate Gate & Evidence Bundle | Features 11-13: `artifacts/release/`, baseline test passes, `release-candidate-report.md` (`RC-GO`) | M1, M2, M3, M4 | DONE |

## Interface Contracts
### Status Matrix Enum Contract
Allowed status values in `docs/status/platform-matrix.yaml`:
- `verified`: Tested and verified with concrete evidence (commit, document, test, timestamp).
- `probe`: Basic build or compile probe verified in CI.
- `pending`: Feature or platform under active development.
- `hardware-gated`: Requires physical hardware (e.g., Apple Mac/iPhone) not currently connected.
- `credential-gated`: Requires production credentials/signing keys.
- `blocked`: Blocked by external dependency or known P0 bug.
- `not-applicable`: Dimension does not apply to this platform.

### FirstVNStateSnapshot Schema Contract (`artifacts/parity/<platform>.json`)
```json
{
  "platform": "windows|linux|web|android|ios",
  "story": "first_vn",
  "status": "verified|hardware-gated",
  "route_a": {
    "choice": "sun",
    "route": "sun",
    "flag_is_sun": 1,
    "final_label": "*ending",
    "ending": "sunset",
    "save_roundtrip": true,
    "languages": ["zh", "en", "ja"]
  },
  "route_b": {
    "choice": "rain",
    "route": "rain",
    "flag_is_sun": 0,
    "final_label": "*ending",
    "ending": "rainy_day",
    "save_roundtrip": true,
    "languages": ["zh", "en", "ja"]
  }
}
```

## Code Layout
- `docs/status/platform-matrix.yaml`: Master platform status YAML definition
- `scripts/generate_platform_status.py`: Platform status markdown generator and schema validator
- `docs/status/platform-status.md`: Auto-generated human-readable platform status document
- `tests/projects/first_vn/`: Standard acceptance test visual novel project
- `artifacts/parity/`: Cross-platform state parity snapshots
- `scripts/compare_platform_parity.py`: Parity comparison and validation tool
- `docs/platform/cross-platform-parity.md`: Cross-platform parity architecture and verification guide
- `docs/platform/android-latest-head-validation.md`: Latest HEAD Android regression documentation
- `scripts/verify_android_regression.py`: Automated Android latest HEAD regression verifier
- `docs/platform/ios-device-validation.md`: iOS toolchain, Metal shaders, and hardware-gated audit documentation
- `artifacts/release/`: Release candidate evidence bundle (`manifest.json`, `checksums/`, `reports/`)
- `scripts/verify_release_candidate.py`: Master release candidate verification tool
- `docs/status/release-candidate-report.md`: Master 1.x Release Candidate report with `RC-GO` decision
