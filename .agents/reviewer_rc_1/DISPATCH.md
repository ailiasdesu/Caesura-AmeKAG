## 2026-08-24T22:17:21Z

You are Reviewer 1 for the Caesura (AmeKAG) 1.x Release Candidate Gate (Milestones M2 to M5).
Your assigned working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\reviewer_rc_1

You MUST read the following authoritative files first:
1. d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md (specifically section ## 2026-08-24T18:16:03Z)
2. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\00_MASTER_ORCHESTRATOR.md
3. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\02_PLATFORM_PARITY.md
4. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\03_ANDROID_LATEST_HEAD.md
5. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\04_IOS_DEVICE_CLOSURE.md
6. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\05_RELEASE_CANDIDATE.md
7. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\07_AGENT_RULES.md
8. d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
9. d:\文件存放处\code\Caesura(AmeKAG)\.agents\PROJECT.md
10. d:\文件存放处\code\Caesura(AmeKAG)\docs\status\release-candidate-report.md
11. d:\文件存放处\code\Caesura(AmeKAG)\artifacts\release\manifest.json

Review Scope:
1. Review all 5 deliverables:
   - R1: Platform Matrix (`docs/status/platform-matrix.yaml`, `docs/status/platform-status.md`, `scripts/generate_platform_status.py`)
   - R2: First-VN Cross-Platform Behavioral Parity (`artifacts/parity/`, `scripts/compare_platform_parity.py`, `docs/platform/cross-platform-parity.md`)
   - R3: Android Latest HEAD Real-Device Regression (`docs/platform/android-latest-head-validation.md`, `scripts/verify_android_regression.py`)
   - R4: iOS Real-Device Track & Hardware Gate Audit (`docs/platform/ios-device-validation.md`)
   - R5: Release Candidate Evidence Bundle & Report (`artifacts/release/`, `scripts/verify_release_candidate.py`, `docs/status/release-candidate-report.md`)
2. Verify that all 9 release blockers are genuinely cleared.
3. Verify that all 5 acceptance criteria sections in `ORIGINAL_REQUEST.md` are 100% satisfied.
4. Execute verification commands and record exact outputs.
5. Provide a definitive verdict: `APPROVE` or `REQUEST_CHANGES` in `handoff.md`.
6. Use `send_message` to report your verdict to orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
