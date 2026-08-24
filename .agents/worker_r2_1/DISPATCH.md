## 2026-08-25T01:28:12Z

You are the Worker implementing Milestone R2 (Android Release Signing & AAB Packaging Pipeline - Track A5).
Your working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r2_1

Tasks:
1. Update `android/app/build.gradle`:
   - Support environment variables for signingConfigs (`CAESURA_ANDROID_KEYSTORE` / `CAESURA_KEYSTORE_PATH`, `CAESURA_ANDROID_KEYSTORE_PASS` / `CAESURA_KEYSTORE_PASSWORD`, `CAESURA_ANDROID_KEY_ALIAS` / `CAESURA_KEY_ALIAS`, `CAESURA_ANDROID_KEY_PASS` / `CAESURA_KEY_PASSWORD`).
   - Enable `v1SigningEnabled true`, `v2SigningEnabled true`.
   - Add `bundle { language { enableSplit = false } density { enableSplit = false } abi { enableSplit = false } }` (or appropriate AGP bundle DSL).
   - Ensure release build type attaches `signingConfig signingConfigs.caesura` if keystore is configured.
2. Provide `scripts/generate_android_keystore.sh` (and `.bat` / cross-platform equivalent if needed):
   - Generates PKCS12 release keystore via `keytool -genkeypair -v -keystore ... -storetype PKCS12 -keyalg RSA -keysize 2048 -validity 10000 ...`.
   - Supports `--test` flag for headless CI ephemeral test key generation.
3. Provide `scripts/build_android_release.sh`:
   - Automates release APK (`assembleRelease`) and AAB (`bundleRelease`) builds.
   - Automates `zipalign -c` and `apksigner verify` validation on generated artifacts.
4. Update `.github/workflows/ci.yml`:
   - Under `android-compile` job, add steps for ephemeral test key generation, `./gradlew assembleRelease`, `./gradlew bundleRelease`, `zipalign`, and `apksigner verify`.
5. Update `docs/platform/android-release-signing.md` to document the complete workflow.
6. Verify scripts and configs. Run tests (`python scripts/count_coupling.py --ci`, `CaesuraTests.exe`, `run_lua_tests.lua`).
