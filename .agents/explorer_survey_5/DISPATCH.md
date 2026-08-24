## 2026-08-24T18:17:43Z
You are Survey Explorer 2 for the Caesura (AmeKAG) 1.x Release Candidate project.
Your assigned working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_5

You MUST read the following authoritative files first:
1. d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md (specifically section ## 2026-08-24T18:16:03Z)
2. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\02_PLATFORM_PARITY.md
3. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\07_AGENT_RULES.md
4. d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md

Task Scope:
Focus on Requirement R2 (Task 02: First-VN Cross-Platform Behavioral Parity).
1. Survey `tests/projects/first_vn/` (story script, scenes, branching, variables, save/load, localization zh/en/ja, audio triggers, input handling).
2. Survey existing test harnesses and runners for First-VN across platforms (C++ test runner, Lua runner, Web test harness, Android test harness).
3. Design the lightweight platform-independent `FirstVNStateSnapshot` format for `artifacts/parity/<platform>.json` (tracking label, choice result, `flag_is_sun`, language, save roundtrip, ending, and ensuring no OS/GPU data is leaked).
4. Design the parity verification tool `scripts/compare_platform_parity.py` that compares `desktop == web == android == ios` (with unverified platforms honestly marked `hardware-gated`).
5. Verify how cross-platform script parity is maintained without inserting platform-specific if/else logic into story scripts.
6. Write your comprehensive survey report to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_5\survey_report.md` and `handoff.md`.
7. Use `send_message` to report your findings to the orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
