// test_kag_execution.cpp - KAG script parse -> execute integration tests
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/VFXBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/UnifiedBinding.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

// Helper: create LuaManager with all KAG-related C++ bindings registered
static LuaManager* initKAGLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) {
        delete lm;
        return nullptr;
    }
    lua_State* L = lm->state();

    // Set up Lua package.path so require() finds scripts/ modules
    luaL_dostring(L,
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path");

    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

// Helper: load a Lua module by name via require, expect no error
static bool requireModule(lua_State* L, const char* moduleName) {
    lua_getglobal(L, "require");
    lua_pushstring(L, moduleName);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        fprintf(stderr, "[KAG test] require('%s') error: %s\n",
                moduleName, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);
    return true;
}

// Helper: execute a Lua string, return success
static bool doString(lua_State* L, const char* code) {
    if (luaL_dostring(L, code) != LUA_OK) {
        fprintf(stderr, "[KAG test] Lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// =============================================================================
// SECTION 1: KAG module loading sanity checks
// =============================================================================

TEST_CASE("KAG: Lua modules load without error") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    // Core modules
    CHECK(requireModule(L, "kag"));
    CHECK(requireModule(L, "tokenizer"));
    CHECK(requireModule(L, "scheduler"));
    CHECK(requireModule(L, "flow"));

    // Command handlers
    CHECK(requireModule(L, "kag.commands.audio"));
    CHECK(requireModule(L, "kag.commands.layer"));
    CHECK(requireModule(L, "kag.commands.text"));
    CHECK(requireModule(L, "kag.commands.system"));
    CHECK(requireModule(L, "kag.commands.transition"));
    CHECK(requireModule(L, "kag.commands.vfx"));
    CHECK(requireModule(L, "kag.commands.video"));
    CHECK(requireModule(L, "kag.commands.resource"));
    CHECK(requireModule(L, "kag.commands.save"));
    CHECK(requireModule(L, "kag.operation"));

    delete lm;
}

TEST_CASE("KAG: parser parses empty script") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local tokens = tokenizer.parse('')\n"
        "assert(#tokens == 0, 'empty script should yield 0 tokens')\n";
    CHECK(doString(L, code));

    delete lm;
}

// =============================================================================
// SECTION 2: Tokenizer parsing tests
// Token format: { type = "command", cmd = "<name>", params = { {key,val},... } }
// =============================================================================

TEST_CASE("KAG: parser tokenizes @text command") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local tokens = tokenizer.parse('[text text=\"hello\"]')\n"
        "assert(#tokens >= 1, 'should have at least 1 token')\n"
        "local tok = tokens[1]\n"
        "assert(tok.type == 'command', 'type should be command')\n"
        "assert(tok.cmd == 'text', 'cmd should be text')\n"
        "assert(#tok.params >= 1, 'should have at least 1 param')\n"
        "assert(tok.params[1][1] == 'text', 'param key should be text')\n"
        "assert(tok.params[1][2] == 'hello', 'param value should be hello')\n";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG: parser tokenizes @bg command") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local tokens = tokenizer.parse('[bg file=\"scene.png\"]')\n"
        "assert(#tokens >= 1, 'should have at least 1 token')\n"
        "local tok = tokens[1]\n"
        "assert(tok.type == 'command', 'type should be command')\n"
        "assert(tok.cmd == 'bg', 'cmd should be bg')\n"
        "assert(#tok.params >= 1, 'should have params')\n"
        "assert(tok.params[1][1] == 'file', 'param key should be file')\n"
        "assert(tok.params[1][2] == 'scene.png', 'param value should be scene.png')\n";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG: parser tokenizes @wait command") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local tokens = tokenizer.parse('[wait time=100]')\n"
        "assert(#tokens >= 1)\n"
        "local tok = tokens[1]\n"
        "assert(tok.type == 'command')\n"
        "assert(tok.cmd == 'wait')\n"
        "assert(#tok.params >= 1)\n"
        "assert(tok.params[1][1] == 'time')\n"
        "assert(tok.params[1][2] == '100')\n";
    CHECK(doString(L, code));

    delete lm;
}

// =============================================================================
// SECTION 3: Multi-command scripts and labels
// =============================================================================

TEST_CASE("KAG: parser handles multi-command script") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local script = [=[\n"
        "[bg file=\"bg.png\"]\n"
        "[wait time=300]\n"
        "[text text=\"hello world\"]\n"
        "[p]\n"
        "]=]\n"
        "local tokens = tokenizer.parse(script)\n"
        "assert(#tokens >= 4, 'should have at least 4 tokens, got ' .. #tokens)\n"
        "assert(tokens[1].cmd == 'bg', 'token 1 should be bg')\n"
        "assert(tokens[2].cmd == 'wait', 'token 2 should be wait')\n"
        "assert(tokens[3].cmd == 'text', 'token 3 should be text')\n"
        "assert(tokens[4].cmd == 'p', 'token 4 should be p')\n";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG: parser handles labels and flow commands") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local script = [=[\n"
        "*start\n"
        "[text text=\"beginning\"]\n"
        "[jump target=\"*label_a\"]\n"
        "*label_a\n"
        "[text text=\"arrived\"]\n"
        "[end]\n"
        "]=]\n"
        "local tokens = tokenizer.parse(script)\n"
        "assert(#tokens >= 5, 'should have label + commands, got ' .. #tokens)\n"
        "assert(tokens[1].type == 'label', 'first token should be a label')\n"
        "assert(tokens[1].name == 'start', 'label name should be start')\n";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG: parser handles inline text between commands") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "local script = 'Hello, this is plain text.\\n[wait time=100]\\nMore text here.'\n"
        "local tokens = tokenizer.parse(script)\n"
        "local cmdCount = 0\n"
        "for _, tok in ipairs(tokens) do\n"
        "    if tok.type == 'command' and tok.cmd == 'wait' then\n"
        "        cmdCount = cmdCount + 1\n"
        "    end\n"
        "end\n"
        "assert(cmdCount >= 1, 'should find at least 1 wait command')\n";
    CHECK(doString(L, code));

    delete lm;
}

// =============================================================================
// SECTION 4: Scheduler execution (no-render, no-crash)
// =============================================================================

TEST_CASE("KAG: scheduler runs empty token list without crash") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "scheduler"));

    const char* code =
        "local scheduler = require('scheduler')\n"
        "local ctx = {}\n"
        "local tokens = {}\n"
        "local status, err = pcall(scheduler.run, ctx, tokens)\n"
        "assert(status, 'scheduler.run with empty tokens should not error')\n";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG: scheduler runs non-blocking commands without crash") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag"));
    REQUIRE(requireModule(L, "scheduler"));
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local scheduler = require('scheduler')\n"
        "local tokenizer = require('tokenizer')\n"
        "local script = [=[\n"
        "[text text=\"test message\"]\n"
        "]=]\n"
        "local tokens = tokenizer.parse(script)\n"
        "local ctx = { tokens = tokens, token_index = 1 }\n"
        "local co = coroutine.create(function()\n"
        "    scheduler.run(ctx, tokens, 1)\n"
        "end)\n"
        "local ok, err = coroutine.resume(co)\n"
        "assert(type(ok) == 'boolean', 'coroutine.resume should return boolean')\n";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG runner arbitrates scheduler and debugger resume ownership") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code = R"lua(
        package.loaded['kag_runner'] = nil
        package.loaded['flow'] = {
            load_scene = function(path)
                return { tokens = { 1, 2, 3, 4 }, labels = {}, path = path }
            end
        }
        package.loaded['scheduler'] = {
            run = function(ctx, tokens, start_index)
                for i = start_index or 1, #tokens do
                    ctx.token_index = i
                    ctx.executed = (ctx.executed or 0) + 1
                    coroutine.yield()
                end
            end
        }

        local live_pause = true
        _CAESURA_DEBUG_IS_PAUSED = function() return live_pause end
        _CAESURA_DEBUG_PAUSED = false
        local runner = require('kag_runner')
        local probe_ok, probe_reason = runner.start('blocked-by-live-probe.ks')
        assert(not probe_ok and probe_reason == 'debug-paused')
        live_pause = false

        _CAESURA_DEBUG_IS_PAUSED = nil
        _CAESURA_DEBUG_PAUSED = true
        local ok, reason = runner.start('blocked-by-default-gate.ks')
        assert(not ok and reason == 'debug-paused')
        _CAESURA_DEBUG_PAUSED = false

        local paused = false
        local resumes = {}
        local scheduler_co = nil
        runner.set_resume_adapter({
            is_paused = function()
                return paused
            end,
            resume = function(origin, co, value)
                resumes[#resumes + 1] = origin
                scheduler_co = co
                return coroutine.resume(co, value)
            end
        })

        assert(runner.start('scene.ks'))
        local ctx = _CAESURA_CTX
        assert(ctx.executed == 1)
        assert(#resumes == 1 and resumes[1] == 'start')

        paused = true
        ok, reason = runner.update(0.016)
        assert(not ok and reason == 'debug-paused')
        assert(ctx.executed == 1 and #resumes == 1)

        ctx.waiting_input = true
        ok, reason = runner.on_click()
        assert(not ok and reason == 'debug-paused')
        assert(ctx.waiting_input == true)
        assert(ctx.executed == 1 and #resumes == 1)

        assert(not runner.start('replacement.ks'))
        assert(_CAESURA_CTX == ctx)

        paused = false
        local resumed, resume_error = coroutine.resume(scheduler_co)
        assert(resumed, resume_error)
        assert(ctx.executed == 2)
        assert(#resumes == 1)

        ok, reason = runner.debug_resume()
        assert(ok and reason == 'suspended')
        assert(ctx.executed == 2 and #resumes == 1)

        ok, reason = runner.update(0.016)
        assert(not ok and reason == 'waiting-input')
        assert(ctx.executed == 2 and #resumes == 1)

        ctx.waiting_input = false
        assert(runner.update(0.016))
        assert(ctx.executed == 3)
        assert(#resumes == 2 and resumes[2] == 'update')
    )lua";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG operation scope cleans up tokens and cancellation callbacks on error") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code = R"lua(
        local Operation = require('kag.operation')
        local ctx = {}
        local callback_count = 0
        local token = nil
        local co = coroutine.create(function()
            local operation <close> = Operation.start(ctx)
            token = operation.token
            token:register(function()
                callback_count = callback_count + 1
            end)
            error('forced operation failure')
        end)

        local ok = coroutine.resume(co)
        assert(not ok)
        local closed = coroutine.close(co)
        assert(not closed)
        assert(token.cancelled)
        assert(callback_count == 1)
        assert(#ctx.active_operations == 0)

        Operation.cancel_all(ctx)
        assert(callback_count == 1)
    )lua";
    CHECK(doString(L, code));

    delete lm;
}

TEST_CASE("KAG layer fade is immediate at zero duration and preserves cancellation state") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const char* code = R"lua(
        local Layers = require('layers')
        local Commands = require('kag.commands.transition')
        Layers.init()
        local node = Layers.add_layer(Layers.get_root(), {
            name = 'fg',
            tag = 'fg',
            x = 1,
            y = 2,
            opacity = 255,
        })
        local ctx = {}

        Commands.fade(ctx, { layer = 'fg', from = 0, to = 210, time = 0 })
        assert(node.opacity == 210)
        assert(#(ctx.active_operations or {}) == 0)

        Commands.move(ctx, { layer = 'fg', x = 12, y = 34, time = 0 })
        assert(node.x == 12 and node.y == 34)

        local co = coroutine.create(function()
            Commands.fade(ctx, {
                layer = 'fg',
                from = 10,
                to = 210,
                time = 100,
            })
        end)
        assert(coroutine.resume(co))
        assert(node.opacity == 10)
        assert(#ctx.active_operations == 1)

        assert(coroutine.resume(co, 25))
        assert(math.abs(node.opacity - 60) < 0.001)
        ctx.active_operations[1]:mark_cancelled()
        assert(coroutine.resume(co, 25))
        assert(coroutine.status(co) == 'dead')
        assert(math.abs(node.opacity - 60) < 0.001)
        assert(#ctx.active_operations == 0)

        local natural = coroutine.create(function()
            Commands.fade(ctx, {
                layer = 'fg',
                from = 0,
                to = 200,
                time = 100,
            })
        end)
        assert(coroutine.resume(natural))
        assert(coroutine.resume(natural, 100))
        assert(coroutine.status(natural) == 'dead')
        assert(node.opacity == 200)
        assert(#ctx.active_operations == 0)
    )lua";
    CHECK(doString(L, code));

    delete lm;
}

// =============================================================================
// SECTION 5: Lua stack integrity and idempotency
// =============================================================================

TEST_CASE("KAG: Lua stack is clean after multiple parse cycles") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const char* code =
        "local tokenizer = require('tokenizer')\n"
        "for i = 1, 10 do\n"
        "    local tokens = tokenizer.parse('[text text=\"cycle_' .. i .. '\"]')\n"
        "    assert(#tokens >= 1, 'cycle ' .. i .. ' should parse')\n"
        "end\n";
    CHECK(doString(L, code));

    int top = lua_gettop(L);
    CHECK(top == 0);

    delete lm;
}

TEST_CASE("KAG: require module idempotency") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    CHECK(requireModule(L, "tokenizer"));
    CHECK(requireModule(L, "tokenizer"));
    CHECK(requireModule(L, "kag"));
    CHECK(requireModule(L, "kag"));
    CHECK(requireModule(L, "scheduler"));
    CHECK(requireModule(L, "scheduler"));

    delete lm;
}

// =============================================================================
// SECTION 6: P1 command expansion tests
// =============================================================================

TEST_CASE("KAG: unlock cg command adds to unlockedCG") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.system"));

    const char* code =
        "local System = require('kag.commands.system')\n"
        "local ctx = {}\n"
        "System.unlock(ctx, { type = 'cg', id = 'scene01' })\n"
        "assert(ctx.unlockedCG ~= nil, 'unlockedCG should exist')\n"
        "assert(ctx.unlockedCG['scene01'] == true, 'scene01 should be unlocked')\n"
        "System.unlock(ctx, { type = 'music', id = 'track01' })\n"
        "assert(ctx.unlockedMusic ~= nil, 'unlockedMusic should exist')\n"
        "assert(ctx.unlockedMusic['track01'] == true, 'track01 should be unlocked')\n";
    CHECK(doString(L, code));
    delete lm;
}


TEST_CASE("KAG: saveplace and loadplace roundtrip") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "system"));
    const char* code = "local System = require('system'); local ctx = { current_label = 'start', pc = 5, tf = { flag = true }, dialog_index = 3 }; System.saveplace(ctx); ctx.current_label = nil; ctx.pc = nil; ctx.tf = nil; local ok = System.loadplace(ctx); assert(ok ~= false); assert(ctx.current_label == 'start'); assert(ctx.pc == 5); assert(ctx.tf.flag == true); assert(ctx.dialog_index == 3);";
    CHECK(doString(L, code));
    delete lm;
}

// =============================================================================
// SECTION 7: U1.3 -- [ch] multi-character position & state management
// =============================================================================

TEST_CASE("KAG: ch command with pos=left renders left-aligned") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "Text.ch(ctx, { name = 'Hero', text = 'Hello!', pos = 'left' })\n"
        "assert(ctx.characters ~= nil, 'ctx.characters should exist')\n"
        "assert(ctx.characters['Hero'] ~= nil, 'Hero should be registered')\n"
        "assert(ctx.characters['Hero'].pos == 'left', 'Hero pos should be left')\n"
        "assert(ctx.waiting_input == true, 'should wait for input')\n"
        "assert(#ctx.backlog == 1, 'should have 1 backlog entry')\n"
        "assert(ctx.backlog[1].name == 'Hero', 'backlog speaker should be Hero')\n"
        "assert(ctx.backlog[1].text == 'Hello!', 'backlog text should match')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: ch command with pos=right renders right-aligned") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "Text.ch(ctx, { name = 'Heroine', text = 'Hi!', pos = 'right' })\n"
        "assert(ctx.characters['Heroine'] ~= nil, 'Heroine should be registered')\n"
        "assert(ctx.characters['Heroine'].pos == 'right', 'pos should be right')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: ch command defaults to center when pos omitted") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "Text.ch(ctx, { name = 'Narrator', text = 'Once upon a time...' })\n"
        "assert(ctx.characters['Narrator'] ~= nil, 'should be registered')\n"
        "assert(ctx.characters['Narrator'].pos == 'center', 'default pos should be center')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: ch command tracks multiple characters independently") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "Text.ch(ctx, { name = 'Hero', text = 'Hello!', pos = 'left' })\n"
        "Text.ch(ctx, { name = 'Heroine', text = 'Hi!', pos = 'right' })\n"
        "assert(ctx.characters['Hero'] ~= nil, 'Hero should exist')\n"
        "assert(ctx.characters['Hero'].pos == 'left', 'Hero pos should be left')\n"
        "assert(ctx.characters['Heroine'] ~= nil, 'Heroine should exist')\n"
        "assert(ctx.characters['Heroine'].pos == 'right', 'Heroine pos should be right')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: ch command inherits position from stored character state") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "Text.ch(ctx, { name = 'Hero', text = 'First line', pos = 'left' })\n"
        "Text.ch(ctx, { name = 'Hero', text = 'Second line' })\n"
        "-- Second call omits pos, should inherit left from stored state\n"
        "assert(ctx.characters['Hero'].pos == 'left', 'pos should be inherited')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: ch command handles invalid pos gracefully") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "-- Invalid pos values should fall back to center without crash\n"
        "Text.ch(ctx, { name = 'Test', text = 'A', pos = 'top' })\n"
        "assert(ctx.characters['Test'].pos == 'center', 'invalid pos should default to center')\n"
        "Text.ch(ctx, { name = 'Test2', text = 'B', pos = '' })\n"
        "assert(ctx.characters['Test2'].pos == 'center', 'empty pos should default to center')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: ch command stores layer and sprite info in character state") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "kag.commands.text"));
    const char* code =
        "local Text = require('kag.commands.text')\n"
        "local ctx = {}\n"
        "Text.ch(ctx, { name = 'Hero', text = 'Hello', layer = 'fg0', storage = 'hero.png' })\n"
        "assert(ctx.characters['Hero'].layer == 'fg0', 'layer should be stored')\n"
        "assert(ctx.characters['Hero'].sprite == 'hero.png', 'sprite should be stored')\n";
    CHECK(doString(L, code));
    delete lm;
}


// =============================================================================

// =============================================================================
// SECTION 8: U1.5 -- macro record & erase (expansion via scheduler inline)
// =============================================================================

TEST_CASE("KAG: macro records body between macro and endmacro") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "scheduler"));
    REQUIRE(requireModule(L, "tokenizer"));
    const char* code =
        "local scheduler = require('scheduler')\n"
        "local tokenizer = require('tokenizer')\n"
        "local script = [=[\n"
        "[macro name=\"greet\"]\n"
        "[text text=\"Hello!\"]\n"
        "[p]\n"
        "[endmacro]\n"
        "]=]\n"
        "local tokens = tokenizer.parse(script)\n"
        "local ctx = {}\n"
        "local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)\n"
        "while coroutine.status(co) ~= 'dead' do coroutine.resume(co) end\n"
        "assert(ctx.macros ~= nil, 'macros should exist')\n"
        "assert(ctx.macros['greet'] ~= nil, 'macro greet should be recorded')\n"
        "assert(#ctx.macros['greet'] == 2, 'body should have 2 tokens')\n"
        "assert(ctx.macros['greet'][1][1] == 'text', 'first body token: text')\n"
        "assert(ctx.macros['greet'][2][1] == 'p', 'second body token: p')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: erasemacro removes macro from registry") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "scheduler"));
    REQUIRE(requireModule(L, "tokenizer"));
    const char* code =
        "local scheduler = require('scheduler')\n"
        "local tokenizer = require('tokenizer')\n"
        "local script = [=[\n"
        "[macro name=\"temp\"]\n"
        "[text text=\"temp body\"]\n"
        "[endmacro]\n"
        "[erasemacro name=\"temp\"]\n"
        "]=]\n"
        "local tokens = tokenizer.parse(script)\n"
        "local ctx = {}\n"
        "local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)\n"
        "while coroutine.status(co) ~= 'dead' do coroutine.resume(co) end\n"
        "assert(ctx.macros ~= nil, 'macros should exist')\n"
        "assert(ctx.macros['temp'] == nil, 'macro temp should be erased')\n";
    CHECK(doString(L, code));
    delete lm;
}

TEST_CASE("KAG: macro with no name is not recorded") {
    auto* lm = initKAGLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "scheduler"));
    REQUIRE(requireModule(L, "tokenizer"));
    const char* code =
        "local scheduler = require('scheduler')\n"
        "local tokenizer = require('tokenizer')\n"
        "local script = [=[\n"
        "[macro]\n"
        "[text text=\"no name\"]\n"
        "[endmacro]\n"
        "]=]\n"
        "local tokens = tokenizer.parse(script)\n"
        "local ctx = {}\n"
        "local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)\n"
        "while coroutine.status(co) ~= 'dead' do coroutine.resume(co) end\n"
        "local empty = (ctx.macros == nil) or (next(ctx.macros) == nil)\n"
        "assert(empty, 'no macro should be recorded without name')\n";
    CHECK(doString(L, code));
    delete lm;
}
