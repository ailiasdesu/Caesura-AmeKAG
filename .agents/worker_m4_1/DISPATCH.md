## 2026-08-24T18:30:40Z
<USER_REQUEST>
You are Worker M4 for Milestone M4 (Task 04: iOS Real-Device Track & Hardware Gate Audit).
Your assigned working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m4_1

You MUST read the following authoritative files first:
1. d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md (specifically section ## 2026-08-24T18:16:03Z)
2. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\04_IOS_DEVICE_CLOSURE.md
3. d:\文件存放处\code\Caesura(AmeKAG)\docs\Caesura_AmeKAG_Agent_Pack\07_AGENT_RULES.md
4. d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
5. d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_6\survey_report.md
6. d:\文件存放处\code\Caesura(AmeKAG)\.agents\PROJECT.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

File Ownership:
You have exclusive write ownership of:
- `docs/platform/ios-device-validation.md`
Do NOT modify unrelated source files.

Implementation Tasks:
1. Construct the authoritative iOS validation and hardware gate document `docs/platform/ios-device-validation.md`.
2. Document the full Track I technical spectrum (I0 Build, I1 Metal Shaders, I2 Lifecycle, I3 Storage, I4 Audio, I5 CJK/IME, I6 Packaging/TestFlight).
3. Audit and record the 12 embedded Metal shaders (`scripts/verify_metal_shaders.py`), Post-FX identity fallback, and SMA CPU soft-skinning fallback.
4. Enforce Iron Rule 10: Explicitly define the hardware-gated prerequisite matrix (macOS 14+, Xcode 15+, iOS 17+ SDK, physical iPhone/iPad, Apple Developer account) and clearly mark real-device execution as `HARDWARE-GATED` with zero false claims.
5. Run baseline test suites and record outputs.
6. Write your handoff report to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m4_1\handoff.md` and send message to orchestrator (Recipient: 5dc851ea-da57-497a-b335-311843d28636).
</USER_REQUEST>

## 2026-08-24T22:10:33Z
**Context**: Task 04 iOS Real-Device Track & Hardware Gate Audit
**Content**: The API quota has reset. Please resume work on Task 04: audit Track I0-I6, verify 12 Metal shaders and fallbacks, construct `docs/platform/ios-device-validation.md` with explicit hardware-gated boundary marking, verify baseline tests, and deliver your handoff report to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_m4_1\handoff.md`.
**Action**: Complete implementation and report completion.
