# Caesura (AmeKAG) — Project Concepts & Vocabulary

## Engine

### Caesura Engine
A cross-platform visual novel engine built on SDL3 + bgfx + SoLoud + Lua. Renders KAG-scripted stories with layer compositing, blend modes, transitions, and audio buses. All core subsystems run on the main thread; Lua handles game logic through C-binding modules.

### KAG (Kirikiri Adventure Game)
The scripting language parsed by the engine. KAG tags like `[bg]`, `[fg]`, `[play_bgm]` drive layer changes, audio playback, and flow control. The tokenizer (LPeg-based) produces a token stream consumed by the scheduler.

### Backend Registry
A service-locator singleton that holds raw pointers to all active subsystem backends (render, audio, platform). Subsystems register at init; Lua bindings resolve backends at call time. The Engine owns the instances; the Registry only exposes them.

### Layer
A composited visual plane in the rendering pipeline. Three ordered views exist: RTT (render-to-texture, lowest), MAIN (background/foreground/message layers), and DEBUG (overlay, highest). Layers use scissor-based dirty-rectangle optimization — when the union exceeds 75% of the screen, the engine falls back to a full redraw.

### Blend Mode
A pixel-combining rule applied when a foreground layer composites over a background. The engine supports 28 Porter-Duff and Photoshop-style modes, compiled per rendering backend (DX11 HLSL, GLSL bgfx shaderc, Metal). The core VN path uses ~6 common modes; all 28 are available.

### Render-to-Texture (RTT)
An off-screen render target usable as a texture input. RTT canvases enable 3D mini-games and post-processing effects within the 2D VN pipeline. The RTT pool uses spinlock-based concurrency with bounded slots.

---

## Audio

### Bus
A logical audio channel with independent volume control. Three buses exist: BGM (single stream, music), Voice (single stream with interruption, character speech), and SE (pool of 8 concurrent slots, sound effects). Each bus has fade-in/fade-out via SoLoud `fadeVolume`.

### LRU Wave Cache
A bounded in-memory cache for decoded audio waveforms. Uses a `std::list` for access-order tracking with `std::unordered_map` for lookup. When the cache exceeds its limit (128 entries), the least-recently-used waveform is evicted. Replaces the earlier approach of clearing the entire cache on overflow.

---

## Security

### CARC (Caesura ARChive)
A custom container format providing AES-256-GCM encryption, Ed25519 digital signatures, and X.509-style CRL chain-of-trust verification. Chosen over standard ZIP because streaming authenticated encryption with certificate revocation cannot be expressed in standard formats.

### Chain of Trust
A hierarchical certificate model: the root key signs child CARC certificates, which carry expiry timestamps and permission flags. The CRL (Certificate Revocation List) enables key rotation and compromise response without re-encrypting archives.

### Crypto Engine
The encryption/decryption backend. On Windows, uses BCrypt (CNG) for AES-256-GCM; on Linux/macOS, a platform abstraction layer is planned. Key material must be generated via `BCryptGenRandom` (Windows) or `/dev/urandom` (Unix) — non-cryptographic PRNGs like `std::mt19937` are forbidden.

---

## Scripting

### Lua Manager
Owns the single `lua_State`, initializes the Lua VM, registers all C-binding modules, and applies the `lockdownScriptEnv()` sandbox in Release builds. All public methods assert main-thread execution via `CAESURA_ASSERT_MAIN_THREAD()`.

### Binding
A C++ module that exposes a subsystem to Lua as a global table. Bindings follow a naming convention: `*Binding.cpp/h` in `src/Scripting/` (or `src/System/` for save). Examples: `KAGBinding` (KAG tag execution), `RenderBinding` (GPU commands), `VFXBinding` (visual effects dispatch), `SaveBinding` (save/load).

### Sandbox
A Lua environment restriction system with two modes. In Dev mode, `debug.*` is a read-only subset (getinfo, traceback, getlocal, getupvalue, getmetatable). In Release mode, `debug.*` is entirely removed, `require` is locked to `package.loaded` cache only, and `os.execute/os.remove/os.rename/os.exit` plus `io.open/io.popen` are replaced with no-op stubs. The `sandbox.lua` module provides a whitelist-based `_ENV` for `[eval]` and `[emb]` tags.

---

## Rendering

### Embedded Shaders
Shader bytecode compiled into the engine binary at build time. DX11 uses DXBC embedded as C arrays; SPIR-V serves Vulkan; GLSL and Metal sources exist for bgfx shaderc cross-compilation. The `BgfxRenderDevice` creates `bgfx::ProgramHandle` from embedded binaries at init.

### Font Renderer / Text Renderer
Two cooperating subsystems for text display. FontRenderer rasterizes glyphs from TTF files into a texture atlas via FreeType. TextRenderer handles layout, wrapping, and rendering of styled text blocks. Both share a single `FT_Library` instance through the `FreeTypeContext` singleton.

### Particle System
A CPU-driven particle emitter supporting configurable spawn rate, lifetime, velocity, color, and size. Resolution-aware: receives screen dimensions at `update(dt, w, h)` rather than hardcoding values.

### Shader Cache
A registry of `bgfx::ProgramHandle` values indexed by name. Does not own the handles — `BgfxRenderDevice` creates on init and destroys on shutdown. The cache is read-only after initialization.

---

## Resource

### Asset Provider
An abstract interface (`IAssetProvider`) for reading game assets. Providers are ordered in a `ProviderChain` by priority: CARC (10), patch directory, filesystem directory. Each provider answers `read()`, `exists()`, and `verify()`. The `CarcAssetProvider` wraps a `CARCReader` and returns Ed25519 signature verification results.

### Async Loader
A background thread pool for non-blocking asset loading. Textures can be created asynchronously via `createTextureAsync`, decoupling I/O from the render thread.

---

## Systems

### Save Manager
Handles game state persistence as JSON serialization with schema versioning. The migration chain (`registerBuiltinMigrations`) enables forward-compatible save format evolution. Saves include thumbnails and engine version stamps.

### Hot Reload
A file-watching system that detects changes to Lua scripts at runtime. When a script changes, HotReload re-executes it in the Lua VM, enabling live iteration. Mutually exclusive with the debug protocol — only one can be active.

---

## Flagged Ambiguities

- "Backend" had been used for both the abstract interface (`IAudioBackend`) and the concrete implementation (`SoLoudAudioEngine`) — these are distinct. The interface lives in `Core/`, the implementation in its subsystem directory.
- "Layer" in the rendering pipeline vs "Layer" in the Lua `layers.lua` module — same concept, different representation layers. The C++ `LayerManager` drives the GPU; `layers.lua` is the script-facing API.
