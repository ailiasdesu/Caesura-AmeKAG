#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "di/BackendRegistry.h"
#include "script/api/ILuaManager.h"
#include "storage/api/ISaveManager.h"
#include "TestPaths.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {

EngineConfig bindingConfig() {
    EngineConfig config;
    config.headless = true;
    return config;
}

// Real composition root, VM registrations, SaveManager and default provider.
// Only the save directory is isolated; no Lua/storage mock replaces the path.
struct SaveBindingFixture {
    TestPaths::ScopedTempDir directory{"save_binding"};
    Engine engine{bindingConfig()};
    ILuaManager* vm = nullptr;
    ISaveManager* saves = nullptr;
    lua_State* state = nullptr;

    SaveBindingFixture() {
        REQUIRE(engine.init());
        vm = BackendRegistry::instance().getLuaManager();
        saves = BackendRegistry::instance().getSaveManager();
        REQUIRE(vm != nullptr);
        REQUIRE(saves != nullptr);
        REQUIRE(saves->getSaveProvider() != nullptr);
        state = vm->state();
        REQUIRE(state != nullptr);
        saves->init(directory.string());
        saves->clearEncryptionKey();
    }

    void run(const std::string& source) {
        vm->resetInstructionBudget();
        const int top = lua_gettop(state);
        const int status = luaL_dostring(state, source.c_str());
        const std::string error = status != LUA_OK && lua_isstring(state, -1)
            ? lua_tostring(state, -1) : "Lua binding assertion failed";
        if (status == LUA_OK) CHECK(lua_gettop(state) == top);
        lua_settop(state, top);
        REQUIRE_MESSAGE(status == LUA_OK, error);
    }

    std::string bytes() const {
        std::ifstream input(directory.path() / "save_9.json", std::ios::binary);
        REQUIRE(input.good());
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    void seed() {
        run("assert(KAG.save_game(9, {marker='original'}, 'binding.ks', 7))");
    }

    void reject(const std::string& value) {
        const auto original = bytes();
        run("local value = " + value + R"lua(
            local ok, err = KAG.save_game(9, value, 'binding.ks', 8)
            assert(ok == false, 'unrepresentable state was accepted')
            assert(type(err) == 'string' and #err > 0, 'failure needs a reason')
            local state, meta = KAG.load_game(9)
            assert(state.marker == 'original' and meta.token_index == 7)
            assert(20 + 22 == 42)
        )lua");
        CHECK(bytes() == original);
    }

    void rejectDiskValue(const json& value) {
        REQUIRE(saves->save(9, value, "binding.ks", 7));
        const auto original = bytes();
        run(R"lua(
            local state, err = KAG.load_game(9)
            assert(state == nil, 'unsupported disk state was accepted')
            assert(type(err) == 'string' and #err > 0)
            assert(20 + 22 == 42)
        )lua");
        CHECK(bytes() == original);
        seed();
        run("assert(KAG.load_game(9).marker == 'original')");
    }
};

class FailingLuaAllocator {
public:
    FailingLuaAllocator(lua_State* state, size_t allowance) : state_(state), remaining_(allowance) {
        original_ = lua_getallocf(state_, &originalData_);
        lua_setallocf(state_, allocate, this);
    }
    ~FailingLuaAllocator() { lua_setallocf(state_, original_, originalData_); }
    size_t rejected = 0;
private:
    static void* allocate(void* context, void* memory, size_t oldSize, size_t newSize) {
        auto& self = *static_cast<FailingLuaAllocator*>(context);
        if (newSize != 0 && (memory == nullptr || newSize > oldSize)) {
            if (self.remaining_ == 0) { ++self.rejected; return nullptr; }
            --self.remaining_;
        }
        return self.original_(self.originalData_, memory, oldSize, newSize);
    }
    lua_State* state_;
    lua_Alloc original_ = nullptr;
    void* originalData_ = nullptr;
    size_t remaining_;
};

} // namespace

TEST_CASE("U11 SaveBinding: empty objects survive the real JSON roundtrip") {
    SaveBindingFixture fixture;
    fixture.run(R"lua(
        assert(KAG.save_game(9, {}, 'binding.ks', 1))
        local state = assert(KAG.load_game(9))
        assert(type(state) == 'table' and next(state) == nil)
        assert(KAG.save_game(9, {nested={}, frames={{}, {}}}, 'binding.ks', 2))
        state = assert(KAG.load_game(9))
        assert(type(state.nested) == 'table' and next(state.nested) == nil)
        assert(#state.frames == 2 and next(state.frames[1]) == nil)
        assert(type(state.frames[2]) == 'table' and next(state.frames[2]) == nil)
    )lua");
    CHECK(json::parse(fixture.bytes()).at("data").at("nested").is_object());
}

TEST_CASE("U11 SaveBinding: dense arrays preserve integer index order") {
    SaveBindingFixture fixture;
    fixture.run(R"lua(
        local values = {}
        for i=96,1,-1 do values[i] = {index=i, enabled=i%2==0} end
        assert(KAG.save_game(9, values, 'binding.ks', 3))
        local restored = assert(KAG.load_game(9))
        assert(#restored == 96)
        for i=1,96 do
            assert(restored[i].index == i and restored[i].enabled == (i%2==0))
        end
    )lua");
    const auto disk = json::parse(fixture.bytes()).at("data");
    REQUIRE(disk.is_array());
    for (size_t i = 0; i < disk.size(); ++i) CHECK(disk[i].at("index") == i + 1);
}

TEST_CASE("U11 SaveBinding: NUL and Unicode object keys and values retain their lengths") {
    SaveBindingFixture fixture;
    fixture.run(R"lua(
        local zero = string.char(0)
        local key = 'key' .. zero .. 'suffix'
        local value = 'left' .. zero .. 'right' .. '\u{4e2d}\u{1f600}'
        assert(KAG.save_game(9, {[key]=value, plain='ok'}, value, 4, value))
        local restored, meta = KAG.load_game(9)
        assert(restored)
        assert(restored[key] == value and #restored[key] == #value)
        assert(restored.key == nil and restored.plain == 'ok')
        assert(meta.scene == value, 'loaded scene metadata was truncated')
        local listed = KAG.list_saves()
        assert(#listed == 1 and listed[1].scene == value, 'listed scene metadata was truncated')
    )lua");
    const auto envelope = json::parse(fixture.bytes());
    CHECK(envelope.at("scene") == envelope.at("data").begin().value());
    CHECK(envelope.at("thumbnail") == envelope.at("scene"));
}

TEST_CASE("U11 SaveBinding: shared acyclic tables are copied as independent JSON values") {
    SaveBindingFixture fixture;
    fixture.run(R"lua(
        local shared = {answer=42}
        assert(KAG.save_game(9, {a=shared, b=shared}, 'binding.ks', 5))
        local restored = assert(KAG.load_game(9))
        assert(restored.a.answer == 42 and restored.b.answer == 42)
        restored.a.answer = 0
        assert(restored.b.answer == 42)
    )lua");
}

TEST_CASE("U11 SaveBinding: invalid table keys cannot overwrite an existing slot") {
    SaveBindingFixture fixture;
    fixture.seed();
    for (const char* value : {
             "{[2]='second', named='value'}", "{[1]='first', named='value'}",
             "{[2]='sparse'}", "{[0]='zero'}", "{[-1]='negative'}",
             "{[1.5]='fraction'}", "{[true]='boolean'}", "{[{}]='table'}"}) {
        CAPTURE(value);
        fixture.reject(value);
    }
}

TEST_CASE("U11 SaveBinding: unsupported values and cycles fail without damaging the VM or slot") {
    SaveBindingFixture fixture;
    fixture.seed();
    for (const char* value : {
             "{value=function() end}", "{value=coroutine.create(function() end)}",
             "{value=math.huge}", "{value=-math.huge}", "{value=0/0}",
             "(function() local t={} t.self=t return t end)()",
             "(function() local a,b={},{} a.b=b b.a=a return a end)()"}) {
        CAPTURE(value);
        fixture.reject(value);
    }
    // Full userdata is supplied by the host without replacing any binding.
    lua_newuserdatauv(fixture.state, 1, 0);
    lua_setglobal(fixture.state, "u11_test_userdata");
    fixture.reject("{value=u11_test_userdata}");
    lua_pushnil(fixture.state);
    lua_setglobal(fixture.state, "u11_test_userdata");
}

TEST_CASE("U11 SaveBinding: invalid UTF-8 fails cleanly before writing") {
    SaveBindingFixture fixture;
    fixture.seed();
    fixture.reject("{value=string.char(0xff)}");
    fixture.reject("{[string.char(0xc0, 0x80)]='invalid key'}");
    const auto original = fixture.bytes();
    fixture.run(R"lua(
        local invalid = 'prefix' .. string.char(0, 0xff)
        for _, args in ipairs({{invalid, ''}, {'binding.ks', invalid}}) do
            local ok, err = KAG.save_game(9, {marker='replacement'}, args[1], 8, args[2])
            assert(ok == false and type(err) == 'string', 'invalid metadata was accepted')
        end
    )lua");
    CHECK(fixture.bytes() == original);
}

TEST_CASE("U11 SaveBinding: the supported depth boundary roundtrips and excess depth is rejected") {
    SaveBindingFixture fixture;
    fixture.run(R"lua(
        local root, cursor = {}, nil
        cursor = root
        for i=1,64 do cursor.child={} cursor=cursor.child end
        cursor.leaf = 'depth-64'
        assert(KAG.save_game(9, root, 'binding.ks', 6))
        local restored = assert(KAG.load_game(9))
        for i=1,64 do restored=assert(restored.child) end
        assert(restored.leaf == 'depth-64')
    )lua");
    fixture.seed();
    fixture.reject(R"lua((function()
        local root, cursor = {}, nil
        cursor = root
        for i=1,65 do cursor.child={} cursor=cursor.child end
        return root
    end)())lua");
}

TEST_CASE("U11 SaveBinding: unrepresentable disk JSON is rejected with a healthy Lua stack") {
    SaveBindingFixture fixture;
    SUBCASE("scalar root cannot become a fabricated empty table") {
        fixture.rejectDiskValue(json("not a state table"));
    }
    SUBCASE("legacy object null keeps the established absent-key mapping") {
        // Historical bindings encoded empty state tables as object nulls.
        // The state codec supplies known-field defaults after native loading.
        REQUIRE(fixture.saves->save(9, json{{"sf", nullptr}, {"f", {{"route", "old"}}}},
                                   "binding.ks", 7));
        fixture.run(R"lua(
            local state = assert(KAG.load_game(9))
            assert(state.sf == nil and state.f.route == 'old')
        )lua");
    }
    SUBCASE("array null cannot become a hole") {
        fixture.rejectDiskValue(json::array({1, nullptr, 3}));
    }
    SUBCASE("unsigned integer must fit Lua integer") {
        fixture.rejectDiskValue(json{{"integer", std::numeric_limits<uint64_t>::max()}});
    }
    SUBCASE("deep input cannot consume an unrelated Lua stack value") {
        json nested = json::object();
        for (int i = 0; i < 65; ++i) nested = json{{"child", std::move(nested)}};
        fixture.rejectDiskValue(nested);
    }
}

TEST_CASE("U11 SaveBinding: Lua allocation failure during load and listing leaves the slot reusable") {
    SaveBindingFixture fixture;
    fixture.run(R"lua(
        local values = {}
        for i=1,32 do values[i]={index=i,text=string.rep(tostring(i),32)} end
        assert(KAG.save_game(9, {marker='original',values=values}, 'allocation.ks', 7))
    )lua");
    const auto original = fixture.bytes();
    for (const char* function : {"load_game", "list_saves"}) {
        size_t failures = 0;
        for (size_t allowance = 0; allowance < 40; ++allowance) {
            CAPTURE(function);
            CAPTURE(allowance);
            lua_gc(fixture.state, LUA_GCCOLLECT);
            REQUIRE(lua_checkstack(fixture.state, 16));
            lua_getglobal(fixture.state, "KAG");
            lua_getfield(fixture.state, -1, function);
            lua_remove(fixture.state, -2);
            const bool loading = std::string(function) == "load_game";
            if (loading) lua_pushinteger(fixture.state, 9);
            int result = LUA_OK;
            size_t rejected = 0;
            {
                FailingLuaAllocator allocator(fixture.state, allowance);
                result = lua_pcall(fixture.state, loading ? 1 : 0, 2, 0);
                rejected = allocator.rejected;
            }
            if (rejected != 0) ++failures;
            CHECK((result == LUA_ERRMEM || result == LUA_OK));
            if (result == LUA_OK) {
                CHECK((lua_istable(fixture.state, -2) || lua_isnil(fixture.state, -2)));
            }
            lua_settop(fixture.state, 0);
            CHECK(fixture.bytes() == original);
            fixture.run("local s=assert(KAG.load_game(9)); assert(s.marker=='original' and #s.values==32)");
        }
        CHECK(failures > 1);
    }
}
