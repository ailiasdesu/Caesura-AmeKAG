# Changes Report - Milestone R2: Android Release Signing & AAB Packaging Pipeline (Track A5)

**Worker**: Worker R2  
**Date**: 2026-08-25  
**Working Directory**: `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r2_1`  

---

## 1. Summary of Changes

Milestone R2 implements the complete release signing, key generation, AAB packaging, and artifact validation pipeline for the Android target of Caesura (AmeKAG) visual novel engine.

### Key Deliverables:
1. **`android/app/build.gradle`**:
   - Reconciled dual environment variable naming conventions (`CAESURA_ANDROID_*` and `CAESURA_*_PATH` / `CAESURA_*_PASSWORD`).
   - Added explicit `v1SigningEnabled true` and `v2SigningEnabled true` to `signingConfigs.caesura`.
   - Added AGP bundle configuration (`bundle { language { enableSplit = false } density { enableSplit = false } abi { enableSplit = false } }`) to preserve visual novel CJK scripts and shared assets.
   - Configured `buildTypes.release` to attach `signingConfig signingConfigs.caesura` if a valid keystore file is detected.
   - Added `abiFilters 'arm64-v8a'` to `defaultConfig.ndk`.

2. **`scripts/generate_android_keystore.sh` & `scripts/generate_android_keystore.bat`**:
   - Implemented standard PKCS12 keytool generation (`keytool -genkeypair -v -keystore ... -storetype PKCS12 -keyalg RSA -keysize 2048 -validity 10000 ...`).
   - Implemented `--test` flag for non-interactive / headless CI test key creation with standard ephemeral credentials.
   - Provided formatted environment variable export instructions upon completion.

3. **`scripts/build_android_release.sh`**:
   - Created end-to-end bash build script supporting `--abi`, `--keystore`, `--ephemeral-key` / `--test-key`, `--skip-apk`, `--skip-aab`, `--skip-verify`.
   - Automates release APK (`assembleRelease`) and AAB (`bundleRelease`) builds.
   - Automates 4-byte boundary validation (`zipalign -c -v 4`) and signature scheme checks (`apksigner verify --verbose --print-certs`).
   - Validates AAB archive structure and entry existence.

4. **`.github/workflows/ci.yml`**:
   - Updated `android-compile` job:
     - Added step to generate an ephemeral PKCS12 test keystore via `scripts/generate_android_keystore.sh --test` and export environment variables.
     - Added `./gradlew assembleRelease` and `./gradlew bundleRelease` execution.
     - Added `zipalign -c -v 4` and `apksigner verify` validation steps.
     - Added AAB internal archive structure checks.
     - Updated artifact upload to archive both release APK and AAB bundles.

5. **`docs/platform/android-release-signing.md`**:
   - Updated full documentation covering key generation, dual env-vars, Gradle signing, bundle split rationale, automated build scripts, `zipalign` / `apksigner` validation, and CI workflows.

---

## 2. File-by-File Details

| File | Changes Made | Rationale |
|---|---|---|
| `android/app/build.gradle` | Dual env-var parsing, v1/v2 signing flags, `bundle { language { enableSplit = false } ... }`, opt-in signing config on release build type. | Eliminates hardcoded secrets, enables multi-scheme signing, preserves VN localization resources in AAB. |
| `scripts/generate_android_keystore.sh` | Bash script for PKCS12 release keystore generation with interactive prompts and `--test` mode. | Standardizes release keystore creation across Unix/macOS/Git Bash environments. |
| `scripts/generate_android_keystore.bat` | Windows batch script for PKCS12 release keystore generation with `--test` mode. | Standardizes release keystore creation for Windows developers. |
| `scripts/build_android_release.sh` | Bash script automating assembleRelease, bundleRelease, zipalign, apksigner, and AAB verification. | Provides a single unified entry point for local and automated Android release packaging. |
| `.github/workflows/ci.yml` | Added ephemeral test key generation, bundleRelease, zipalign, apksigner, and AAB verification to `android-compile`. | Ensures pull requests continuously validate the entire Android signing and packaging pipeline. |
| `docs/platform/android-release-signing.md` | Comprehensive guide on Android release signing, keystore tools, Gradle configuration, and verification commands. | Maintains authoritative, up-to-date platform documentation. |

---

## 3. Verification Commands & Results

1. **Coupling Check**:
   ```bash
   python scripts/count_coupling.py --ci
   ```
   *Result*: `PASS: All modules within thresholds and API boundaries.`

2. **C++ Unit Tests (doctest)**:
   ```cmd
   build\tests\Debug\CaesuraTests.exe
   ```
   *Result*: `[doctest] test cases: 1041 | 1041 passed | 0 failed | 0 skipped` (385,095 assertions passed).

3. **Lua Test Suites**:
   ```pwsh
   & (Get-ChildItem -Recurse build -Filter "lua.exe" | Select-Object -First 1).FullName tests/scripts/run_lua_tests.lua
   ```
   *Result*: `Results: 134 passed, 0 failed, 134 total`.

4. **Script Syntax & Help Verification**:
   - `bash -n scripts/generate_android_keystore.sh` -> PASS (Code 0)
   - `bash -n scripts/build_android_release.sh` -> PASS (Code 0)
   - `bash scripts/generate_android_keystore.sh -h` -> PASS (Code 0)
   - `cmd /c "scripts\generate_android_keystore.bat -h"` -> PASS (Code 0)
   - `bash scripts/build_android_release.sh -h` -> PASS (Code 0)
