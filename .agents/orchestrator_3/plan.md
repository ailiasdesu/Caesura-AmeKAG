# Execution Plan — 1.x Release Candidate

## Objective
Advance Caesura (AmeKAG) to a verified, evidence-backed 1.x Release Candidate (RC) across 5 core requirements:
- R1 (Task 01): Unified Platform Status Matrix (`docs/status/platform-matrix.yaml`), generator script (`scripts/generate_platform_status.py`), markdown report (`docs/status/platform-status.md`), and CI schema validation.
- R2 (Task 02): First-VN Cross-Platform Behavioral Parity across Windows, Linux, Web, Android, snapshot format (`FirstVNStateSnapshot`), artifact generation (`artifacts/parity/<platform>.json`), and comparison script (`scripts/compare_platform_parity.py`).
- R3 (Task 03): Android Latest HEAD Real-Device Regression validation, comprehensive smoke check (boot, CJK RGBA8 font atlas, multi-texture batching, physical-to-logical touch, save persistence, IME text input, orientation), and report (`docs/platform/android-latest-head-validation.md`).
- R4 (Task 04): iOS Real-Device Track & Hardware Gate Audit, Xcode build, Metal shader census/fallback, lifecycle, audio session, sandbox storage, and hardware gate boundary documentation (`docs/platform/ios-device-validation.md`).
- R5 (Task 05): Release Candidate Gate & Evidence Bundle (`artifacts/release/` manifest, checksums, reports, platform status), 100% baseline tests, and final `RC-GO` declaration (`docs/status/release-candidate-report.md`).

## Step-by-Step Workflow
1. **Phase 0: Survey & Baselining**
   - Spawn 3 parallel Explorers to map existing artifacts, check current scripts/tests/docs, and identify exact delta for R1-R5.
   - Aggregate findings and update master `PROJECT.md` Feature Inventory & Milestones.
2. **Phase 1: Milestone R1 (Task 01 - Status Matrix & Generator)**
   - Explorer -> Worker -> Reviewers (2) -> Challengers (2) -> Forensic Auditor -> Gate.
3. **Phase 2: Milestone R2 (Task 02 - First-VN Parity) & R3 (Task 03 - Android Latest HEAD Regression)**
   - Run R2 & R3 workflows with full Explorer -> Worker -> Reviewers -> Challengers -> Auditor -> Gate cycle.
4. **Phase 3: Milestone R4 (Task 04 - iOS Real-Device Track & Hardware Gate)**
   - Audit iOS toolchain, Metal shaders, and document hardware-gated requirements.
5. **Phase 4: Milestone R5 (Task 05 - Release Candidate Gate & Evidence Bundle)**
   - Run full regression suites (C++, Lua, coupling, test coverage, parity, matrix), assemble artifacts, produce `release-candidate-report.md`, and certify `RC-GO`.
6. **Phase 5: Reporting to Sentinel**
   - Deliver final verified report to parent agent.
