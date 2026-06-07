#pragma once
// ===========================================================================
//  Caesura (AmeKAG) -- SandboxQuota (Track 3)
//  Resource quota enforcement for AI-generated Lua scripts.
//  Updates _SANDBOX_RESOURCES counters in the Lua state.
// ===========================================================================

struct lua_State;

namespace Caesura {
namespace SandboxQuota {

// Try to allocate one unit of `kind`. Returns true on success.
// `kind` matches the Lua key stem: "textures", "audio_handles",
// "rtt_canvases", "particles_emitters".
// When the limit is reached, prints a warning and returns false.
// The caller should abort the allocation when false is returned.
bool tryAlloc(lua_State* L, const char* kind);

// Release one unit of `kind` (decrement counter).
// Safe to call even when counter is already 0.
void release(lua_State* L, const char* kind);

// Query the current count for diagnostic purposes.
int  count(lua_State* L, const char* kind);
int  maxLimit(lua_State* L, const char* kind);

} // namespace SandboxQuota
} // namespace Caesura
