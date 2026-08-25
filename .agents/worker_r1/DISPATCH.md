## 2026-08-25T01:10:01Z
You are Worker R1 for the Caesura (AmeKAG) Multi-Platform Release Packaging & Distribution Bundling Milestone.
Working Directory: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r1

Inputs:
- ORIGINAL_REQUEST: d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md
- AGENTS.md: d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
- Explorer 1 Handoff: d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_1\handoff.md
- Worker R3/R4 Handoff: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r3_r4\handoff.md

Your Exclusive Write Boundaries:
- scripts/package_distribution.py
- scripts/package_game.sh
- scripts/build_android_release.sh
- artifacts/dist/

Your Task:
Implement Pillar R1 (Multi-Platform Release Packaging & Distribution Bundling):
1. Review Explorer 1 handoff at d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_1\handoff.md.
2. Ensure Windows CPack release package is built and staged:
   - Build Release binaries if needed (`cmake --build build --config Release`).
   - Run CPack in `build/` to generate `CaesuraAmeKAG-1.0.0-rc.1-win64.zip` containing `CaesuraAmeKAG.exe`, `SDL3.dll`, FFmpeg DLLs, `scripts/`, `demo/`, `assets/`, `projects/`, `shaders/`, and licenses.
3. Ensure Web standalone static distribution bundle is packaged:
   - Run `scripts/package_game.sh demo/example_game --out dist/example_game`.
   - Ensure `sw.js`, `manifest.webmanifest`, `web-assets/glue.wasm`, and web runtime are bundled.
   - Archive the web distribution into `CaesuraAmeKAG-1.0.0-rc.1-web.zip`.
4. Ensure Android signed Release APK and AAB are packaged:
   - Run `scripts/build_android_release.sh --ephemeral-key` (or generate/stage APK & AAB).
   - Verify 4-byte boundary zipalign and apksigner signature verification.
   - Stage `CaesuraAmeKAG-1.0.0-rc.1-android.apk` and `CaesuraAmeKAG-1.0.0-rc.1-android.aab`.
5. Create and execute `scripts/package_distribution.py`:
   - Stage all 4 release packages (`CaesuraAmeKAG-1.0.0-rc.1-win64.zip`, `CaesuraAmeKAG-1.0.0-rc.1-web.zip`, `CaesuraAmeKAG-1.0.0-rc.1-android.apk`, `CaesuraAmeKAG-1.0.0-rc.1-android.aab`) into `artifacts/dist/`.
   - Compute SHA-256 hashes for all 4 packages and write `artifacts/dist/checksums.txt`.
   - Generate structured release metadata `artifacts/dist/release-manifest.json` (platform targets, file sizes, SHA-256 hashes, build timestamps, and signing/verification status).
   - Implement and execute `--verify` flag in `scripts/package_distribution.py` to independently verify all checksums cryptographically.
