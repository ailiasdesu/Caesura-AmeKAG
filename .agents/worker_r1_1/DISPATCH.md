## 2026-08-24T15:23:08Z

You are the Worker implementing Milestone R1 (IME Virtual Keyboard & Text Input Component - Track IME).
Your working directory is: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r1_1

Read:
- `ORIGINAL_REQUEST.md` at d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md
- `AGENTS.md` at d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
- `PROJECT.md` at d:\文件存放处\code\Caesura(AmeKAG)\.agents\PROJECT.md
- Implementation guide at d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_r1_1\implementation_guide.md
- Handoff report at d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_r1_1\handoff.md

Tasks:
1. Implement the pure virtual methods in `src/platform/api/IPlatformBackend.h` (`startTextInput`, `stopTextInput`, `setTextInputRect`, `isTextInputActive`).
2. Implement them in `src/platform/SDL3PlatformBackend.h` / `SDL3PlatformBackend.cpp` using SDL3 APIs.
3. Implement them in `src/platform/NullPlatformBackend.h` / `NullPlatformBackend.cpp` and `tests/cpp/EntryLifecycleBackends.h`.
4. Update `src/entry/Engine.cpp` to route `SDL_EVENT_TEXT_INPUT` (`_KAG_onTextInput`) and `SDL_EVENT_TEXT_EDITING` (`_KAG_onTextEditing`), plus handle backspace/enter keys.
5. Update `src/input/InputRouter.cpp` to ensure text events are non-advancing in KAG mode.
6. Update `src/script/bindings/DevCoreBinding.cpp` to expose the text input methods to Lua.
7. Update `scripts/backend.lua`, `scripts/backend_factory.lua`, and `scripts/sandbox.lua`.
8. Update `scripts/kag/schema.lua` to define `[input]` and `[edit]` commands.
9. Update `scripts/kag/commands/text.lua` to implement `[input]` interactive UI with viewport offset (`y <= 0.45 * height`) to avoid virtual keyboard occlusion.
10. Update/Add C++ doctests in `tests/cpp/test_platform.cpp` and `tests/cpp/test_input.cpp`.
11. Add `tests/scripts/test_input_cmd.lua` and register it in `tests/scripts/run_lua_tests.lua`.
12. Build and run tests to verify:
    - `python scripts/count_coupling.py --ci`
    - `cmake --build build --config Debug` (or appropriate build command)
    - `./build/tests/Debug/CaesuraTests.exe`
    - `lua tests/scripts/run_lua_tests.lua`
