## 2026-08-24T15:19:03Z
You are an Explorer for Milestone R1 (IME Virtual Keyboard & Text Input Component).
Your working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_r1_1
Read:
- `ORIGINAL_REQUEST.md` at d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md
- `AGENTS.md` at d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
- `PROJECT.md` at d:\文件存放处\code\Caesura(AmeKAG)\.agents\PROJECT.md
- Survey handoff at d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_1\handoff.md

Your task:
Analyze and formulate the exact code modifications for Worker to implement:
1. `src/platform/api/IPlatformBackend.h`: Add pure virtual methods (`startTextInput`, `stopTextInput`, `setTextInputRect`, `isTextInputActive`).
2. `src/platform/SDL3PlatformBackend.h` & `.cpp`: Implement with `SDL_StartTextInput`, `SDL_StopTextInput`, `SDL_SetTextInputArea`, `SDL_TextInputActive`.
3. `src/platform/NullPlatformBackend.h`: Implement stub methods with active state tracking.
4. `src/entry/Engine.cpp`: Route `SDL_EVENT_TEXT_INPUT` and `SDL_EVENT_TEXT_EDITING` to `_KAG_onTextInput` and `_KAG_onTextEditing`.
5. `src/input/InputRouter.cpp`: Ensure text input events are treated as non-advancing under `InputFocus::KAG`.
6. `src/script/bindings/DevCoreBinding.cpp`: Expose `start_text_input`, `stop_text_input`, `set_text_input_rect`, `is_text_input_active`.
7. `scripts/backend.lua` and `scripts/sandbox.lua`: Add backend facade methods and sandbox whitelist entries.
8. `scripts/kag/schema.lua`: Define `[input]` schema with parameters (`name`, `prompt`, `default`, `maxlen`, `x`, `y`, `width`, `height`, `font_size`, `color`, `bg_color`, `password`, `cond`).
9. `scripts/kag/commands/text.lua`: Implement `[input]` command handler with coroutine yielding, text box UI creation, backspace/typing handling, enter confirmation, and adaptive upper viewport positioning (`y <= 0.45 * height`) to avoid virtual keyboard occlusion.
10. `tests/cpp/test_platform.cpp` & `tests/cpp/test_input.cpp`: Add C++ doctest cases.
11. `tests/scripts/test_input_cmd.lua` and `tests/scripts/run_lua_tests.lua`: Add comprehensive Lua headless test suite.

Write your detailed implementation design to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_r1_1\implementation_guide.md` and a self-contained handoff to `d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_r1_1\handoff.md`.
Send a completion message back when done.
