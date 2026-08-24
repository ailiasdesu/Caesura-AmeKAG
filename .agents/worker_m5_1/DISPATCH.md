## 2026-08-24T22:11:42Z
You are Worker M5 for Milestone M5 (Task 05: Release Candidate Gate & Evidence Bundle).
Your assigned working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m5_1

You MUST read the following authoritative files first:
1. d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md (specifically section ## 2026-08-24T18:16:03Z)
2. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\05_RELEASE_CANDIDATE.md
3. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\07_AGENT_RULES.md
4. d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
5. d:\文件存放处\code\Caesura(AmeKAG)\.agents\PROJECT.md
6. d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m1_1\handoff.md
7. d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m2_1\handoff.md
8. d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m3_1\handoff.md
9. d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m4_1\handoff.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

File Ownership:
You have exclusive write ownership of:
- `artifacts/release/manifest.json`
- `artifacts/release/platform-status.json`
- `artifacts/release/parity/`
- `artifacts/release/checksums/`
- `artifacts/release/reports/`
- `docs/status/release-candidate-report.md`
- `scripts/verify_release_candidate.py`
Do NOT modify unrelated engine source files.

Implementation Tasks:
1. Assemble the comprehensive `artifacts/release/` evidence bundle:
   - `manifest.json`: Structured release candidate metadata (version "1.0.0-rc.1", commit SHA `62132e783dd238752659d4227ff26b0235258ea9`, timestamp, platform status breakdown, baseline test results, checksums index).
   - `platform-status.json`: Exported via `python scripts/generate_platform_status.py --json`.
   - `parity/`: Mirrored cross-platform state snapshots (`windows.json`, `linux.json`, `web.json`, `android.json`, `ios.json`, `parity_summary.json`).
   - `checksums/sha256sums.txt`: Cryptographic SHA256 checksums of core release artifacts and test baselines.
   - `reports/`: Machine-readable / markdown reports for C++ doctest suite (1052 tests), Lua test suite (158 suites), module coupling audit (16/16 pass), Metal shader audit (12/12 pass), and First-VN parity assertion (13/13 pass).
2. Construct authoritative `docs/status/release-candidate-report.md`:
   - Declare definitive decision: **`RC-GO`**.
   - Review and clear all 9 release blockers (0 crashes, 0 save corruptions, 0 branch divergence, 0 missing CJK, 0 broken input, 0 broken packages, 0 broken lifecycle, 0 broken audio resume, 0 platform gameplay differences).
   - Detail the 6-platform status matrix with verified evidence citations.
   - Detail acceptance criteria validation (R1 to R5) with 100% verified outcomes.
3. Implement `scripts/verify_release_candidate.py` to assert the completeness, checksum validity, and JSON validity of `artifacts/release/`.
4. Run full baseline tests and record outputs.
5. Write your handoff report to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m5_1\handoff.md` and send message to orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
