# Getting Started with Caesura (AmeKAG)

## Requirements

- **Windows**: Visual Studio 2022, CMake 3.25+, vcpkg
- **Linux (Ubuntu 24.04)**: GCC 13+, CMake 3.25+, SDL3, FreeType, zstd, OpenSSL
- **macOS**: Xcode 15+, CMake 3.25+, Homebrew

## Quick Build

### Windows

```powershell
# Install SDL3 via vcpkg
vcpkg install sdl3 --triplet x64-windows

# Configure
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"

# Build + Run
cmake --build build --config Debug --parallel
```

### Linux

```bash
# Install dependencies
sudo apt-get install cmake build-essential libfreetype-dev libzstd-dev \
  libssl-dev libx11-dev libpulse-dev libasound2-dev

# Build SDL3 from source
wget https://github.com/libsdl-org/SDL/releases/download/release-3.2.0/SDL3-3.2.0.tar.gz
tar xzf SDL3-3.2.0.tar.gz && cd SDL3-3.2.0
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local && sudo cmake --install build

# Build engine
cd .. && cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### macOS

```bash
brew install cmake sdl3 freetype zstd openssl@3

cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)"
cmake --build build --parallel
```

## Run Tests

```bash
# From project root
cd build/tests/Debug   # Windows
# cd build/tests        # Linux/macOS
./CaesuraTests
```

## Project Structure

```
Caesura(AmeKAG)/
├── src/                  # C++ engine source
│   ├── audio/            # SoLoud audio backend
│   ├── archive/          # CARC format + crypto
│   ├── entry/            # Engine composition root
│   ├── render/           # bgfx rendering
│   ├── script/           # Lua VM + KAG bindings
│   ├── storage/          # Save/load system
│   └── ...
├── scripts/              # Lua game scripts
│   ├── kag/              # KAG command parser + scheduler
│   ├── demo.lua          # Demo scene
│   └── demo_story.ks     # KAG demo script
├── tests/                # Test suite
│   ├── cpp/              # C++ tests (doctest)
│   └── scripts/          # Test KAG scripts
├── external/             # Third-party libraries
│   ├── SDL3/             # SDL3 prebuilt (Windows)
│   ├── bgfx/             # bgfx rendering library
│   ├── soloud/           # SoLoud audio engine
│   └── lua/              # Lua 5.4
└── docs/                 # Documentation
    ├── api/              # API references
    └── guides/           # User guides
```

## Your First Scene

Create a file `my_first_scene.ks`:

```kag
; my_first_scene.ks — Your first KAG scene

*start
[bg storage="bg/school.png"]
[wait time=500]

[ch name="Hero" text="Hello, this is my first visual novel scene!"]
[p]

[playbgm storage="bgm/title.ogg" volume=0.8]
[ch name="Hero" text="Background music starts playing..."]
[p]

[fg storage="chara/hero_smile.png" layer=1]
[position layer="fg0" x=0.5 y=0.3 scale=1.0]
[ch name="Hero" text="Here is a character sprite!"]
[p]

[stopbgm time=500]
[end]
```

## Running Your Script

The engine loads scripts from the `scripts/` directory. Place your `.ks` files there
and reference them from `scripts/demo.lua` or your own game entry point.

## Next Steps

- Read the [KAG Command Reference](api/kag-commands.md) for all available commands
- Read the [Lua Module API](api/lua-modules.md) for scripting APIs
- Study `scripts/demo_story.ks` for a complete example
