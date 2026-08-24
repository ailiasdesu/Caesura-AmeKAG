## 2026-08-24T18:17:43Z
Task Scope:
Focus on Requirements R3 (Task 03: Android Latest HEAD Regression), R4 (Task 04: iOS Real-Device Track & Hardware Gate Audit), and R5 (Task 05: Release Candidate Gate & Evidence Bundle).
1. Android Latest HEAD: Survey existing Android build scripts, gradle configurations, past validation in `docs/plans/2026-08-24-028-android-full-closure.md`, and test items required for `docs/platform/android-latest-head-validation.md` (boot, CJK RGBA8 atlas, multi-texture, touch mapping, save persistence, IME virtual keyboard, lifecycle, orientation).
2. iOS Track & Hardware Gate: Survey CMake iOS configuration, Metal shader verification, CI workflow, and requirements for `docs/platform/ios-device-validation.md` (prerequisite matrix, Xcode build procedures, Metal shader validations, explicit `hardware-gated` boundary markers).
3. Release Candidate Bundle: Survey required artifact structure under `artifacts/release/` (`manifest.json`, `platform-status.json`, `parity/`, `checksums/`, `reports/`), blocker definitions, baseline test suites (1052+ C++ doctests, 158 Lua test suites, 16/16 coupling limits), and report format for `docs/status/release-candidate-report.md` (`RC-GO` / `RC-NO-GO`).
4. Write your comprehensive survey report to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_6\survey_report.md` and `handoff.md`.
5. Use `send_message` to report your findings to the orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
