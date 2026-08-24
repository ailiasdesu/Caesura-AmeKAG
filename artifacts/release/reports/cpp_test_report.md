# C++ Core Doctest Suite Report

- **Target Commit**: `62132e783dd238752659d4227ff26b0235258ea9`
- **Test Executable**: `build/tests/Debug/CaesuraTests.exe`
- **Result**: **1052 passed | 0 failed | 0 skipped**
- **Assertions**: **385,299 passed | 0 failed**
- **Status**: **SUCCESS (100% Pass Rate)**

## Subsystem Breakdown
| Module | Description | Test Status |
|--------|-------------|-------------|
| `archive` | CARC encryption, compression, integrity headers | PASS |
| `audio` | SoLoud 3-bus mixer (BGM, SE, Voice), fading, looping | PASS |
| `debug` | Profiler markers, logger sinks, diagnostic counters | PASS |
| `di` | BackendRegistry, quotas, sandbox limits, type resolution | PASS |
| `entry` | Engine composition root, loop, mobile adapter | PASS |
| `input` | Touch gestures, physical-to-logical scaling, IME bridge | PASS |
| `job` | Multi-threaded worker pool, dependencies, atomics | PASS |
| `live2d` | Cubism loader, motion player, physics, expressions | PASS |
| `minigame` | 3D mesh rendering, orbit camera, MSL shaders | PASS |
| `platform` | SDL3 backend, display DPI, IME input methods | PASS |
| `render` | bgfx device, quad batching, FreeType CJK RGBA8 atlas | PASS |
| `resource` | Async resource pipeline, package resolver, caching | PASS |
| `rpc` | HTTP RPC server, remote inspection endpoints | PASS |
| `script` | Lua 5.4 VM, 123 KAG command contracts, sandbox | PASS |
| `steam` | Steamworks bindings, achievements, mock provider | PASS |
| `storage` | SaveManager v1-v5 schema migration, slots -2..99 | PASS |
