// MiniGameCommands.h — single source of truth for the mini-game Lua command
// names (round 31, audit P2-1 tail). Both the C++ luaCall dispatch table
// (BgfxMiniGameBackend::kLuaMethods) and the Lua binding luaL_Reg list
// (MiniGameBinding.cpp) expand this X-macro, so the 15 names live in exactly
// one place and cannot drift apart.
//
// Each invocation is X(snake_case_command, CamelCaseHandlerName) so consumers
// can generate both the Lua-visible name (#snake) and the C++ member-function
// identifier (camel) from a single entry.
#pragma once

namespace Caesura {

// X-macro: CAESURA_MINIGAME_COMMANDS(X) invokes X(snake, camel) once per
// command. Usage:
//   #define CAESURA_MG_ENTRY(snake, camel) {#snake, &camel},
//   constexpr LuaMethod kLuaMethods[] = { CAESURA_MINIGAME_COMMANDS(CAESURA_MG_ENTRY) };
#define CAESURA_MINIGAME_COMMANDS(X) \
    X(spawn_cube, luaSpawnCube) \
    X(spawn_sphere, luaSpawnSphere) \
    X(spawn_plane, luaSpawnPlane) \
    X(remove_object, luaRemoveObject) \
    X(set_camera, luaSetCamera) \
    X(create_material, luaCreateMaterial) \
    X(set_material, luaSetMaterial) \
    X(set_ambient, luaSetAmbient) \
    X(set_directional, luaSetDirectional) \
    X(add_point_light, luaAddPointLight) \
    X(remove_light, luaRemoveLight) \
    X(check_collision, luaCheckCollision) \
    X(set_collision, luaSetCollision) \
    X(set_velocity, luaSetVelocity) \
    X(set_gravity, luaSetGravity)

} // namespace Caesura
