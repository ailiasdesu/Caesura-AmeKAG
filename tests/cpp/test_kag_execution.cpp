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
