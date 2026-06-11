# Backlog -- v1.0-rc -> v1.0

> Date: 2026-06-11
> Engine: Caesura (AmeKAG) v1.0-rc
> Assign these items to other agents via the `#` IDs below.

---

## 1. Audio Format Expansion

**Status**: DONE (2026-06-11) WAV/OGG/MP3/FLAC via SoLoud (`dr_wav`, `dr_mp3`, `stb_vorbis`, `dr_flac`). Missing test + doc closure.

| # | Type | Severity | Description | Files |
|---|------|----------|-------------|-------|
| A-01 | test | medium | No real-format loading test. `test_audio.cpp` only tests init/shutdown/volume/nonexistent. Add tests that load actual `.ogg`, `.mp3`, `.flac` files and verify playback. | `tests/cpp/test_audio.cpp` |
| A-02 | test | low | No unsupported-format graceful-failure test. Verify `.aiff` / `.mid` / random bytes don't crash. | `tests/cpp/test_audio.cpp` |
| A-03 | docs | low | README and KAG API docs don't list supported audio formats. | `README.md`, `docs/api/KAG-API.md` |
| A-04 | asset | medium | No test audio files in the repo. Generate four 1-second silent test files: `silence.wav`, `silence.ogg`, `silence.mp3`, `silence.flac`. Place in `tests/audio/`. | `tests/audio/` (new dir) |

**Estimated effort**: ~50 lines C++ (A-01 + A-02) + 5 lines doc (A-03) + 4 tiny audio files (A-04).

**Acceptance criteria**:
- [x] `test_audio.cpp` loads and plays each of the 4 formats
- [x] Loading an unsupported format returns 0 handle without crash
- [x] README lists "WAV, OGG, MP3, FLAC" as supported formats
- [x] Test audio files are checked in under `tests/audio/`
- [x] `cmake --build build --target CaesuraTests && ./build/tests/CaesuraTests` passes

---

## 2. Steam SDK Integration

**Status**: DONE (2026-06-11). Follow the same pattern as Live2D -- optional module (`CAESURA_HAS_STEAM`), user downloads Steamworks SDK separately.

| # | Type | Severity | Description | Files |
|---|------|----------|-------------|-------|
| S-01 | build | high | Add `option(CAESURA_ENABLE_STEAM "Enable Steam integration (requires Steamworks SDK)" OFF)` to CMakeLists.txt. Mirror the existing Live2D pattern (`CAESURA_LIVE2D`). | `CMakeLists.txt` |
| S-02 | feat | high | Define `ISteamBackend` pure virtual interface with three method groups: Achievements (unlock/query/reset), Stats (set/get/store), Cloud (file read/write/ quota). | `src/Steam/ISteamBackend.h` (new) |
| S-03 | build | high | Add Steamworks SDK discovery: `find_path` for `steam_api.h`, `find_library` for `steam_api64.lib` (Win) / `libsteam_api.a` (Unix). Guard behind `CAESURA_ENABLE_STEAM`. | `CMakeLists.txt` |
| S-04 | feat | high | `SteamBackend` -- real implementation of `ISteamBackend`. Contains: `SteamAPI_Init()` / shutdown, achievement unlock/query, stat set/get/store to disk. | `src/Steam/SteamBackend.h`, `src/Steam/SteamBackend.cpp` (new) |
| S-05 | feat | high | `NullSteamBackend` -- no-op implementation for builds without Steam. All methods return false/0/empty. | `src/Steam/NullSteamBackend.h`, `src/Steam/NullSteamBackend.cpp` (new) |
| S-06 | feat | high | `SteamBinding` -- Lua bindings exposing `steam.unlock_achievement(id)`, `steam.get_achievement(id)`, `steam.set_stat(name, val)`, `steam.get_stat(name)`, `steam.store_stats()`. Guard with `#ifdef CAESURA_HAS_STEAM`. | `src/Scripting/SteamBinding.cpp`, `src/Scripting/SteamBinding.h` (new) |
| S-07 | feat | high | `CloudSaveProvider` -- implements `ISaveProvider` using `ISteamRemoteStorage`. Maps save slot IDs to `FileWrite()`/`FileRead()`. Handle 256KB per-file limit by splitting large saves into chunks (or use SDL3 + Steam cloud conflict resolution). | `src/System/CloudSaveProvider.h`, `src/System/CloudSaveProvider.cpp` (new) |
| S-08 | feat | medium | Integrate callbacks into engine main loop: call `SteamAPI_RunCallbacks()` every frame. When Steam overlay is active, pause input processing to prevent double-feed. | `src/Core/Engine.cpp` (modify) |
| S-09 | feat | medium | CRL integration: when `CAESURA_HAS_STEAM` is on, skip offline CARC signature verification (Steam's own DRM handles authorization). Guard with `#ifdef CAESURA_HAS_STEAM`. | `src/Carc/CRLManager.cpp` (modify) |
| S-10 | build | medium | Packaging: add CPack rules to produce SteamPipe-ready `depot/` structure -- engine binary + `steam_appid.txt` + `config.vdf` stub. | `CMakeLists.txt` (modify) or `tools/steam_depot.py` (new) |
| S-11 | test | medium | Unit tests: mock Steam init (or `SteamAPI_InitSafe()` if SDK present), test achievement/stats/cloud operations with NullBackend. | `tests/cpp/test_steam.cpp` (new) |
| S-12 | docs | low | Quick-start guide: "Building with Steam support" -- download SDK, set `STEAMWORKS_SDK_ROOT`, enable `CAESURA_ENABLE_STEAM`. | `docs/steam-integration.md` (new) |

**Estimated effort**: ~600 lines C++ (S-02-S-08), ~80 lines CMake (S-01 + S-03 + S-10), ~100 lines test (S-11), ~30 lines doc (S-12).

**Acceptance criteria**:
- [ ] `cmake -B build -DCAESURA_ENABLE_STEAM=ON -DSTEAMWORKS_SDK_ROOT=...` configures without error
- [ ] NullBackend builds and runs without Steamworks SDK (CAESURA_ENABLE_STEAM=OFF)
- [ ] `steam.unlock_achievement("ACH_WIN_ONE_GAME")` via Lua calls `SteamUserStats()->SetAchievement()`
- [ ] Save slots read/write through `ISteamRemoteStorage` when Steam backend is active
- [ ] `SteamAPI_RunCallbacks()` called each frame; overlay pauses input
- [ ] All tests pass with NullBackend
- [ ] `cpack` produces a `depot/` folder with all release files

---

## Architecture Notes

### Audio format detection logic (existing, for reference)

The engine's `SoLoudAudioEngine::loadWave()` uses this dispatch:

```
isStreamFormat() = .ogg or .mp3  ->  WavStream (disk-streamed, low memory)
else (including .wav, .flac)      ->  Wav (memory-loaded)
```

SoLoud's audio source decoder references:
- `dr_mp3.h` -> MP3
- `stb_vorbis.c` -> OGG Vorbis
- `dr_wav.h` -> WAV
- `dr_flac.h` -> FLAC

All four are compiled and linked through `external/soloud/src/audiosource/wav/dr_impl.cpp`.

### Live2D module -- reference pattern for Steam

Use the same optional-module pattern. Reference `CAESURA_LIVE2D` in `CMakeLists.txt`:

```cmake
option(CAESURA_ENABLE_STEAM "Enable Steam integration (requires Steamworks SDK)" OFF)

if(CAESURA_ENABLE_STEAM)
    set(STEAMWORKS_SDK_ROOT "..." CACHE PATH "Path to Steamworks SDK root")
    # ... find_path/find_library ...
    target_compile_definitions(${PROJECT_NAME} PRIVATE CAESURA_HAS_STEAM)
endif()
```

Conditional compilation in C++:

```cpp
#ifdef CAESURA_HAS_STEAM
    #include "Steam/SteamBackend.h"
#else
    #include "Steam/NullSteamBackend.h"
#endif
```
