# Pillar R1 Survey Report: Multi-Platform Release Packaging & Distribution Bundling

> **Milestone**: Post-RC Production Sprint — Pillar R1  
> **Target Version**: `1.0.0-rc.1`  
> **Date**: `2026-08-25`  
> **Status**: Survey Complete & Actionable Architecture Plan Prepared  

---

## 1. Executive Summary

Pillar R1 focuses on establishing a production-grade, reproducible, multi-platform release distribution pipeline for **Caesura (AmeKAG) v1.0.0-rc.1**. The distribution bundle encompasses:
1. **Windows Desktop Binary Distribution**: CPack ZIP package (`CaesuraAmeKAG-1.0.0-rc.1-win64.zip`) containing the engine executable, required DLLs (SDL3, FFmpeg, Steamworks), runtime scripts, assets, demo projects, and shaders.
2. **Web Standalone Static Distribution**: Static HTML5/Wasm package (`CaesuraAmeKAG-1.0.0-rc.1-web.zip` / `dist/example_game/`) generated via `scripts/package_game.sh`, bundling the Wasm runtime (`glue.wasm`), DOM renderer, pre-baked story bytecode (`story.lua`), audio engine, PWA Service Worker (`sw.js`), and Web App Manifest (`manifest.webmanifest`).
3. **Android Signed Release Distribution**: Universal Android Release APK (`app-release.apk`) and Android App Bundle (`app-release.aab`) built via `scripts/build_android_release.sh`, signed via environment-variable-driven PKCS12 keystore (`generate_android_keystore.sh`), and verified via `zipalign -c 4` and `apksigner verify`.
4. **Unified Release Artifacts Manifest & Cryptographic Checksums**: `artifacts/dist/` containing all distribution bundles, `checksums.txt` (SHA-256), and `release-manifest.json`.

---

## 2. Deep-Dive Investigation by Subsystem

### 2.1 Subsystem 1: CMakeLists.txt & CPack Windows ZIP Packaging

#### Code Analysis (`CMakeLists.txt:504–557`):
- **Install Targets**:
  - `install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . LIBRARY DESTINATION . BUNDLE DESTINATION .)`
  - `install(DIRECTORY scripts/ DESTINATION scripts)`
  - `install(DIRECTORY demo/ DESTINATION demo)`
  - `install(DIRECTORY assets/ DESTINATION assets)`
  - `install(DIRECTORY projects/ DESTINATION projects)`
  - `install(DIRECTORY shaders/ DESTINATION shaders OPTIONAL)`
  - `install(FILES README.md LICENSE DESTINATION . OPTIONAL)`
- **Dynamic Link Libraries (Windows)**:
  - SDL3: `install(FILES "$<TARGET_FILE:SDL3::SDL3>" DESTINATION . OPTIONAL)`
  - FFmpeg: `install(FILES ${FFMPEG_BIN_DIR}/avcodec-62.dll ${FFMPEG_BIN_DIR}/avformat-62.dll ${FFMPEG_BIN_DIR}/avutil-60.dll ${FFMPEG_BIN_DIR}/swscale-9.dll ${FFMPEG_BIN_DIR}/swresample-6.dll DESTINATION . OPTIONAL)`
  - Steamworks: `build/Release/steam_api64.dll`
- **CPack Configuration**:
  - `CPACK_PACKAGE_NAME`: `"CaesuraAmeKAG"`
  - `CPACK_PACKAGE_VERSION`: `${PROJECT_VERSION}` (currently `1.0.1`)
  - `CPACK_GENERATOR`: `"ZIP"`
  - `CPACK_PACKAGE_CHECKSUM`: `"SHA256"`
  - `CPACK_PACKAGE_FILE_NAME`: `"CaesuraAmeKAG-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"`
  - Output artifact name on Windows x64: `CaesuraAmeKAG-1.0.1-Windows-AMD64.zip`.
- **Existing Artifacts in `build/`**:
  - `build/CaesuraAmeKAG-1.0.1-Windows-AMD64.zip` (87,994,297 bytes, ~83.9 MB, 386 files).
  - `build/CaesuraAmeKAG-1.0.1-Windows-AMD64.zip.sha256` (sidecar SHA-256).
- **Identified Gaps & Refinements**:
  - Naming standardization: To support the explicit canonical target `CaesuraAmeKAG-1.0.0-rc.1-win64.zip`, `CMakeLists.txt` or a packaging wrapper script should alias or configure `CPACK_PACKAGE_FILE_NAME`.
  - FreeType header pruning: `external/freetype` installs dev headers (`include/freetype2/`) by default during `cpack`; adding `EXCLUDE_FROM_ALL` or controlling `FT_DISABLE_INSTALL` keeps the runtime ZIP lightweight and strictly production-focused.

---

### 2.2 Subsystem 2: Web Standalone Static Distribution (`scripts/package_game.sh`)

#### Workflow Analysis:
`scripts/package_game.sh` implements an automated 5-step packaging pipeline:
1. **Input Scene Resolution**: Traverses directories (`demo/example_game`, `tests/projects/first_vn`) or single `.ks` files.
2. **Static Contract Gate (`ks_check.lua`)**: Validates declarative contracts for every scene (ensures 0 syntax/attribute errors).
3. **Bytecode Pre-Baking (`ks_bake.lua --web`)**: Compiles `.ks` source into `cache/story/story.lua` bundle with pre-computed FNV-1a hash table and asset pre-discovery.
4. **Assembly (`web/dist` + Runtime + Assets)**:
   - Builds Vite web player (`web/vite.config.js` -> `web/dist`).
   - Copies `index.html`, `web-assets/` (`glue.wasm`, JS/CSS bundle, `manifest.webmanifest`), `scripts/` (with generated `scripts/index.json` via `web/gen-index.mjs`), and `assets/`.
   - Strips dev artifacts (`__pycache__`, `*.pyc`).
5. **Manifest Generation**: Generates `dist/<game>/MANIFEST.txt` with timestamp, scene list, and byte-size tree.

#### Identified Gaps & Refinements:
- **PWA Service Worker Copying**: `web/sw.js` was not explicitly copied into the root of `dist/<game>/` by `package_game.sh` or `vite.config.js`, causing ServiceWorker registration to 404 in pure static hosting unless explicitly copied.
- **Web App Manifest Root Link**: `web/index.html` references `<link rel="manifest" href="manifest.webmanifest" />`. `manifest.webmanifest` needs to be placed at the root of `dist/<game>/` alongside `index.html`.
- **Archive Bundling**: `package_game.sh` outputs an uncompressed directory `dist/<game>/`. For `artifacts/dist/`, an archive target `CaesuraAmeKAG-1.0.0-rc.1-web.zip` (or `.tar.gz`) is required.

---

### 2.3 Subsystem 3: Android Release Packaging & Signing (`scripts/build_android_release.sh`)

#### Workflow Analysis:
`scripts/build_android_release.sh` provides a complete headless/interactive Android release pipeline:
1. **Signing Setup**:
   - Supports `--ephemeral-key` (generates test PKCS12 keystore via `scripts/generate_android_keystore.sh` for CI/automated testing).
   - Supports `--keystore`, `--storepass`, `--alias`, `--keypass` or environment variables `CAESURA_ANDROID_KEYSTORE`, etc.
   - `android/app/build.gradle` conditionally attaches `signingConfigs.caesura` with `v1SigningEnabled true` and `v2SigningEnabled true`.
2. **Native Library & Asset Staging**:
   - Stages `libCaesuraAmeKAG.so` and `libSDL3.so` to `android/app/src/main/jniLibs/arm64-v8a/`.
   - Stages `scripts/`, `assets/`, `demo/first_vn/` to `android/app/src/main/assets/game/`.
3. **Gradle Build Execution**:
   - `gradle assembleRelease` -> generates `android/app/build/outputs/apk/release/app-release.apk`.
   - `gradle bundleRelease` -> generates `android/app/build/outputs/bundle/release/app-release.aab`.
4. **Verification**:
   - `zipalign -c -v 4` verifies 4-byte page boundary alignment.
   - `apksigner verify --verbose --print-certs` verifies V1/V2/V3 cryptographic signatures.
   - Unzip structure check validates `.so`, `AndroidManifest.xml`, and KAG asset tree.

#### Identified Gaps & Refinements:
- Automated handoff to `artifacts/dist/`: Add flag/step to copy and rename finalized artifacts to `artifacts/dist/CaesuraAmeKAG-1.0.0-rc.1-android.apk` and `artifacts/dist/CaesuraAmeKAG-1.0.0-rc.1-android.aab`.

---

### 2.4 Subsystem 4: `artifacts/dist/` Structure, Manifest & SHA-256 Checksums

#### Target Directory Architecture:
```
artifacts/dist/
├── CaesuraAmeKAG-1.0.0-rc.1-win64.zip        # Windows x64 desktop release package
├── CaesuraAmeKAG-1.0.0-rc.1-web.zip          # Web standalone static distribution bundle
├── CaesuraAmeKAG-1.0.0-rc.1-android.apk      # Signed Release APK (arm64-v8a)
├── CaesuraAmeKAG-1.0.0-rc.1-android.aab      # Release Android App Bundle (Google Play)
├── checksums.txt                             # Unified SHA-256 hashes for all dist artifacts
└── release-manifest.json                     # JSON metadata specification for distribution
```

#### Checksum Standard (`checksums.txt`):
Formatted according to standard coreutils `sha256sum`:
```
<sha256_hash_64_hex>  CaesuraAmeKAG-1.0.0-rc.1-win64.zip
<sha256_hash_64_hex>  CaesuraAmeKAG-1.0.0-rc.1-web.zip
<sha256_hash_64_hex>  CaesuraAmeKAG-1.0.0-rc.1-android.apk
<sha256_hash_64_hex>  CaesuraAmeKAG-1.0.0-rc.1-android.aab
```

#### Release Manifest Schema (`release-manifest.json`):
Must provide structured metadata:
- `name`, `version` (`1.0.0-rc.1`), `release_type`, `commit_sha`, `build_timestamp`
- `artifacts`: List of packaged files with `filename`, `platform`, `target_arch`, `format`, `size_bytes`, `sha256`, `entry_point`, `verification_status`.

---

## 3. Inventory of Existing Files vs Required Work

| Subsystem / File | Current Status | Required Action / Enhancement |
|---|---|---|
| `CMakeLists.txt` (CPack config) | Exists (lines 504–557), generates `CaesuraAmeKAG-1.0.1-Windows-AMD64.zip` | Support custom package file name variable `CPACK_PACKAGE_FILE_NAME` / alias to `CaesuraAmeKAG-1.0.0-rc.1-win64.zip`. |
| `scripts/package_game.sh` | Exists (222 lines), builds `dist/<game>/` | Ensure `sw.js` and `manifest.webmanifest` are copied to bundle root; support zip archiving to `artifacts/dist/`. |
| `web/vite.config.js` | Exists, copies runtime dirs & pins `glue.wasm` | Ensure `sw.js` and `manifest.webmanifest` are included in build output. |
| `web/sw.js` | Exists (68 lines), caches static assets | Verified clean; ensure included in static web distributions. |
| `web/manifest.webmanifest` | Exists (28 lines), PWA manifest | Verified clean; ensure included in static web distributions. |
| `scripts/build_android_release.sh` | Exists (259 lines), builds signed APK/AAB | Add automatic artifact staging into `artifacts/dist/`. |
| `scripts/generate_android_keystore.sh` | Exists (154 lines), supports `--test` / interactive | Fully operational; no changes needed. |
| `android/app/build.gradle` | Exists (65 lines), env-var signing config | Fully operational; no changes needed. |
| `artifacts/dist/` directory | Does not exist | Create directory and generate `checksums.txt` and `release-manifest.json`. |
| `scripts/package_distribution.py` | Does not exist | Create orchestrator script to assemble, zip, hash, and generate manifest for `artifacts/dist/`. |

---

## 4. End-to-End Build & Packaging Execution Steps

For the implementation phase (Worker/Implementer), the full procedure is:
1. **Windows Package**:
   ```bash
   cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release --parallel
   cd build && cpack -C Release -G ZIP && cd ..
   ```
2. **Web Package**:
   ```bash
   bash scripts/package_game.sh demo/example_game --out dist/example_game
   ```
3. **Android Release Package**:
   ```bash
   bash scripts/build_android_release.sh --ephemeral-key
   ```
4. **Assembly into `artifacts/dist/` & Checksums**:
   ```bash
   python scripts/package_distribution.py
   ```
5. **Verification**:
   ```bash
   python scripts/package_distribution.py --verify
   ```
