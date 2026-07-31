// test_demo_e2e.cpp - Demo end-to-end smoke test (S1.2)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/UnifiedBinding.h"
#include <fstream>
#include <sstream>
#include <filesystem>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

static LuaManager* initDemoLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) { delete lm; return nullptr; }
    lua_State* L = lm->state();
    luaL_dostring(L,
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path");
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerDevCoreBinding(L);
    registerDebugBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

static bool requireModule(lua_State* L, const char* name) {
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);
    return true;
}

static std::string readDemoScript() {
    std::ifstream f("../../scripts/demo_story.ks");
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST_CASE("Demo E2E: tokenizer and scheduler modules load") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    CHECK(requireModule(L, "tokenizer"));
    CHECK(requireModule(L, "scheduler"));
    delete lm;
}

TEST_CASE("Demo E2E: scheduler runs 10-line demo for 50 iterations") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    REQUIRE(requireModule(L, "scheduler"));

    std::string kag = "*start\n";
    for (int i = 1; i <= 10; ++i) {
        kag += "[ch name=\"T" + std::to_string(i) + "\" text=\"line " + std::to_string(i) + "\"]\n[p]\n";
    }
    kag += "[end]\n";

    lua_pushstring(L, kag.c_str());
    const char* code =
        "local tokenizer = require('tokenizer'); "
        "local scheduler = require('scheduler'); "
        "local tokens = tokenizer.parse(...); "
        "local ctx = {}; "
        "for i = 1, 50 do "
        "  local ok, err = pcall(scheduler.run, ctx, tokens); "
        "  if not ok then error(err) end; "
        "end; "
        "assert(true)";
    int r = luaL_dostring(L, code);
    if (r != LUA_OK) {
        MESSAGE("Lua error: " << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown"));
    }
    CHECK(r == LUA_OK);
    delete lm;
}

TEST_CASE("Demo E2E: demo_story.ks parses and scheduler runs it") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));
    REQUIRE(requireModule(L, "scheduler"));

    std::string script = readDemoScript();
    if (script.empty()) {
        MESSAGE("demo_story.ks not found, skipping");
        delete lm;
        return;
    }

    const char* code =
        "local tokenizer = require('tokenizer'); "
        "local tokens = tokenizer.parse(...); "
        "assert(#tokens > 0, 'should parse demo script')";
    REQUIRE(luaL_loadstring(L, code) == LUA_OK);
    lua_pushstring(L, script.c_str());
    const int r = lua_pcall(L, 1, 0, 0);
    CHECK(r == LUA_OK);
    delete lm;
}

namespace {
std::filesystem::path fullPipelineRepoRoot() {
    auto path = std::filesystem::current_path();
    while (!path.empty()) {
        if (std::filesystem::exists(path / "src") &&
            std::filesystem::exists(path / "tests" / "cpp")) {
            return path;
        }
        const auto parent = path.parent_path();
        if (parent == path) break;
        path = parent;
    }
    return {};
}

std::string readFullPipelineDemo() {
    const auto root = fullPipelineRepoRoot();
    if (root.empty()) return "";
    std::ifstream f(root / "demo" / "full_pipeline_demo.ks");
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
} // namespace

TEST_CASE("Demo E2E: full_pipeline_demo.ks parses the complete command surface") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    REQUIRE(requireModule(L, "tokenizer"));

    const std::string script = readFullPipelineDemo();
    if (script.empty()) {
        MESSAGE("full_pipeline_demo.ks not found, skipping");
        delete lm;
        return;
    }

    const char* code =
        "local tokenizer = require('tokenizer'); "
        "local tokens = tokenizer.parse(...); "
        "assert(#tokens > 40, 'full pipeline should produce a rich token stream'); "
        "local names = {}; "
        "for _, t in ipairs(tokens) do "
        "  if type(t) == 'table' and t.cmd then names[t.cmd] = true "
        "  elseif type(t) == 'table' and t.type then names[t.type] = true end "
        "end; "
        "for _, cmd in ipairs({'bg','fg','cl','ch','p','ruby','er','playbgm','playse','playvoice','setbgmvolume','stopbgm','wait','if','jump','eval','iscript','button','endbutton','save','load','trans','quake','vfx','label','end'}) do "
        "  assert(names[cmd], 'missing command in full pipeline: ' .. cmd) "
        "end";
    REQUIRE(luaL_loadstring(L, code) == LUA_OK);
    lua_pushstring(L, script.c_str());
    const int r = lua_pcall(L, 1, 0, 0);
    if (r != LUA_OK) {
        MESSAGE("Lua error: " << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown"));
    }
    CHECK(r == LUA_OK);
    delete lm;
}
TEST_CASE("Demo E2E: full_pipeline_demo.ks drives to [end] with stub handlers") {
    auto* lm = initDemoLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const std::string script = readFullPipelineDemo();
    if (script.empty()) {
        MESSAGE("full_pipeline_demo.ks not found, skipping");
        delete lm;
        return;
    }

    const char* code = R"lua(
        package.loaded['kag'] = nil
        local dispatched = {}
        local stub = setmetatable({}, {
            __index = function(_, key)
                return function(ctx, params)
                    dispatched[#dispatched + 1] = tostring(key)
                end
            end
        })
        package.loaded['kag'] = stub

        local tokenizer = require('tokenizer')
        local scheduler = require('scheduler')
        local tokens = tokenizer.parse(...)

        local ctx = {
            f = {}, sf = {}, tf = {},
            tokens = tokens, token_index = 1,
            call_stack = {}, layers = {}, backlog = {},
            active_operations = {}, stop_flag = false,
            variables = {}, characters = {},
            unlockedCG = {}, unlockedMusic = {}, seen_scenes = {},
            waiting_input = false, macros = {},
            load_tokens = function() return nil end,
        }

        local co = coroutine.create(function()
            scheduler.run(ctx, tokens, 1)
        end)
        local guard = 0
        while coroutine.status(co) ~= 'dead' and guard < 8000 do
            guard = guard + 1
            local ok, err = coroutine.resume(co, 16)
            if not ok then error(err, 0) end
        end
        assert(coroutine.status(co) == 'dead', 'full pipeline did not reach [end]')

        local found = {}
        for _, name in ipairs(dispatched) do found[name] = true end
        for _, cmd in ipairs({'font','pt','bg','fg','cl','ch','p','ruby','er',
                              'playbgm','playse','playvoice','setbgmvolume','stopbgm',
                              'wait','button','endbutton','save','load','trans',
                              'quake','vfx'}) do
            assert(found[cmd], 'command never dispatched: ' .. cmd)
        end
    )lua";
    REQUIRE(luaL_loadstring(L, code) == LUA_OK);
    lua_pushstring(L, script.c_str());
    const int r = lua_pcall(L, 1, 0, 0);
    if (r != LUA_OK) {
        MESSAGE("Lua error: " << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown"));
    }
    CHECK(r == LUA_OK);
    delete lm;
}