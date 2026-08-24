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
