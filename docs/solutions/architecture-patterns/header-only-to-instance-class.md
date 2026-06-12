---
title: "Header-only no-op class to instance-based class with DI (NullAnimationBackend pattern)"
date: 2026-06-12
category: architecture-patterns
module: live2d
problem_type: architecture_pattern
component: tooling
severity: medium
applies_when:
  - "A header-only no-op/stub class needs real behavior (e.g., loading resources)"
  - "The new behavior requires access to singletons managed by BackendRegistry"
  - "The class must remain usable without external SDKs (e.g., Cubism SDK)"
tags: [header-only, instance-class, dependency-injection, backend-registry, null-pattern, fallback]
---

# Header-only no-op class to instance-based class with DI

## Context

`NullAnimationBackend` was a header-only class implementing `IAnimationBackend` with
pure no-op methods (all returning `0`, `false`, or doing nothing). This worked for
test environments and SDK-less builds, but provided zero visual feedback — games
without Live2D Cubism SDK could not display any character art.

The requirement was to add PNG static image fallback: when `loadModel("character.png")`
is called, load it as a static texture and display it. This required access to
`TextureManager` and `IRenderDevice`, which are accessed through the project's
central dependency injection hub: `BackendRegistry`.

## Guidance

### Step 1: Move method definitions from header to .cpp

Convert inline `override` methods to declarations in the header, with implementations
in a `.cpp` file. This is necessary because the implementations now need `#include`
directives for `BackendRegistry.h`, `ITextureManager.h`, and other headers that
should not be in the public interface.

```cpp
// Before (header-only, all methods inline):
class NullAnimationBackend : public IAnimationBackend {
public:
    bool init() override { return true; }
    int loadModel(const std::string&, const std::string&) override { return 0; }
    // ...
};

// After (header declares, .cpp implements):
class NullAnimationBackend : public IAnimationBackend {
public:
    bool init() override;
    int  loadModel(const std::string& path, const std::string& name) override;
    // ...
private:
    struct StaticSprite { uint32_t textureId; float x, y, scale, opacity; bool visible; };
    std::unordered_map<int, StaticSprite> m_sprites;
    int m_nextHandle = 1;
    bool m_initialized = false;
};
```

### Step 2: Access singletons via BackendRegistry in .cpp

In the `.cpp` file, include `BackendRegistry.h` and use its getters to access
singletons. Never include concrete implementation headers.

```cpp
// NullAnimationBackend.cpp
#include "../di/BackendRegistry.h"
#include "../render/api/ITextureManager.h"
#include "../render/IRenderDevice.h"

int NullAnimationBackend::loadModel(const std::string& path, const std::string&) {
    if (!isImagePath(path)) return 0;  // keep old behavior for non-images

    auto* tm = BackendRegistry::instance().getTextureManager();
    if (!tm) return 0;  // safe fallback

    uint32_t texId = tm->loadTexture(path);
    if (texId == 0) return 0;

    int handle = m_nextHandle++;
    m_sprites[handle] = {texId};
    return handle;
}
```

### Step 3: Update test project CMakeLists.txt

When a previously header-only class becomes instance-based, the test project must
include the `.cpp` in its source list:

```cmake
# tests/CMakeLists.txt
set(TEST_SOURCES
    ...
    ${CMAKE_SOURCE_DIR}/src/live2d/NullAnimationBackend.cpp
    ...
)
```

### Step 4: Update test assertions

If the class name or behavior changes (e.g., `name()` return value), update
corresponding test assertions:

```cpp
// Before:
CHECK(std::string(anim.name()) == "NullAnimation");

// After:
CHECK(std::string(anim.name()) == "NullAnimation+PNG");
```

## Why This Matters

- **Testability**: The class can be instantiated in test code via `BackendRegistry::instance().registerNullBackends()`
- **Module boundary**: The `.cpp` includes `BackendRegistry.h` but the header only
  includes `IAnimationBackend.h` — no circular dependency
- **SDK independence**: The PNG fallback works without Cubism SDK, and the code
  compiles regardless of `CAESURA_HAS_LIVE2D`
- **Memory safety**: `shutdown()` iterates `m_sprites` and calls `TextureManager::destroyTexture()`
  on each loaded texture

## When to Apply

- When a Null/stub class needs to graduate from pure no-op to limited real behavior
- When the real behavior needs singleton access via `BackendRegistry`
- When the class must still compile and work without optional external SDKs
- When adding state (member variables) to a previously stateless class

## Examples

Same pattern applies to other stub classes in the project:
- `NullMiniGameBackend` — could load static 3D placeholder scenes
- `NullAudioBackend` — could play simple WAV files without SoLoud
- `NullSteamBackend` — could provide mock achievement data for testing

The pattern is: header declares interface + private state, `.cpp` implements via
`BackendRegistry::instance().get*()`, test CMakeLists.txt includes the `.cpp`.
