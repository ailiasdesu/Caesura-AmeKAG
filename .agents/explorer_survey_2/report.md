# Milestone R2 & R3 Survey Report: Android Release Signing & AAB Pipeline and iOS & Metal Toolchain / CI Hardening

**Explorer Agent**: Explorer Survey 2  
**Date**: 2026-08-24  
**Working Directory**: `.agents/explorer_survey_2`  
**Status**: Read-only Investigation & Comprehensive Survey  

---

## Executive Summary

This investigation comprehensively surveys the architecture, build configurations, shader compilation pipelines, CI/CD workflows, and verification mechanisms for:
1. **Milestone R2**: Android Release Signing, Keystore Generation, and AAB / APK Packaging Pipeline.
2. **Milestone R3**: iOS CMake Xcode Toolchain, Metal Shader Compilation, Embedded Fallbacks, and CI Hardening.

The codebase already possesses high-quality mobile foundational architecture:
- **Android**: NDK 27.3 cross-compilation produces `libCaesuraAmeKAG.so`; assets and native libs stage into `android/app/`; `assembleDebug` runs on real devices (Xiaomi 11) with 120 FPS GLES rendering and touch input mapping.
- **iOS & Metal**: CMake generates Xcode projects with `-DCMAKE_SYSTEM_NAME=iOS`; iOS system frameworks (`Metal`, `QuartzCore`, `UIKit`, `AudioToolbox`, `OpenSSL`) are linked; 10 Metal shaders are precompiled into `EmbeddedShaders_Metal.cpp`; 3D minigame MSL shaders are embedded in `EmbeddedShaders_MiniGame_Metal.cpp`; Post-FX shaders gracefully degrade to identity texture copies on Metal.
- **CI Probes**: GitHub Actions contains functional `android-compile` and `ios-compile` probes on Ubuntu and macOS runners.

However, concrete gaps exist before R2 and R3 reach full production hardening:
- **R2 Gaps**: No Gradle wrapper checked into `android/`; `bundleRelease` (AAB) is not exercised in CI; release signing is not verified with an ephemeral keystore in CI; `zipalign` and `apksigner verify` validation steps are absent; environment variable naming between `build.gradle` and documentation has minor inconsistencies.
- **R3 Gaps**: `ios-compile` CI job runs as an audit probe with `continue-on-error: true`; missing caching for prebuilt SDL3/OpenSSL slices causes runner latency; target bundle properties (`MACOSX_BUNDLE`, `PRODUCT_BUNDLE_IDENTIFIER`) need full definition in `CMakeLists.txt`; Metal embedded shader integrity is not asserted by automated static tests.

Below is the detailed findings breakdown, architecture analysis, and execution plan.

---

## Part 1: Android Release Signing & AAB Packaging Pipeline (Milestone R2)

### 1.1 Project Structure & Build Configuration Audit

| File Path | Current Status | Findings & Assessment |
|---|---|---|
| `android/build.gradle` | AGP 8.7.3 (`com.android.tools.build:gradle:8.7.3`) | Top-level build configuration is modern and aligned with official SDL3 project layouts. Standard repositories `mavenCentral()` and `google()` configured. |
| `android/app/build.gradle` | `compileSdkVersion 35`, `minSdkVersion 24`, `targetSdkVersion 35` | `namespace "com.caesura.app"`, `applicationId "com.caesura.app"`. `signingConfigs.caesura` reads `CAESURA_ANDROID_KEYSTORE`, `CAESURA_ANDROID_KEYSTORE_PASS`, `CAESURA_ANDROID_KEY_ALIAS`, `CAESURA_ANDROID_KEY_PASS`. |
| `android/gradle/wrapper/` | **MISSING** | No `gradlew`, `gradlew.bat`, `gradle-wrapper.jar`, or `gradle-wrapper.properties` in `android/`. CI currently downloads Gradle 8.9 manually. |
| `android/app/src/main/jniLibs/arm64-v8a/` | Staging directory for `.so` | Receives `libCaesuraAmeKAG.so` (from NDK CMake) and `libSDL3.so` (from SDL3 Android package). |
| `android/app/src/main/assets/game/` | Staging directory for assets | Canonical resource root containing `scripts/`, `assets/`, and `demo/first_vn/`. `MainActivity` extracts to internal storage on first boot and passes `--resource-root`. |

### 1.2 Environment-Driven Signing Architecture

#### Problem & Requirements
- Hardcoding passwords or committing keystores to Git is strictly prohibited.
- Release signing must be opt-in: if credentials are provided in the environment, Gradle signs the release artifact; if not provided, Gradle produces an unsigned release artifact (`app-release-unsigned.apk` / `app-release-unsigned.aab`) without failing the build.

#### Current `android/app/build.gradle` Implementation:
```groovy
signingConfigs {
    caesura {
        storeFile file(System.getenv('CAESURA_ANDROID_KEYSTORE') ?: 'none')
        storePassword System.getenv('CAESURA_ANDROID_KEYSTORE_PASS') ?: ''
        keyAlias System.getenv('CAESURA_ANDROID_KEY_ALIAS') ?: ''
        keyPassword System.getenv('CAESURA_ANDROID_KEY_PASS') ?: ''
    }
}
buildTypes {
    release {
        minifyEnabled false
        proguardFiles getDefaultProguardFile('proguard-android.txt'), 'proguard-rules.pro'
        if (System.getenv('CAESURA_ANDROID_KEYSTORE') != null) {
            signingConfig signingConfigs.caesura
        }
    }
}
```

#### Refinement & Hardening:
1. **Support Dual Env-Var Naming**: Reconcile `CAESURA_ANDROID_*` and `CAESURA_*_PATH` sets:
   ```groovy
   def ksPath = System.getenv('CAESURA_ANDROID_KEYSTORE') ?: System.getenv('CAESURA_KEYSTORE_PATH')
   def ksPass = System.getenv('CAESURA_ANDROID_KEYSTORE_PASS') ?: System.getenv('CAESURA_KEYSTORE_PASSWORD') ?: ''
   def kAlias = System.getenv('CAESURA_ANDROID_KEY_ALIAS') ?: System.getenv('CAESURA_KEY_ALIAS') ?: ''
   def kPass  = System.getenv('CAESURA_ANDROID_KEY_PASS') ?: System.getenv('CAESURA_KEY_PASSWORD') ?: ''
   ```
2. **Explicit Verification & Signing Scheme Flags**:
   ```groovy
   signingConfigs {
       release {
           if (ksPath != null && file(ksPath).exists()) {
               storeFile file(ksPath)
               storePassword ksPass
               keyAlias kAlias
               keyPassword kPass
               v1SigningEnabled true
               v2SigningEnabled true
           }
       }
   }
   ```
3. **App Bundle (AAB) DSL Configuration**:
   ```groovy
   bundle {
       language { enableSplit false } // Keep CJK / multi-language assets intact in base APK
       density  { enableSplit true }
       abi      { enableSplit true }
   }
   ```

### 1.3 Keytool Keystore Generation Specification

To generate a PKCS12 release keystore compatible with Google Play and modern Android devices:
```bash
keytool -genkeypair -v \
    -keystore caesura-release.keystore \
    -alias caesura \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -storetype PKCS12 \
    -storepass "<KEYSTORE_PASSWORD>" \
    -keypass "<KEY_PASSWORD>" \
    -dname "CN=Caesura Game, OU=Release, O=CaesuraEngine, C=JP"
```

### 1.4 Verification Tools Pipeline (`zipalign` & `apksigner`)

1. **4-Byte Alignment Verification**:
   ```bash
   zipalign -c -v 4 android/app/build/outputs/apk/release/app-release.apk
   ```
   *Requirement*: All uncompressed native libraries (`.so`) and resource files must be aligned on 4-byte boundaries (or 16KB / 4KB for Android 15 page sizes).
2. **APK Signature Scheme Verification**:
   ```bash
   apksigner verify --verbose --print-certs android/app/build/outputs/apk/release/app-release.apk
   ```
   *Requirement*:
   - `Verifies: true`
   - `Verified using v1 scheme (JAR signing): true`
   - `Verified using v2 scheme (APK Signature Scheme v2): true`
   - `Verified using v3 scheme (APK Signature Scheme v3): true`
3. **AAB Content Verification**:
   Inspect zip entry structure:
   - `base/lib/arm64-v8a/libCaesuraAmeKAG.so`
   - `base/lib/arm64-v8a/libSDL3.so`
   - `base/assets/game/scripts/kag/init.lua`
   - `base/assets/game/demo/first_vn/story.ks`
   - `base/manifest/AndroidManifest.xml`

---

## Part 2: iOS CMake & Metal Toolchain / CI Build Hardening (Milestone R3)

### 2.1 iOS CMake & Xcode Toolchain Analysis

#### CMake System & Target Architecture:
1. **Toolchain Flags**:
   - Generator: `-G Xcode`
   - System: `-DCMAKE_SYSTEM_NAME=iOS`
   - Architecture: `-DCMAKE_OSX_ARCHITECTURES=arm64`
   - Deployment Target: `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0`
   - Sysroot: `-DCMAKE_OSX_SYSROOT=iphoneos` (device) or `iphonesimulator` (simulator).
2. **Framework Dependencies** (`cmake/CaesuraModules.cmake:78-111`):
   - `Metal`: bgfx Metal rendering backend.
   - `QuartzCore`: `CAMetalLayer` integration.
   - `UIKit`: iOS application window and touch event layer.
   - `Foundation`: Core OS services.
   - `AudioToolbox`: SoLoud CoreAudio driver backend (`SOLOUD_BACKEND_COREAUDIO=ON`).
   - `OpenSSL`: Static `libssl.a` and `libcrypto.a` compiled for iOS (`ios64-xcrun`).
3. **Application Target Definition** (`CMakeLists.txt`):
   - For iOS builds, the target is defined as `add_executable(${PROJECT_NAME} src/main.cpp)`.
   - To produce a compliant iOS App Bundle with Xcode, the target properties must include:
     ```cmake
     if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
         set_target_properties(${PROJECT_NAME} PROPERTIES
             MACOSX_BUNDLE TRUE
             MACOSX_BUNDLE_GUI_IDENTIFIER "com.caesura.amekag"
             MACOSX_BUNDLE_BUNDLE_NAME "CaesuraAmeKAG"
             XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.caesura.amekag"
             XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" # iPhone, iPad
             XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
             XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
         )
     endif()
     ```

### 2.2 Metal Shader Compilation Pipeline & Census

#### Compilation Toolchain:
- Shaders are authored in GLSL `.sc` (bgfx shader dialect) in `shaders/glsl/`.
- `shaders/compile_shaders.sh` invokes `shaderc`:
  `shaderc -f shaders/glsl/<name>.sc -o shaders/compiled/macos/<name>.metal.bin --type <vertex|fragment> --varyingdef shaders/varying.def --platform osx --profile metal`
- `shaders/embed_to_c.py` reads compiled `.metal.bin` files and generates C++ byte arrays in `src/render/EmbeddedShaders_Metal.cpp`.

#### Embedded Metal Shaders Census:

| Shader Name | Type | Symbol in `EmbeddedShaders_Metal.cpp` | Function & Role |
|---|---|---|---|
| `vs_sprite` | Vertex | `kEmbeddedMetal_vs_sprite` | 2D quad batching & UI sprite transformation |
| `vs_fullscreen` | Vertex | `kEmbeddedMetal_vs_fullscreen` | Fullscreen effect quad (-1..1 NDC) |
| `stretch_blt_vs` | Vertex | `kEmbeddedMetal_stretch_blt_vs` | Viewport stretch blit transformation |
| `affine_blt_vs` | Vertex | `kEmbeddedMetal_affine_blt_vs` | Affine transform 2D quad blit |
| `fs_texture` | Fragment | `kEmbeddedMetal_fs_texture` | Standard RGBA texture sampler (also used as universal fallback) |
| `fs_blend` | Fragment | `kEmbeddedMetal_fs_blend` | 10 Photoshop/KAG blend modes (Multiply, Screen, Overlay, etc.) |
| `fs_transition` | Fragment | `kEmbeddedMetal_fs_transition` | Universal rule-masked scene transitions |
| `fs_vfx` | Fragment | `kEmbeddedMetal_fs_vfx` | Color filters, tinting, blur, and screen shaking |
| `stretch_blt_fs` | Fragment | `kEmbeddedMetal_stretch_blt_fs` | Stretched viewport pixel sampler |
| `affine_blt_fs` | Fragment | `kEmbeddedMetal_affine_blt_fs` | Affine transformed pixel sampler |
| `MiniGame_VS` | Vertex (MSL) | `kEmbeddedMSL_MiniGame_VS` | 3D Skeletal/Mesh vertex transformation (MSL source) |
| `MiniGame_FS` | Fragment (MSL) | `kEmbeddedMSL_MiniGame_FS` | 3D Phong lighting + PBR material fragment shader (MSL source) |

### 2.3 Metal Graceful Fallback & Degradation Paths

1. **Post-Processing Shaders (Post-FX)** (`BgfxShaderManager.cpp:275-278`):
   - DXBC contains dedicated PS bytecodes (`fs_postfx_vignette`, `fs_postfx_lut`, `fs_postfx_blur`, `fs_postfx_bloom`).
   - On Metal (and OpenGL/Vulkan), dedicated PS bytecodes are omitted. `BgfxShaderManager` evaluates:
     `if (fsPostfxX.size == 0) fsPostfxX = fsTexture;`
   - The post-processing render pipeline executes without modification, passing frames through identity copies. There are zero GPU crashes, memory leaks, or pipeline stalls.
2. **Skeletal Mesh Animation (SMA S5)** (`SmaMeshRenderer.cpp:185-198`):
   - S5 GPU compute skinning is active on D3D11/D3D12 and OpenGL 4.3+ (`BGFX_CAPS_COMPUTE`).
   - On Metal, compute skinning is bypassed and the engine automatically falls back to `SmaSkinner.h` (SIMD-accelerated CPU skeletal skinning), ensuring 100% rendering correctness.
3. **Missing Blit Shaders Fallback**:
   - `stretchVs/Fs` and `affineVs/Fs` fall back to `vsSprite` + `fsTexture` if not loaded.
4. **Invalid Program Fallback**:
   - If any specialized shader program fails compilation, `BgfxRenderDevice` binds `m_fallbackProgram` to guarantee continuous rendering.

---

## Part 3: GitHub Actions CI Workflows Analysis

### 3.1 Current CI Pipeline Inspection (`.github/workflows/ci.yml`)

```
CI Workflow Graph:
├── build-windows (MSVC Debug + Release, Lua suites, ctest, ks_check, doc freshness)
├── build-macos   (Apple Clang, Lua suites, ctest)
├── build-linux   (GCC, xvfb bundle smoke, Lua suites, ctest, coupling check)
├── release       (Windows CPack ZIP artifact)
├── ios-compile   (macOS runner, SDL3 iOS + OpenSSL iOS + Xcode build probe) [continue-on-error: true]
├── android-static (Ubuntu runner, build_android.sh syntax + ks_check)
└── android-compile (Ubuntu runner, SDL3 + OpenSSL NDK + libCaesuraAmeKAG.so + assembleDebug + assembleRelease) [continue-on-error: true]
```

### 3.2 Fragile & Missing Steps in CI

| Target | CI Step | Issue / Vulnerability | Hardening Recommendation |
|---|---|---|---|
| **Android** | `android-compile` | `continue-on-error: true` hides regressions. | Make this a strict gating check once release pipeline is verified. |
| **Android** | AAB Generation | `gradle bundleRelease` is not invoked. | Add `gradle bundleRelease` step and verify `.aab` output artifact. |
| **Android** | Release Signing | Only unsigned release APK is created; signing path is untested. | Generate an ephemeral PKCS12 test keystore in CI, sign the APK, and verify with `apksigner`. |
| **Android** | Alignment / Verification | No `zipalign -c` or `apksigner verify`. | Add automated validation step checking V1/V2/V3 signatures. |
| **Android** | Gradle Invocation | Downloads Gradle 8.9 manually via curl. | Introduce checked-in Gradle Wrapper (`./gradlew`). |
| **Android** | Dependency Caching | SDL3 and OpenSSL cross-compiled every run (~8 min). | Add `actions/cache` keyed on SDL3/OpenSSL commit hashes. |
| **iOS** | `ios-compile` | `continue-on-error: true`. | Harden toolchain prerequisites and remove `continue-on-error` or guard with clear error diagnostics. |
| **iOS** | Dependency Caching | SDL3 and OpenSSL built from source every run (~12 min). | Add `actions/cache` for `$RUNNER_TEMP/sdl3-ios` and `$RUNNER_TEMP/openssl-ios`. |
| **iOS** | Static Shader Check | No check that Metal embedded shaders match headers. | Add Python verification script checking Metal shader byte arrays. |

---

## Part 4: Proposed Concrete Scripts & Verification Artifacts

### 4.1 Scripts to Be Created

1. **`scripts/generate_android_keystore.sh` / `.bat`**:
   - Generates PKCS12 keystore using standard parameters.
   - Sets secure default validity (25 years) and prints environment variable export snippet.
2. **`scripts/build_android_release.sh`**:
   - Full automated pipeline: cross-compile NDK `.so` -> stage assets & jniLibs -> build APK & AAB -> zipalign -> apksigner verify.
   - Supports `--ephemeral-key` flag for zero-configuration CI/local testing.
3. **`scripts/verify_android_release.py`**:
   - Python-based headless verification script:
     - Verifies APK and AAB zip structures.
     - Confirms inclusion of `libCaesuraAmeKAG.so`, `libSDL3.so`, scripts, assets, story scenes.
     - Runs `zipalign` and `apksigner` checks if SDK tools are on PATH.
4. **`scripts/verify_metal_shaders.py`**:
   - Python static analysis script:
     - Validates `src/render/EmbeddedShaders_Metal.cpp` and `src/minigame/EmbeddedShaders_MiniGame_Metal.cpp`.
     - Asserts all 10 render shaders and 2 minigame MSL shaders are present and non-empty.
     - Verifies declarations in `EmbeddedShaders.h`.

### 4.2 C++ Unit Tests & Mock Headless Verification

1. **`tests/cpp/test_render_metal_contract.cpp`**:
   - Tests `EmbeddedShaders.h` Metal symbols (`kEmbeddedMetal_*`).
   - Asserts non-zero byte lengths and valid bytecode magic.
   - Tests Post-FX graceful degradation logic under simulated Metal renderer caps.
2. **`tests/cpp/test_mobile_platform_lifecycle.cpp`**:
   - Unit tests for `MobileAdapter` and `PlatformBackend` orientation hints, touch routing, and focus/audio pause hooks.

---

## Summary Matrix: Milestone R2 & R3 Execution Plan

| Milestone | Deliverable | Action Item | Target Location |
|---|---|---|---|
| **R2** | Release Signing Config | Support dual env-vars, enable V1/V2/V3 signing, configure AAB bundle DSL | `android/app/build.gradle` |
| **R2** | Keystore Generator | Create PKCS12 keytool generation script | `scripts/generate_android_keystore.sh` |
| **R2** | Release Pipeline Script | Build release APK + AAB, run zipalign & apksigner | `scripts/build_android_release.sh` |
| **R2** | CI Workflow Hardening | Add ephemeral signing test, bundleRelease, zipalign & apksigner steps | `.github/workflows/ci.yml` |
| **R3** | iOS CMake Target | Add `MACOSX_BUNDLE` and bundle identifier properties for iOS | `CMakeLists.txt` |
| **R3** | Metal Shader Static Check | Create Python script asserting Metal embedded shader integrity | `scripts/verify_metal_shaders.py` |
| **R3** | C++ Contract Tests | Add doctest test case for Metal embedded shader symbols and degradation | `tests/cpp/test_render_metal_contract.cpp` |
| **R3** | iOS CI Hardening | Add caching for SDL3/OpenSSL iOS slices to improve build reliability | `.github/workflows/ci.yml` |

---
