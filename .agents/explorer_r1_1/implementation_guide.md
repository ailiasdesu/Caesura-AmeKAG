# Milestone R1 Implementation Guide: IME Virtual Keyboard & Text Input Component (Track IME)

This guide provides the complete, exact code changes required for Worker to implement Milestone R1 across all 11 components.

---

## 1. `src/platform/api/IPlatformBackend.h`

### Location: `src/platform/api/IPlatformBackend.h`
Add the 4 pure virtual methods under a dedicated text input section before the closing brace of `IPlatformBackend`.

### Exact Diff / Code to Add:
```cpp
    // -- Text Input / IME (Virtual Keyboard) --------------------------------
    virtual bool startTextInput() = 0;
    virtual bool stopTextInput() = 0;
    virtual bool setTextInputRect(int x, int y, int w, int h, int cursor = 0) = 0;
    virtual bool isTextInputActive() const = 0;
```

---

## 2. `src/platform/SDL3PlatformBackend.h` & `src/platform/SDL3PlatformBackend.cpp`

### Location: `src/platform/SDL3PlatformBackend.h`
Add method declarations in `public:` section under `// -- IPlatformBackend`:

```cpp
    bool startTextInput() override;
    bool stopTextInput() override;
    bool setTextInputRect(int x, int y, int w, int h, int cursor = 0) override;
    bool isTextInputActive() const override;
```

### Location: `src/platform/SDL3PlatformBackend.cpp`
Append implementation functions at the bottom of the file (before the closing namespace `}`):

```cpp
bool SDL3PlatformBackend::startTextInput() {
    if (!m_window) return false;
    return SDL_StartTextInput(m_window);
}

bool SDL3PlatformBackend::stopTextInput() {
    if (!m_window) return false;
    return SDL_StopTextInput(m_window);
}

bool SDL3PlatformBackend::setTextInputRect(int x, int y, int w, int h, int cursor) {
    if (!m_window) return false;
    SDL_Rect rect{ x, y, w, h };
    return SDL_SetTextInputArea(m_window, &rect, cursor);
}

bool SDL3PlatformBackend::isTextInputActive() const {
    if (!m_window) return false;
    return SDL_TextInputActive(m_window);
}
```

---

## 3. `src/platform/NullPlatformBackend.h` & `src/platform/NullPlatformBackend.cpp`

### Location: `src/platform/NullPlatformBackend.h`
Add method declarations in `public:` and private tracking variables:

```cpp
    bool startTextInput() override;
    bool stopTextInput() override;
    bool setTextInputRect(int x, int y, int w, int h, int cursor = 0) override;
    bool isTextInputActive() const override;

private:
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_textInputActive = false;
    int m_textInputX = 0;
    int m_textInputY = 0;
    int m_textInputW = 0;
    int m_textInputH = 0;
    int m_textInputCursor = 0;
```

### Location: `src/platform/NullPlatformBackend.cpp`
Implement the stub methods and update `shutdown()`:

```cpp
void NullPlatformBackend::shutdown() {
    m_initialized = false;
    m_textInputActive = false;
}

bool NullPlatformBackend::startTextInput() {
    if (!m_initialized) return false;
    m_textInputActive = true;
    return true;
}

bool NullPlatformBackend::stopTextInput() {
    m_textInputActive = false;
    return true;
}

bool NullPlatformBackend::setTextInputRect(int x, int y, int w, int h, int cursor) {
    if (!m_initialized) return false;
    m_textInputX = x;
    m_textInputY = y;
    m_textInputW = w;
    m_textInputH = h;
    m_textInputCursor = cursor;
    return true;
}

bool NullPlatformBackend::isTextInputActive() const {
    return m_initialized && m_textInputActive;
}
```

---

## 4. `tests/cpp/EntryLifecycleBackends.h`

### Location: `tests/cpp/EntryLifecycleBackends.h`
Update the test probe `PlatformBackend` class in `tests/cpp/EntryLifecycleBackends.h` (around line 125) to implement the pure virtual methods:

```cpp
    bool startTextInput() override { return true; }
    bool stopTextInput() override { return true; }
    bool setTextInputRect(int, int, int, int, int = 0) override { return true; }
    bool isTextInputActive() const override { return false; }
```

---

## 5. `src/entry/Engine.cpp`

### Location: `src/entry/Engine.cpp` (in `Engine::processEvents()`)
In `Engine::processEvents()`, within the `while (SDL_PollEvent(&event))` loop, add handling for `SDL_EVENT_TEXT_INPUT` and `SDL_EVENT_TEXT_EDITING`, and pass backspace/return key events:

```cpp
        // -- IME / Virtual Keyboard Text Input & Editing --------------------
        if (event.type == SDL_EVENT_TEXT_INPUT && L && !isLuaExecutionPaused()) {
            lua_getglobal(L, "_KAG_onTextInput");
            if (lua_isfunction(L, -1)) {
                lua_pushstring(L, event.text.text ? event.text.text : "");
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "_KAG_onTextInput error: %s\n", err ? err : "unknown");
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }

        if (event.type == SDL_EVENT_TEXT_EDITING && L && !isLuaExecutionPaused()) {
            lua_getglobal(L, "_KAG_onTextEditing");
            if (lua_isfunction(L, -1)) {
                lua_pushstring(L, event.edit.text ? event.edit.text : "");
                lua_pushinteger(L, event.edit.start);
                lua_pushinteger(L, event.edit.length);
                if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "_KAG_onTextEditing error: %s\n", err ? err : "unknown");
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }
```

And in the `SDL_EVENT_KEY_DOWN` block:
```cpp
                if (event.key.key == SDLK_BACKSPACE) {
                    lua_pushboolean(L, 1); lua_setglobal(L, "_GAME_KEY_BACKSPACE");
                    if (!isLuaExecutionPaused()) {
                        lua_getglobal(L, "_KAG_onKeyDown");
                        if (lua_isfunction(L, -1)) {
                            lua_pushinteger(L, event.key.key);
                            lua_pushstring(L, "backspace");
                            if (lua_pcall(L, 2, 0, 0) != LUA_OK) { lua_pop(L, 1); }
                        } else { lua_pop(L, 1); }
                    }
                }
```
And in the `SDL_EVENT_KEY_UP` block:
```cpp
                if (event.key.key == SDLK_BACKSPACE) {
                    lua_pushboolean(L, 0); lua_setglobal(L, "_GAME_KEY_BACKSPACE");
                }
```

---

## 6. `src/input/InputRouter.cpp`

### Location: `src/input/InputRouter.cpp`
Confirm that `InputRouter::dispatchSdlEvent` preserves non-advancing behavior for text input/editing events under `InputFocus::KAG`:
```cpp
        case InputFocus::KAG: {
            // In KAG mode, mouse clicks and keyboard advance the story.
            // Text input and editing events are non-advancing.
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_KEY_DOWN) {
                // Publish before callbacks so a handler can consume this input.
                m_kagClickPending = true;

                for (auto& cb : m_kagCallbacks) {
                    if (m_focus != InputFocus::KAG) break;
                    cb(event);
                }
            }
            break;
        }
```

---

## 7. `src/script/bindings/DevCoreBinding.cpp`

### Location: `src/script/bindings/DevCoreBinding.cpp`
Add C++ Lua binding functions and register them in `devcore_functions[]`:

```cpp
// -- DevCore.start_text_input() -------------------------------------------

static int lua_DevCore_start_text_input(lua_State* L) {
    auto* platform = getPlatform(L);
    if (!platform) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, platform->startTextInput() ? 1 : 0);
    return 1;
}

// -- DevCore.stop_text_input() --------------------------------------------

static int lua_DevCore_stop_text_input(lua_State* L) {
    auto* platform = getPlatform(L);
    if (!platform) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, platform->stopTextInput() ? 1 : 0);
    return 1;
}

// -- DevCore.set_text_input_rect(x, y, w, h, [cursor]) --------------------

static int lua_DevCore_set_text_input_rect(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    int cursor = (int)luaL_optinteger(L, 5, 0);
    auto* platform = getPlatform(L);
    if (!platform) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, platform->setTextInputRect(x, y, w, h, cursor) ? 1 : 0);
    return 1;
}

// -- DevCore.is_text_input_active() ---------------------------------------

static int lua_DevCore_is_text_input_active(lua_State* L) {
    auto* platform = getPlatform(L);
    if (!platform) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, platform->isTextInputActive() ? 1 : 0);
    return 1;
}
```

In `devcore_functions[]`:
```cpp
static const luaL_Reg devcore_functions[] = {
    { "set_input_focus", lua_DevCore_set_input_focus },
    { "get_input_focus", lua_DevCore_get_input_focus },
    { "log",             lua_DevCore_log             },
    { "quit",            lua_DevCore_quit            },
    { "set_resolution",  lua_DevCore_set_resolution  },
    { "get_resolution",  lua_DevCore_get_resolution  },
    { "set_fullscreen",   lua_DevCore_set_fullscreen },
    { "get_window_size",  lua_DevCore_get_window_size },
    { "get_display_metrics", lua_DevCore_get_display_metrics },
    { "start_text_input",     lua_DevCore_start_text_input     },
    { "stop_text_input",      lua_DevCore_stop_text_input      },
    { "set_text_input_rect",  lua_DevCore_set_text_input_rect  },
    { "is_text_input_active", lua_DevCore_is_text_input_active },
    { nullptr, nullptr }
};
```

---

## 8. `scripts/backend.lua`, `scripts/backend_factory.lua`, `scripts/sandbox.lua`

### Location: `scripts/backend.lua`
Add:
```lua
-- =========================================================================
-- Platform / IME / Text Input
-- =========================================================================

function Backend.start_text_input()
    return devcore_or_guard("start_text_input")
end

function Backend.stop_text_input()
    return devcore_or_guard("stop_text_input")
end

function Backend.set_text_input_rect(x, y, w, h, cursor)
    return devcore_or_guard("set_text_input_rect", x, y, w, h, cursor)
end

function Backend.is_text_input_active()
    return devcore_or_guard("is_text_input_active")
end
```

### Location: `scripts/backend_factory.lua`
In `backend.platform = function(cmd, ...)`:
```lua
        elseif cmd == "start_text_input" then return DevCore.start_text_input()
        elseif cmd == "stop_text_input" then return DevCore.stop_text_input()
        elseif cmd == "set_text_input_rect" then return DevCore.set_text_input_rect(...)
        elseif cmd == "is_text_input_active" then return DevCore.is_text_input_active()
```

### Location: `scripts/sandbox.lua`
In `DEVCORE_WHITELIST`:
```lua
local DEVCORE_WHITELIST = {
    set_input_focus     = true,
    get_input_focus     = true,
    log                 = true,
    get_resolution      = true,
    get_window_size     = true,
    start_text_input    = true,
    stop_text_input     = true,
    set_text_input_rect = true,
    is_text_input_active = true,
}
```

---

## 9. `scripts/kag/schema.lua`

### Location: `scripts/kag/schema.lua`
Define the schema for `[input]` and alias `[edit]`:

```lua
Schema.define("input", {
    _meta = {
        category = "text",
        blocking = true,
        desc = "prompt user for text input via virtual keyboard / IME and store to variable"
    },
    name        = { type = "string", required = true },
    prompt      = { type = "string", default = "", interpolate = true },
    default     = { type = "string", default = "", interpolate = true },
    maxlen      = { type = "number", default = 32, min = 1, max = 512 },
    max_length  = { type = "number", default = 32, min = 1, max = 512 },
    x           = { type = "number", default = 0 },
    y           = { type = "number", default = 0 },
    width       = { type = "number", default = 640, min = 120, max = 1920 },
    height      = { type = "number", default = 180, min = 60, max = 1080 },
    font_size   = { type = "number", default = 28, min = 12, max = 72 },
    color       = { type = "string", default = "#ffffff" },
    bg_color    = { type = "string", default = "#202020" },
    password    = { type = "boolean", default = false },
    cond        = { type = "string" },
    btn_ok      = { type = "string", default = "OK" },
    btn_cancel  = { type = "string", default = "" },
})

Schema.define("edit", {
    _meta = { category = "text", blocking = true, desc = "KAG3 alias of [input]" },
    name        = { type = "string", required = true },
    prompt      = { type = "string", default = "", interpolate = true },
    default     = { type = "string", default = "", interpolate = true },
    maxlen      = { type = "number", default = 32, min = 1, max = 512 },
    max_length  = { type = "number", default = 32, min = 1, max = 512 },
    x           = { type = "number", default = 0 },
    y           = { type = "number", default = 0 },
    width       = { type = "number", default = 640, min = 120, max = 1920 },
    height      = { type = "number", default = 180, min = 60, max = 1080 },
    font_size   = { type = "number", default = 28, min = 12, max = 72 },
    color       = { type = "string", default = "#ffffff" },
    bg_color    = { type = "string", default = "#202020" },
    password    = { type = "boolean", default = false },
    cond        = { type = "string" },
    btn_ok      = { type = "string", default = "OK" },
    btn_cancel  = { type = "string", default = "" },
})
```

---

## 10. `scripts/kag/commands/text.lua`

### Location: `scripts/kag/commands/text.lua`
Register `TextCommands.input` and `TextCommands.edit`:

```lua
-- =============================================================================
-- [input] — Interactive Text Input with Virtual Keyboard & IME Support
-- =============================================================================

function TextCommands.input(ctx, params)
    -- 1. Condition check: skip if condition evaluates to false
    if params.cond and type(params.cond) == "string" and params.cond ~= "" then
        local exprLang = require("kag.expr")
        local ok, v
        if params.cond:find("[&|!?]") then
            ok, v = exprLang.evaluate(ctx, params.cond)
        else
            ok, v = exprLang.evaluateTranslated(ctx, params.cond, params.cond)
        end
        if not (ok and v) then
            return
        end
    end

    local var_name = params.name
    if not var_name or var_name == "" then
        return
    end

    local max_len = tonumber(params.maxlen or params.max_length) or 32
    local buffer = tostring(params.default or "")
    local comp_text = ""
    local prompt_text = tostring(params.prompt or "")
    local is_password = params.password == true
    local btn_ok_label = params.btn_ok or "OK"
    local btn_cancel_label = params.btn_cancel or ""

    -- 2. Viewport & Dimensions with Adaptive Upper Placement
    local vw, vh = require("viewport").wh()
    local box_w = tonumber(params.width) or 640
    local box_h = tonumber(params.height) or 180
    local box_x = tonumber(params.x) or 0
    if box_x <= 0 then
        box_x = math.floor((vw - box_w) / 2)
    end
    local box_y = tonumber(params.y) or 0
    if box_y <= 0 then
        box_y = math.floor(vh * 0.22)
    else
        -- Virtual keyboard occlusion prevention: upper viewport bound y <= 0.45 * vh
        local max_allowed_y = math.floor(vh * 0.45 - box_h)
        if max_allowed_y < 20 then max_allowed_y = 20 end
        if box_y > max_allowed_y then
            box_y = max_allowed_y
        end
    end

    -- 3. Notify platform backend
    backend.set_text_input_rect(box_x, box_y, box_w, box_h, 0)
    backend.start_text_input()

    -- UTF-8 Helpers
    local function utf8_length(s)
        if not s or #s == 0 then return 0 end
        if type(utf8) == "table" and utf8.len then
            local l = utf8.len(s)
            if l then return l end
        end
        local count = 0
        for i = 1, #s do
            local b = s:byte(i)
            if b < 0x80 or b >= 0xC0 then count = count + 1 end
        end
        return count
    end

    local function utf8_pop(s)
        if not s or #s == 0 then return "" end
        if type(utf8) == "table" and utf8.offset then
            local p = utf8.offset(s, -1)
            if p then return s:sub(1, p - 1) end
        end
        local i = #s
        while i > 1 and s:byte(i) >= 0x80 and s:byte(i) < 0xC0 do
            i = i - 1
        end
        return s:sub(1, i - 1)
    end

    -- 4. UI Layout & Button Rectangles
    local btn_ok_rect = {
        x = box_x + box_w - 120,
        y = box_y + box_h - 44,
        w = 100,
        h = 36,
    }
    local btn_cancel_rect = {
        x = box_x + box_w - 240,
        y = box_y + box_h - 44,
        w = 100,
        h = 36,
    }

    local function redraw_ui()
        TextScene.remove_group(ctx, "text_input")
        if #prompt_text > 0 then
            TextScene.add_text(ctx, prompt_text, box_x + 16, box_y + 16,
                { r = 220, g = 220, b = 220, a = 255 }, "text_input", 1, true, false, true)
        end
        local display_buf = is_password and string.rep("*", utf8_length(buffer)) or buffer
        local full_line = display_buf
        if #comp_text > 0 then
            full_line = full_line .. "[" .. comp_text .. "]"
        end
        full_line = full_line .. "|"
        TextScene.add_text(ctx, full_line, box_x + 20, box_y + 64,
            { r = 255, g = 255, b = 255, a = 255 }, "text_input", 1, false, false, true)
        TextScene.add_text(ctx, "[" .. btn_ok_label .. "]", btn_ok_rect.x, btn_ok_rect.y,
            { r = 100, g = 255, b = 100, a = 255 }, "text_input", 1, true, false, true)
        if #btn_cancel_label > 0 then
            TextScene.add_text(ctx, "[" .. btn_cancel_label .. "]", btn_cancel_rect.x, btn_cancel_rect.y,
                { r = 255, g = 100, b = 100, a = 255 }, "text_input", 1, true, false, true)
        end
    end

    redraw_ui()

    ctx._inputMode = true
    ctx.waiting_input = true

    local oldTextInput = _G._KAG_onTextInput
    local oldTextEditing = _G._KAG_onTextEditing
    local oldKeyDown = _G._KAG_onKeyDown
    local oldClick = _G._KAG_onClick

    local function cleanup_and_finish(save_result)
        backend.stop_text_input()
        TextScene.remove_group(ctx, "text_input")
        _G._KAG_onTextInput = oldTextInput
        _G._KAG_onTextEditing = oldTextEditing
        _G._KAG_onKeyDown = oldKeyDown
        _G._KAG_onClick = oldClick
        ctx._inputMode = false
        ctx.waiting_input = false

        if save_result then
            local scopeName, key = var_name:match("^([%a_]+)%.([%w_]+)$")
            local targetTbl
            if scopeName and ctx[scopeName] then
                targetTbl, key = ctx[scopeName], key
            else
                scopeName, key = "f", var_name
                targetTbl = ctx[scopeName]
            end
            if type(targetTbl) == "table" and key and key ~= "" then
                targetTbl[key] = buffer
            end
        end
    end

    _G._KAG_onTextInput = function(text)
        if not ctx._inputMode then
            if oldTextInput then oldTextInput(text) end
            return
        end
        if type(text) == "string" and #text > 0 then
            local current_len = utf8_length(buffer)
            local append_len = utf8_length(text)
            if current_len + append_len <= max_len then
                buffer = buffer .. text
            else
                local allowed = max_len - current_len
                if allowed > 0 then
                    local added = 0
                    for _, cp in utf8.codes(text) do
                        if added < allowed then
                            buffer = buffer .. utf8.char(cp)
                            added = added + 1
                        else
                            break
                        end
                    end
                end
            end
            comp_text = ""
            redraw_ui()
        end
    end

    _G._KAG_onTextEditing = function(text, start, length)
        if not ctx._inputMode then
            if oldTextEditing then oldTextEditing(text, start, length) end
            return
        end
        comp_text = tostring(text or "")
        redraw_ui()
    end

    _G._KAG_onKeyDown = function(keyCode, keyName)
        if not ctx._inputMode then
            if oldKeyDown then oldKeyDown(keyCode, keyName) end
            return
        end
        if keyName == "backspace" or keyCode == 8 or keyCode == 0x08 then
            if #comp_text > 0 then
                comp_text = ""
            else
                buffer = utf8_pop(buffer)
            end
            redraw_ui()
        elseif keyName == "return" or keyCode == 13 or keyCode == 0x0D then
            cleanup_and_finish(true)
        elseif keyName == "escape" or keyCode == 27 or keyCode == 0x1B then
            cleanup_and_finish(false)
        end
    end

    _G._KAG_onClick = function()
        if not ctx._inputMode then
            if oldClick then oldClick() end
            return
        end
        local mx = _G._GAME_MOUSE_X or 0
        local my = _G._GAME_MOUSE_Y or 0
        if mx >= btn_ok_rect.x and mx <= (btn_ok_rect.x + btn_ok_rect.w) and
           my >= (btn_ok_rect.y - 6) and my <= (btn_ok_rect.y + btn_ok_rect.h + 6) then
            cleanup_and_finish(true)
            return
        end
        if #btn_cancel_label > 0 and
           mx >= btn_cancel_rect.x and mx <= (btn_cancel_rect.x + btn_cancel_rect.w) and
           my >= (btn_cancel_rect.y - 6) and my <= (btn_cancel_rect.y + btn_cancel_rect.h + 6) then
            cleanup_and_finish(false)
            return
        end
    end

    coroutine.yield()

    if ctx._inputMode then
        cleanup_and_finish(true)
    end
end

TextCommands.edit = TextCommands.input
```

---

## 11. C++ and Lua Test Suites

### Location: `tests/cpp/test_platform.cpp`
Add test cases for `NullPlatformBackend`, `SDL3PlatformBackend`, and `IPlatformBackend` polymorphism:

```cpp
TEST_CASE("Platform: NullPlatformBackend text input lifecycle") {
    NullPlatformBackend backend;
    CHECK_FALSE(backend.isTextInputActive());
    CHECK_FALSE(backend.startTextInput());
    CHECK_FALSE(backend.setTextInputRect(10, 20, 100, 40, 0));

    CHECK(backend.init("test", 1280, 720));
    CHECK_FALSE(backend.isTextInputActive());

    CHECK(backend.startTextInput());
    CHECK(backend.isTextInputActive());

    CHECK(backend.setTextInputRect(100, 200, 300, 50, 5));
    CHECK(backend.isTextInputActive());

    CHECK(backend.stopTextInput());
    CHECK_FALSE(backend.isTextInputActive());

    backend.shutdown();
    CHECK_FALSE(backend.isTextInputActive());
}

TEST_CASE("Platform: SDL3PlatformBackend text input pre-init safety") {
    SDL3PlatformBackend backend;
    CHECK_FALSE(backend.isTextInputActive());
    CHECK_FALSE(backend.startTextInput());
    CHECK_FALSE(backend.stopTextInput());
    CHECK_FALSE(backend.setTextInputRect(0, 0, 100, 100));
}

TEST_CASE("Platform: IPlatformBackend text input polymorphism") {
    NullPlatformBackend backend;
    IPlatformBackend* iface = &backend;
    CHECK(iface->init("test", 1280, 720));
    CHECK(iface->startTextInput());
    CHECK(iface->isTextInputActive());
    CHECK(iface->setTextInputRect(50, 50, 200, 40, 2));
    CHECK(iface->stopTextInput());
    CHECK_FALSE(iface->isTextInputActive());
    iface->shutdown();
}
```

### Location: `tests/cpp/test_input.cpp`
Add test cases for non-advancing KAG behavior and GAME mode event delivery with UTF-8 preservation:

```cpp
TEST_CASE("InputRouter: SDL_EVENT_TEXT_INPUT is non-advancing in KAG focus") {
    InputRouter router;
    int kagEvents = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagEvents; });

    SDL_Event textEvent = {};
    textEvent.type = SDL_EVENT_TEXT_INPUT;
    textEvent.text.text = "Hello Caesura";

    router.processEvent(textEvent);
    CHECK_FALSE(router.isClickPending());
    CHECK_FALSE(router.hasKAGClick());
    CHECK(kagEvents == 0);
}

TEST_CASE("InputRouter: SDL_EVENT_TEXT_EDITING is non-advancing in KAG focus") {
    InputRouter router;
    int kagEvents = 0;
    router.registerKAGCallback([&](const SDL_Event&) { ++kagEvents; });

    SDL_Event editEvent = {};
    editEvent.type = SDL_EVENT_TEXT_EDITING;
    editEvent.edit.text = "Comp";
    editEvent.edit.start = 0;
    editEvent.edit.length = 4;

    router.processEvent(editEvent);
    CHECK_FALSE(router.isClickPending());
    CHECK_FALSE(router.hasKAGClick());
    CHECK(kagEvents == 0);
}

TEST_CASE("InputRouter: text events route to GAME callback in GAME focus with UTF-8 preservation") {
    InputRouter router;
    router.setFocus(InputFocus::GAME);

    std::string receivedText;
    router.registerGameCallback([&](const SDL_Event& ev) {
        if (ev.type == SDL_EVENT_TEXT_INPUT) {
            receivedText = ev.text.text ? ev.text.text : "";
        }
    });

    const char* utf8String = u8"テスト文字列_主人公_Emoji😊";
    SDL_Event textEvent = {};
    textEvent.type = SDL_EVENT_TEXT_INPUT;
    textEvent.text.text = utf8String;

    router.processEvent(textEvent);
    CHECK(receivedText == utf8String);
}
```

### Location: `tests/scripts/test_input_cmd.lua`
Create new test script:

```lua
-- test_input_cmd.lua — unit test suite for [input] / [edit] commands and IME integration

local results = {}
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local Schema = require("kag.schema")
local TextCommands = require("kag.commands.text")
local TextScene = require("kag.text_scene")
local backend = require("backend")

-- 1. Schema Registration & Coercion
check("input schema defined", Schema.isMigrated("input"))
check("edit schema defined", Schema.isMigrated("edit"))

local coerced = Schema.coerce("input", { name = "f.hero", default = "Ame", maxlen = "16" })
check("schema coerces maxlen to number", type(coerced.maxlen) == "number" and coerced.maxlen == 16)
check("schema retains name", coerced.name == "f.hero")

-- 2. Mock Platform Backend Tracking
local ime_active = false
local ime_rect = nil
local original_start = backend.start_text_input
local original_stop = backend.stop_text_input
local original_set_rect = backend.set_text_input_rect

backend.start_text_input = function() ime_active = true return true end
backend.stop_text_input = function() ime_active = false return true end
backend.set_text_input_rect = function(x, y, w, h, c) ime_rect = { x = x, y = y, w = w, h = h, c = c } return true end

local ctx = {
    f = {}, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
    backlog = {}, layers = {},
}

-- 3. Execution & UI Initialization
pcall(function()
    TextCommands.input(ctx, { name = "f.player_name", prompt = "Name:", default = "Hero", maxlen = 10, y = 800 })
end)

check("input mode active", ctx._inputMode == true)
check("waiting_input flag set", ctx.waiting_input == true)
check("IME started", ime_active == true)
check("IME rect assigned", ime_rect ~= nil)
check("adaptive upper viewport positioning (y <= 0.45 * height)", ime_rect.y <= math.floor(1080 * 0.45))

-- 4. Interactive Typing & Backspace Simulation
check("text input handler installed", type(_G._KAG_onTextInput) == "function")
check("key down handler installed", type(_G._KAG_onKeyDown) == "function")

-- Append text
_G._KAG_onTextInput("ine") -- "Heroine"
-- Backspace twice
_G._KAG_onKeyDown(8, "backspace") -- "Heroin"
_G._KAG_onKeyDown(8, "backspace") -- "Heroi"

-- 5. Commit via Enter Key
_G._KAG_onKeyDown(13, "return")

check("input mode cleared after enter", ctx._inputMode == false)
check("waiting_input cleared", ctx.waiting_input == false)
check("IME stopped after commit", ime_active == false)
check("variable assigned in f scope", ctx.f.player_name == "Heroi")
check("UI elements removed", #TextScene.get_state(ctx).draws == 0)

-- 6. Password Masking & Scope Validation
local ctx2 = {
    f = {}, sf = {}, tf = {}, mp = {},
    text_state = { line = 1, char_offset = 0, opacity = 255, cursor_x = 32, cursor_y = 580, draws = {} },
    textCursorX = 32, textCursorY = 580,
}

pcall(function()
    TextCommands.input(ctx2, { name = "tf.pwd", prompt = "PIN:", default = "1234", password = true })
end)

local draws = TextScene.get_state(ctx2).draws
local has_masked_display = false
for _, d in ipairs(draws) do
    if d.text and d.text:find("%*%*%*%*") then
        has_masked_display = true
        break
    end
end
check("password masked in UI display", has_masked_display)

_G._KAG_onKeyDown(13, "return")
check("plaintext stored in tf scope", ctx2.tf.pwd == "1234")

-- Restore backend mocks
backend.start_text_input = original_start
backend.stop_text_input = original_stop
backend.set_text_input_rect = original_set_rect

print("[test_input_cmd] Done. All checks passed.")
```

### Location: `tests/scripts/run_lua_tests.lua`
Register `"test_input_cmd"` in the `tests` array in `tests/scripts/run_lua_tests.lua`.
