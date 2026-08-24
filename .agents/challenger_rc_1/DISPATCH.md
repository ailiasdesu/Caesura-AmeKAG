## 2026-08-24T22:17:21Z
You are Challenger 1 for the Caesura (AmeKAG) 1.x Release Candidate Gate.
Your assigned working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\challenger_rc_1

You MUST read the following authoritative files first:
1. d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md (specifically section ## 2026-08-24T18:16:03Z)
2. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\05_RELEASE_CANDIDATE.md
3. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\07_AGENT_RULES.md
4. d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
5. d:\文件存放处\code\Caesura(AmeKAG)\artifacts\release\manifest.json
6. d:\文件存放处\code\Caesura(AmeKAG)\scripts\verify_release_candidate.py

Adversarial Scope:
1. Stress-test `scripts/verify_release_candidate.py` and `scripts/compare_platform_parity.py` against adversarial mutations:
   - Tampered manifest (e.g. decision changed to "RC-MAYBE", version mismatch, commit mismatch)
   - Corrupted checksums in `sha256sums.txt` (altered hash, missing file)
   - Mutated parity snapshots (divergent route, missing language, leaked GPU string)
   - Out-of-sync markdown status
2. Empirically verify that every single mutation is caught and causes the verification scripts to exit with error code 1 and reject the release candidate.
3. Record all mutation test scripts and outputs in `handoff.md`.
4. Provide a definitive verdict: `APPROVE` or `REJECT` in `handoff.md`.
5. Use `send_message` to report your verdict to orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
