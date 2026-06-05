# Caesura (AmeKAG) — Build Guide

## Prerequisites

| Dependency | Version | Windows | Linux |
|-----------|---------|---------|-------|
| CMake | ≥ 3.25 | [cmake.org](https://cmake.org) | `apt install cmake` |
| MSVC / MinGW / GCC / Clang | C++20 | Visual Studio 2022 | `apt install build-essential` |
| SDL3 | ≥ 3.2 | vcpkg / manual build | `apt install libsdl3-dev` |
| Git | any | [git-scm.com](https://git-scm.com) | `apt install git` |

## Step 1 — Clone & Initialize Submodules

```bash
git clone https://github.com/your-org/Caesura-AmeKAG.git
cd Caesura-AmeKAG
git submodule update --init --recursive
```

This pulls in:
- `external/bgfx` — Cross-platform rendering library
- `external/lua` — Lua 5.4 scripting engine
- `external/soloud` — SoLoud audio engine
- `external/SDL3` — SDL3 platform abstraction

## Step 2 — Install SDL3

### Windows (vcpkg)
```powershell
vcpkg install sdl3:x64-windows
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

### Windows (manual)
Download SDL3-devel from [github.com/libsdl-org/SDL](https://github.com/libsdl-org/SDL/releases).
Set `SDL3_DIR` to the cmake directory.

### Linux
```bash
sudo apt install libsdl3-dev
# or build from source:
git clone https://github.com/libsdl-org/SDL --branch release-3.2.0
cd SDL && cmake -B build && sudo cmake --install build
```

## Step 3 — Configure & Build

```bash
# Configure
cmake -B build -S .

# Build Release
cmake --build build --config Release

# Build Debug (with debug symbols)
cmake --build build --config Debug
```

The executable is at:
- Windows: `build\Release\CaesuraAmeKAG.exe`
- Linux: `build/CaesuraAmeKAG`

## Step 4 — Run

```bash
cd build/Release   # (or build/Debug)
./CaesuraAmeKAG    # (or .\CaesuraAmeKAG.exe on Windows)
```

The engine will:
1. Create a 1280×720 window via SDL3
2. Initialize bgfx with Direct3D 11 (Windows) or Vulkan/Metal
3. Initialize SoLoud audio engine
4. Load Lua scripts from `scripts/`
5. Start the KAG demo script
6. Enter the main loop — press Esc or close the window to exit

## Step 5 — Generate Production Shaders (Optional)

The embedded shaders in `src/Render/EmbeddedShaders.cpp` are fallback stubs.
For production rendering, compile shaders using bgfx's `shaderc` tool:

```bash
# Build shaderc first
cd external/bgfx
make shaderc

# Compile vertex shader
./shaderc -f shaders/vs_sprite.sc -o shaders/vs_sprite.bin \
  --type vertex --platform windows -p s_5_0 \
  --varyingdef shaders/varying.def.sc

# Compile fragment shader
./shaderc -f shaders/fs_texture.sc -o shaders/fs_texture.bin \
  --type fragment --platform windows -p s_5_0 \
  --varyingdef shaders/varying.def.sc
```

The resulting `.bin` files can be loaded at runtime, or converted to C arrays
for embedding using `xxd -i` or a similar tool.

## Directory Layout

```
Caesura(AmeKAG)/
├── CMakeLists.txt          # Build configuration
├── BUILD.md                # This file
├── src/                    # C++ engine source
│   ├── main.cpp            # Entry point
│   ├── Core/               # Engine, InputRouter, BackendRegistry
│   ├── Render/             # IRenderDevice, BgfxRenderDevice, RTTManager
│   ├── Audio/              # SoLoudAudioEngine
│   └── Scripting/          # LuaManager, KAG/Render/DevCore bindings
├── scripts/                # Lua game logic
│   ├── backend.lua         # Unified C++ backend interface
│   ├── config.lua          # Engine configuration
│   ├── game_logic.lua      # Main game entry point
│   ├── kag.lua             # KAG command dispatch
│   ├── scheduler.lua       # Coroutine-based script executor
│   ├── tokenizer.lua       # KAG script parser
│   ├── audio.lua           # Audio subsystem (→ backend.lua)
│   ├── layers.lua          # Layer tree manager
│   └── ...                 # Other subsystems
├── external/               # Git submodules (bgfx, lua, soloud, SDL3)
└── shaders/                # Shader source files
```

## Troubleshooting

**"bgfx::init failed"**: Check GPU driver support. On Windows, ensure Direct3D 11
is available. On Linux, ensure Vulkan drivers are installed.

**"SDL_Init failed"**: Verify SDL3 is properly installed and the DLL/so is in
the library path.

**"Embedded shaders failed"**: This is expected — production shaders should be
compiled using bgfx shaderc. The engine runs with debug text overlay in the
meantime.

**Link error "LNK1104 cannot open file"**: A previous instance is still running.
Kill it: `taskkill /F /IM CaesuraAmeKAG.exe` (Windows) or `pkill CaesuraAmeKAG`.

## CI Build (GitHub Actions)

```yaml
- name: Configure
  run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
- name: Build
  run: cmake --build build --config Release -j$(nproc)
```
