# Original User Request

## 2026-08-24T15:11:30Z

Engineering sprint to complete the four remaining platform and runtime milestones for Caesura (AmeKAG) visual novel engine across Android, iOS, and core runtime capabilities.

Working directory: d:/文件存放处/code/Caesura(AmeKAG)
Integrity mode: development

## Requirements

### R1. IME Virtual Keyboard & Text Input Component (Track IME)
- Implement IPlatformBackend / SDL3PlatformBackend text input methods (startTextInput(), stopTextInput(), setTextInputRect(), isTextInputActive()).
- Route SDL3 SDL_EVENT_TEXT_INPUT and SDL_EVENT_TEXT_EDITING through InputRouter to Lua bindings.
- Implement the KAG [input] command and text box UI component with viewport offset to avoid virtual keyboard occlusion.
- Provide unit tests and headless/mock test coverage.

### R2. Android Release Signing & AAB Packaging Pipeline (Track A5)
- Provide standard release key generation script/configuration via PKCS12 keytool.
- Update ndroid/app/build.gradle to support environment-variable-driven signingConfigs without hardcoding credentials.
- Build and verify both Release APK (ssembleRelease) and Android App Bundle (undleRelease).
- Perform zipalign and pksigner verify validation on the output artifacts.

### R3. iOS & Metal Toolchain / CI Build Hardening (Track I)
- Refine CMake iOS toolchain (-G Xcode -DCMAKE_SYSTEM_NAME=iOS) and Metal shader compilation pipeline.
- Verify iOS compilation targets and ensure GitHub Actions ios-compile CI workflow robustness.
- Document and assert all Metal embedded shader fallback paths.

### R4. Live2D, 3D Minigame & Post-FX Mobile Stress Validation
- Verify Live2D animation backend and SMA 3D mesh rendering lifecycle on mobile / GLES targets.
- Ensure post-processing shaders (bloom, vignette, LUT, softblur) run or degrade cleanly without GPU pipeline stalls.
- Run engine benchmark and scale stress test suites, verifying zero regressions across all 16 modules.

## Acceptance Criteria

### IME Text Input
- [ ] IPlatformBackend text input pure virtual methods implemented without violating AGENTS.md module boundaries.
- [ ] KAG [input] command captures text input and writes to declared variable (e.g. .player_name).
- [ ] C++ unit tests in 	ests/cpp/ and Lua tests pass with zero failures.

### Android Release
- [ ] ./gradlew assembleRelease and ./gradlew bundleRelease execute cleanly.
- [ ] Generated APK/AAB passes pksigner V2/V3 verification.
- [ ] No plaintext secrets or passwords stored in git repository files.

### iOS Toolchain & CI
- [ ] CMake configuration generates valid iOS Xcode project definitions.
- [ ] Metal shader embedder / compile path verified without build errors.

### Stress Testing & Zero-Regression Baseline
- [ ] C++ doctest suite: 100% passed, 0 failed, 0 skipped.
- [ ] Lua full test suite: 100% passed, 0 failed.
- [ ] Architecture coupling script (python scripts/count_coupling.py) passes all 16 module limits.

## 2026-08-24T18:16:03Z

Engineering sprint to advance Caesura (AmeKAG) to a fully verified, evidence-backed 1.x Release Candidate (RC) with unified cross-platform status matrix, First-VN parity verification, latest HEAD Android regression, iOS hardware-gated documentation, and complete release evidence bundle based on docs/Caesura_AmeKAG_Agent_Pack and docs/plans/Caesura_AmeKAG_Agent_Prompt.md.

Working directory: d:/文件存放处/code/Caesura(AmeKAG)
Integrity mode: development

## Requirements

### R1. Unified Platform Status Matrix & Generator (Task 01)
- Create `docs/status/platform-matrix.yaml` tracking all 6 target platforms (Windows, Linux, Web, Android, macOS, iOS) with strictly defined status enums (`verified`, `probe`, `pending`, `hardware-gated`, `credential-gated`) and evidence tracking (commit SHA, document path, test command, verified timestamp).
- Create `scripts/generate_platform_status.py` to auto-generate `docs/status/platform-status.md` and keep README/ROADMAP synchronized without status drift.
- Add schema validation and CI check for platform status.

### R2. First-VN Cross-Platform Behavioral Parity (Task 02)
- Validate `tests/projects/first_vn/` progression, branching, saving/loading, i18n (zh/en/ja), audio, and input across Windows, Linux, Web, and Android.
- Create lightweight `FirstVNStateSnapshot` JSON files (`artifacts/parity/<platform>.json`) recording label, choices, `flag_is_sun`, language, save roundtrip, and ending.
- Create `scripts/compare_platform_parity.py` to assert cross-platform state parity (`desktop == web == android`).

### R3. Android Latest HEAD Real-Device Regression (Task 03)
- Validate latest commit HEAD on real Android device (Xiaomi 11 / alioth / Snapdragon 888 / Adreno 660 / Android 14) or device test suite.
- Re-verify boot, multi-texture rendering, FreeType RGBA8 CJK font atlas, touch coordinates, save persistence, and IME virtual keyboard without relying solely on historical closure documents.
- Produce `docs/platform/android-latest-head-validation.md` with SHA256 hashes, device info, command logs, and test evidence.

### R4. iOS Real-Device Track & Hardware Gate Audit (Task 04)
- Audit Track I (I0–I6) build, Metal shaders, lifecycle, audio session, and packaging.
- Create `docs/platform/ios-device-validation.md` documenting prerequisite matrix, Xcode build procedures, Metal shader validations, and explicit `hardware-gated` boundary markers.

### R5. Release Candidate Gate & Evidence Bundle (Task 05)
- Assemble `artifacts/release/` bundle containing `manifest.json`, `platform-status.json`, `parity/`, `checksums/`, and `reports/`.
- Enforce release blocker rules (0 crashes, 0 save corruptions, 100% C++ doctests, 100% Lua tests, 16/16 module coupling pass).
- Produce authoritative `docs/status/release-candidate-report.md` declaring final `RC-GO` decision.

## Acceptance Criteria

### Platform Matrix
- [ ] `docs/status/platform-matrix.yaml` exists with valid schema and zero undocumented claims.
- [ ] `python scripts/generate_platform_status.py` generates clean markdown without errors.

### Cross-Platform Parity
- [ ] `artifacts/parity/` contains valid parity snapshots for Windows, Linux, Web, and Android.
- [ ] `python scripts/compare_platform_parity.py` validates consistent progression and choice outcome across platforms.

### Android Latest HEAD
- [ ] `docs/platform/android-latest-head-validation.md` provides current commit verification evidence.
- [ ] No regression in CJK rendering, multi-texture quad batching, save persistence, or touch coordinates.

### iOS Hardware Gate
- [ ] `docs/platform/ios-device-validation.md` honestly documents Xcode/Metal status and hardware-gated requirements.

### Release Candidate
- [ ] Full baseline test suites pass: C++ 1052+ doctest cases (0 failed), Lua 158 suites (0 failed), 16/16 coupling limits pass.
- [ ] `docs/status/release-candidate-report.md` generated with definitive `RC-GO` status.

