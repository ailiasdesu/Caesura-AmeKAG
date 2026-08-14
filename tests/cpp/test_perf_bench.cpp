// test_perf_bench.cpp — engine CPU hot-path benchmarks (round 25).
// Establishes baselines for the VN-critical paths that run every frame:
//   - Lua string/table throughput (text formatting is per-line in KAG)
//   - SmaSkinner CPU soft skinning (the GPU compute path's reference)
// Pure CPU, no window/GPU: deterministic, runs on every CI platform.
// Assertions use generous ceilings so slow CI runners stay green; the
// printed numbers are the regression signal.
#include "doctest.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "render/SmaSkinner.h"

#include <chrono>
#include <cstdio>
#include <vector>

namespace {

template <typename F>
double measureMs(F&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// Build an 8k-vertex dual-bone mesh (same shape as the GPU perf test).
Caesura::SMAMesh makeBigMesh() {
    constexpr int kCols = 128;
    constexpr int kRows = 64;
    Caesura::SMAMesh mesh;
    mesh.vertices.reserve(kCols * kRows);
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            Caesura::SMAMeshVertex v;
            v.x = static_cast<float>(c);
            v.y = static_cast<float>(r);
            v.u = static_cast<float>(c) / (kCols - 1);
            v.v = static_cast<float>(r) / (kRows - 1);
            v.bone0 = static_cast<uint16_t>((c + r * 3) % 64);
            v.w0 = 0.6f;
            v.bone1 = static_cast<uint16_t>((c * 7 + r) % 64);
            v.w1 = 0.4f;
            mesh.vertices.push_back(v);
        }
    }
    return mesh;
}

} // namespace

// ---------------------------------------------------------------------------
// Lua VM: string formatting + table append (text/backlog hot path).
// ---------------------------------------------------------------------------
TEST_CASE("Perf: Lua string/table throughput") {
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);

    const char* warmup = "for i=1,2000 do local s = string.format('%d:%s', i, tostring(i)) end";
    REQUIRE(luaL_dostring(L, warmup) == LUA_OK);

    const char* body =
        "local t = {} "
        "for i=1,10000 do "
        "  t[#t+1] = string.format('%d:%s', i, tostring(i)) "
        "end "
        "return #t";
    const double ms = measureMs([&]() {
        REQUIRE(luaL_dostring(L, body) == LUA_OK);
    });
    lua_pop(L, 1);  // the return value

    MESSAGE("Lua throughput: 10k format+append = " << ms << " ms");
    // Generous ceiling: an order-of-magnitude regression must trip it.
    CHECK_MESSAGE(ms < 800.0,
                  "10k Lua format+append took " << ms << "ms (was <100ms locally)");
    lua_close(L);
}

// ---------------------------------------------------------------------------
// Lua VM: table field access (KAG variable reads are table-heavy).
// ---------------------------------------------------------------------------
TEST_CASE("Perf: Lua table field access") {
    lua_State* L = luaL_newstate();
    REQUIRE(L != nullptr);
    luaL_openlibs(L);

    const char* setup = "local t = {} for i=1,100 do t[i] = {x=i, y=i*2} end return t";
    REQUIRE(luaL_dostring(L, setup) == LUA_OK);
    lua_setglobal(L, "perf_t");

    const char* body =
        "local t = perf_t; local acc = 0 "
        "for i=1,10000 do "
        "  local e = t[(i % 100) + 1]; acc = acc + e.x + e.y "
        "end "
        "return acc";
    const double ms = measureMs([&]() {
        REQUIRE(luaL_dostring(L, body) == LUA_OK);
    });
    lua_pop(L, 1);

    MESSAGE("Lua throughput: 10k table reads = " << ms << " ms");
    CHECK_MESSAGE(ms < 400.0,
                  "10k Lua table reads took " << ms << "ms (regression?)");
    lua_close(L);
}

// ---------------------------------------------------------------------------
// SmaSkinner CPU soft skinning (GPU compute reference; see the D3D11 perf
// test for the GPU side of the comparison).
// ---------------------------------------------------------------------------
TEST_CASE("Perf: SmaSkinner 8k-vertex soft skin") {
    const auto mesh = makeBigMesh();
    std::vector<Caesura::BonePose> poses(64);
    for (size_t i = 0; i < poses.size(); ++i) {
        poses[i].rot = static_cast<float>((i * 7) % 360) * 0.01f;
        poses[i].scale = 0.9f + static_cast<float>((i * 13) % 20) * 0.01f;
        poses[i].ox = static_cast<float>((i * 3) % 50);
        poses[i].oy = static_cast<float>((i * 11) % 40);
    }

    std::vector<Caesura::SmaSkinnedVertex> out;
    Caesura::skinMesh(mesh, poses, out);  // warmup

    const double totalMs = measureMs([&]() {
        for (int i = 0; i < 10; ++i) Caesura::skinMesh(mesh, poses, out);
    });
    const double avgMs = totalMs / 10.0;
    MESSAGE("CPU skin 8k verts: " << avgMs << " ms/frame (GPU compute: ~0.08ms)");
    CHECK_MESSAGE(out.size() == mesh.vertices.size(), "skinned output size");
    // Local baseline is ~1.3ms; allow a wide margin for slow CI runners.
    CHECK_MESSAGE(avgMs < 10.0,
                  "CPU skin of 8k verts averaged " << avgMs << "ms (was ~1.3ms)");
}
