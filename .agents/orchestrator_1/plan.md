# Orchestration Plan — Caesura (AmeKAG) Platform & Runtime Sprint

## Objectives
Execute the engineering sprint to complete the 4 milestones specified in `ORIGINAL_REQUEST.md`:
1. **Milestone R1**: IME Virtual Keyboard & Text Input Component (Track IME)
   - IPlatformBackend / SDL3PlatformBackend text input methods (`startTextInput`, `stopTextInput`, `setTextInputRect`, `isTextInputActive`)
   - Route SDL3 text events through InputRouter to Lua bindings
   - KAG `[input]` command and text box UI component with viewport offset
   - C++ unit tests & Lua tests
2. **Milestone R2**: Android Release Signing & AAB Packaging Pipeline (Track A5)
   - Standard release key generation script/config via PKCS12 keytool
   - Update `android/app/build.gradle` for environment-driven `signingConfigs`
   - Build & verify Release APK (`assembleRelease`) and AAB (`bundleRelease`)
   - Run zipalign and apksigner verification
3. **Milestone R3**: iOS & Metal Toolchain / CI Build Hardening (Track I)
   - CMake iOS toolchain & Metal shader compilation pipeline
   - Verify iOS compilation targets & CI workflow robustness
   - Metal embedded shader fallback paths
4. **Milestone R4**: Live2D, 3D Minigame & Post-FX Mobile Stress Validation & Engine Baseline QA
   - Mobile / GLES stress validation for Live2D and 3D minigame lifecycle
   - Post-processing shader degradation without GPU stalls
   - 100% doctest pass (0 failed, 0 skipped), 100% Lua test pass, architecture coupling check pass (`python scripts/count_coupling.py`)

## Phased Execution Strategy
- **Phase 0**: Survey (3 Explorers in parallel for R1, R2+R3, R4+Baseline) -> Generate PROJECT.md & FEATURE_INVENTORY.md
- **Phase 1**: Execute Milestone R1 (IME Virtual Keyboard & Text Input Component)
  - Iteration loop: Explorer -> Worker -> Reviewers (2) -> Challengers (2) -> Auditor -> Gate
- **Phase 2**: Execute Milestone R2 (Android Release Signing & AAB Packaging)
  - Iteration loop: Explorer -> Worker -> Reviewers (2) -> Challengers (2) -> Auditor -> Gate
- **Phase 3**: Execute Milestone R3 (iOS & Metal Toolchain / CI Hardening)
  - Iteration loop: Explorer -> Worker -> Reviewers (2) -> Challengers (2) -> Auditor -> Gate
- **Phase 4**: Execute Milestone R4 & Full Baseline QA Verification
  - Iteration loop: Explorer -> Worker -> Reviewers (2) -> Challengers (2) -> Auditor -> Gate
- **Phase 5**: Final Acceptance Verification & Victory Claim Handoff to Parent Sentinel
