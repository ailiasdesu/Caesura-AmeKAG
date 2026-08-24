## 2026-08-24T22:17:21Z

You are the Forensic Integrity Auditor for the Caesura (AmeKAG) 1.x Release Candidate.
Your assigned working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\auditor_rc_1

You MUST read the following authoritative files first:
1. d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md (specifically section ## 2026-08-24T18:16:03Z)
2. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\05_RELEASE_CANDIDATE.md
3. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\07_AGENT_RULES.md
4. d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
5. d:\文件存放处\code\Caesura(AmeKAG)\artifacts\release\manifest.json
6. d:\文件存放处\code\Caesura(AmeKAG)\artifacts\release\checksums\sha256sums.txt
7. d:\文件存放处\code\Caesura(AmeKAG)\docs\status\release-candidate-report.md
8. d:\文件存放处\code\Caesura(AmeKAG)\scripts\verify_release_candidate.py
9. d:\文件存放处\code\Caesura(AmeKAG)\scripts\compare_platform_parity.py
10. d:\文件存放处\code\Caesura(AmeKAG)\scripts\generate_platform_status.py

Forensic Audit Scope:
Perform a comprehensive forensic integrity audit across all 5 Release Candidate requirements (R1 to R5):
1. Static Analysis: Verify that `scripts/verify_release_candidate.py`, `scripts/compare_platform_parity.py`, `scripts/verify_android_regression.py`, and `scripts/generate_platform_status.py` contain genuine validation algorithms with NO dummy bypasses, NO fake passes, and NO hardcoded returns.
2. Data & Cryptographic Integrity: Verify that all 20 SHA-256 hashes in `artifacts/release/checksums/sha256sums.txt` match disk files exactly. Verify that all parity snapshots in `artifacts/release/parity/` and `artifacts/parity/` contain genuine deterministic game states with zero data leakage.
3. Zero Tolerance Check: Check for any cheating, mock bypasses, or integrity violations across the entire sprint.
4. Issue a definitive verdict: `CLEAN` or `INTEGRITY VIOLATION` in `handoff.md`.
5. Use `send_message` to report your verdict to orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
