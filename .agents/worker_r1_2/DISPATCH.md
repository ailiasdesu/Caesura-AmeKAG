## 2026-08-24T17:16:22Z

You are Worker 2 for Milestone R1 (Fixing Sandbox Whitelist & Viewport Clamping).
Your working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r1_2

Read:
- `ORIGINAL_REQUEST.md` at d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md
- `AGENTS.md` at d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
- Review report at d:\文件存放处\code\Caesura(AmeKAG)\.agents\reviewer_r1_2\review.md
- Audit report at d:\文件存放处\code\Caesura(AmeKAG)\.agents\auditor_r1_1\audit.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Your Tasks:
1. In `scripts/sandbox.lua`: Add `_KAG_onTextInput`, `_KAG_onTextEditing`, `_KAG_onKeyDown`, and `_GAME_KEY_BACKSPACE` to `_G_whitelist`.
2. In `scripts/kag/commands/text.lua`: Apply `max_allowed_y = math.floor(vh * 0.45 - box_h)` unconditionally so `box_y = math.max(0, math.min(box_y, max_allowed_y))`.
3. In `tests/scripts/test_input_cmd.lua`: Add tests verifying strict sandbox mode (`_SANDBOX_MODE == "strict"`) execution of `[input]` and 720p/1080p viewport boundary calculations.
4. Execute tests and verify:
   - `python scripts/count_coupling.py --ci`
   - `cmake --build build --config Debug`
   - `./build/tests/Debug/CaesuraTests.exe`
   - `lua tests/scripts/test_input_cmd.lua`
   - `lua tests/scripts/run_lua_tests.lua`

Write your changes report to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r1_2\changes.md` and handoff to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r1_2\handoff.md`.
Send a completion message back when done.
