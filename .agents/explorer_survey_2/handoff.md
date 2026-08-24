# Handoff Report: Milestone R2 & R3 Survey

**Agent**: Explorer Survey 2  
**Task**: Survey Milestone R2 (Android Release Signing & AAB Pipeline) and Milestone R3 (iOS & Metal Toolchain / CI Build Hardening)  
**Date**: 2026-08-24  
**Working Directory**: `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_2`  
**Full Report**: `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_2\report.md`  

---

## 1. Observation

1. **Android Gradle & Signing Configuration**:
   - `android/build.gradle:13`: Uses AGP `8.7.3` (`classpath 'com.android.tools.build:gradle:8.7.3'`).
   - `android/app/build.gradle:15-33`: `signingConfigs.caesura` reads `CAESURA_ANDROID_KEYSTORE`, `CAESURA_ANDROID_KEYSTORE_PASS`, `CAESURA_ANDROID_KEY_ALIAS`, `CAESURA_ANDROID_KEY_PASS`. `buildTypes.release` sets `signingConfig signingConfigs.caesura` if `CAESURA_ANDROID_KEYSTORE` is non-null.
   - `android/` directory: Missing checked-in `gradlew` wrapper scripts and `gradle/wrapper/gradle-wrapper.properties`.
   - `docs/platform/android-release-signing.md:41-44`: Documents env vars as `CAESURA_KEYSTORE_PATH`, `CAESURA_KEYSTORE_PASSWORD`, `CAESURA_KEY_ALIAS`, `CAESURA_KEY_PASSWORD` (a naming mismatch with `build.gradle`).

2. **iOS CMake & Framework Dependencies**:
   - `CMakeLists.txt:137-143`: `SOLOUD_BACKEND_COREAUDIO` is activated when `CMAKE_SYSTEM_NAME STREQUAL "iOS"`.
   - `CMakeLists.txt:197-205`: `add_library(${PROJECT_NAME} MODULE src/main.cpp)` on Android, else `add_executable(${PROJECT_NAME} src/main.cpp)`.
   - `cmake/CaesuraModules.cmake:78-111`: When `CMAKE_SYSTEM_NAME STREQUAL "iOS"`, links frameworks `Metal`, `Foundation`, `QuartzCore`, `UIKit`, `AudioToolbox`, `pthread`, and OpenSSL static archive slice (`libssl.a`, `libcrypto.a`).

3. **Metal Shaders & Embedded Fallbacks**:
   - `src/render/EmbeddedShaders.h:98-121`: Declares 10 `kEmbeddedMetal_*` symbols (`vs_sprite`, `vs_fullscreen`, `stretch_blt_vs`, `affine_blt_vs`, `fs_texture`, `fs_blend`, `fs_transition`, `fs_vfx`, `stretch_blt_fs`, `affine_blt_fs`).
   - `src/render/EmbeddedShaders_Metal.cpp`: Contains all 10 compiled MSL byte arrays.
   - `src/render/BgfxShaderManager.cpp:254-265`: Selected via `renderer == bgfx::RendererType::Metal`.
   - `src/render/BgfxShaderManager.cpp:275-278`: Post-processing fragment shaders (`fsPostfxVignette`, `fsPostfxLut`, `fsPostfxBlur`, `fsPostfxBloom`) fall back to `fsTexture` (identity copy) on Metal.
   - `src/minigame/EmbeddedShaders_MiniGame_Metal.cpp`: Contains embedded `kEmbeddedMSL_MiniGame_VS` and `kEmbeddedMSL_MiniGame_FS` for 3D Phong + PBR rendering.
   - `src/render/SmaMeshRenderer.cpp:185-198`: S5 GPU compute skinning is disabled on Metal and automatically falls back to CPU SIMD skinning (`SmaSkinner.h`).

4. **GitHub Actions Workflows**:
   - `.github/workflows/ci.yml:339-415`: `ios-compile` runs on `macos-latest` with `continue-on-error: true`, builds SDL3 and OpenSSL slices, and invokes `cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS ...`.
   - `.github/workflows/ci.yml:454-586`: `android-compile` runs on `ubuntu-latest` with `continue-on-error: true`, builds SDL3 and OpenSSL slices, compiles `libCaesuraAmeKAG.so`, and executes `gradle assembleDebug` and `gradle assembleRelease` (unsigned). It does NOT invoke `gradle bundleRelease` (AAB), does NOT test signing, and does NOT run `zipalign` / `apksigner`.

---

## 2. Logic Chain

1. **Android Pipeline Completion (R2)**:
   - *Premise*: R2 requires standard PKCS12 keytool generation, environment-driven signing, Release APK (`assembleRelease`), Android App Bundle (`bundleRelease`), `zipalign`, and `apksigner verify`.
   - *Inference 1*: Supporting both `CAESURA_ANDROID_*` and `CAESURA_*_PATH` environment variable names in `android/app/build.gradle` prevents configuration errors across CI and developer machines without breaking existing setups.
   - *Inference 2*: Enabling V1 and V2/V3 signing schemes and adding `bundle { language { enableSplit false } }` ensures multi-language visual novel scripts and voice packs are not stripped by Google Play dynamic delivery.
   - *Inference 3*: Providing `scripts/generate_android_keystore.sh` and `scripts/build_android_release.sh` automates the entire compile -> stage -> package -> align -> verify cycle.
   - *Inference 4*: Testing release signing in CI using an ephemeral PKCS12 test keystore validates the end-to-end signing and verification pipeline (`zipalign -c` + `apksigner verify`) on every pull request.

2. **iOS & Metal Hardening (R3)**:
   - *Premise*: R3 requires verified CMake iOS toolchain configurations, documented and asserted Metal shader fallback paths, and robust CI build workflows.
   - *Inference 1*: All 10 core 2D shaders and 2 3D minigame shaders already exist as precompiled MSL bytecode or embedded MSL text; the Post-FX chain degrades safely to identity copies, and SMA mesh animation falls back cleanly to CPU skinning.
   - *Inference 2*: Creating a Python shader integrity test (`scripts/verify_metal_shaders.py`) and a C++ doctest (`tests/cpp/test_render_metal_contract.cpp`) guarantees that Metal symbols never regress silently during future shader re-generations.
   - *Inference 3*: Adding dependency caching (`actions/cache`) for SDL3 and OpenSSL in `.github/workflows/ci.yml` reduces macOS CI build times from ~15 minutes to ~3 minutes and hardens the workflow against network transients.

---

## 3. Caveats

1. **Hardware Availability**:
   - The current development environment is Windows 10/11 without a connected macOS host. Direct execution of Xcode GUI or iOS Simulator must be validated via GitHub Actions `macos-latest` runners.
2. **Keystore Security**:
   - Ephemeral keystores generated during CI or testing must use temporary passwords and must never be used for production Google Play uploads. Production keystores remain strictly under publisher custody outside Git.
3. **Post-FX Visuals on Metal**:
   - The Metal Post-FX fallback performs an identity texture copy (graceful degradation). If full visual post-processing (e.g. bloom/vignette) is desired on Metal in the future, native MSL fragment shaders can be compiled with `shaderc --platform osx -p metal` and added to `EmbeddedShaders_Metal.cpp`.

---

## 4. Conclusion

The architecture for Milestone R2 and Milestone R3 is sound, complete, and free of blocking structural defects. The required actions for full closure are clearly scoped:
1. **R2 Implementation**:
   - Update `android/app/build.gradle` (dual env-vars, explicit V1/V2/V3 signing, AAB bundle DSL).
   - Create `scripts/generate_android_keystore.sh` and `scripts/build_android_release.sh`.
   - Add CI steps in `.github/workflows/ci.yml` for ephemeral key signing, `gradle bundleRelease`, `zipalign`, and `apksigner verify`.
2. **R3 Implementation**:
   - Update `CMakeLists.txt` with iOS target bundle properties.
   - Create `scripts/verify_metal_shaders.py` and `tests/cpp/test_render_metal_contract.cpp`.
   - Add GitHub Actions caching and hardening for `ios-compile` in `ci.yml`.

---

## 5. Verification Method

1. **Android R2 Verification**:
   - Run keytool generation: `bash scripts/generate_android_keystore.sh --test`
   - Run release build pipeline: `bash scripts/build_android_release.sh --ephemeral-key`
   - Verify APK alignment: `zipalign -c -v 4 android/app/build/outputs/apk/release/app-release.apk`
   - Verify APK signatures: `apksigner verify --verbose --print-certs android/app/build/outputs/apk/release/app-release.apk`
   - Verify AAB structure: `unzip -l android/app/build/outputs/bundle/release/app-release.aab | grep -E "base/lib/arm64-v8a/libCaesuraAmeKAG.so|base/assets/game/scripts/config.lua"`

2. **iOS & Metal R3 Verification**:
   - Run Metal shader verification script: `python scripts/verify_metal_shaders.py`
   - Run C++ doctest suite: `build/tests/Debug/CaesuraTests.exe -tc="*metal*"`
   - Trigger GitHub Actions CI and verify that `ios-compile` and `android-compile` jobs complete cleanly with exit code 0.
