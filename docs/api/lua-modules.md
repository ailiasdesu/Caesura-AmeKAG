# Lua Module API — Complete Reference

> C++ binding modules exposed to Lua scripts. All modules are global variables after `Engine::init()`.

---

## Render

```lua
-- Global: Render
-- Backend: BackendRegistry::instance().getRenderDevice()
```

### Texture Management

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `load_texture` | `(path)` | `int` | Load texture from file. Returns 0 on failure. |
| `destroy_texture` | `(id)` | `bool` | Release texture by ID. No-op for invalid IDs. |
| `create_solid_texture` | `(r, g, b, a)` | `int` | Create 1×1 solid colour texture. Returns TextureManager ID. |

### Text Rendering

| Function | Signature | Description |
|----------|-----------|-------------|
| `render_text` | `(text, x, y, scale, r, g, b, a)` | Render text to message layer |
| `render_ruby` | `(text, ruby, x, y)` | Render text with furigana annotation |
| `clear_text` | `()` | Clear all text from message layer |
| `set_font` | `(face, size, color)` | Set font face, size, colour |
| `line_height` | `() → number` | Get current line height in pixels |

### View & Resolution

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_resolution` | `() → w, h` | Get backbuffer width and height |
| `set_view_name` | `(viewId, name)` | Set bgfx view debug marker |
| `resize` | `(w, h)` | Notify engine of window resize |

### Batch Submission

```lua
Render.submit_batch({
    { tex = id, x = 0, y = 0, w = 1280, h = 720, opacity = 255, view = 1 },
    -- "rt" key supported for RTT viewport handles
    { tex = 0, rt = viewportHandle, x = 0, y = 0, w = 640, h = 360, opacity = 255, view = 1 },
})
```

Each quad entry:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `tex` | int | required | TextureManager ID |
| `rt` | int | 0 | Alternative: RTT ViewportHandle ID |
| `x` | float | 0 | Left position |
| `y` | float | 0 | Top position |
| `w` | float | backbuffer width | Quad width |
| `h` | float | backbuffer height | Quad height |
| `opacity` | int (0–255) | 255 | Opacity |
| `view` | int | 1 (VIEW_MAIN) | Target bgfx view |

### Blend / Transition / VFX

| Function | Signature | Description |
|----------|-----------|-------------|
| `submit_blend` | `(baseTexId, blendTexId, mode, baseAlpha, blendAlpha, globalAlpha)` | Submit blend effect |
| `submit_transition` | `(fromTexId, toTexId, ruleTexId, method, progress)` | Submit transition |
| `submit_vfx` | `(srcTexId, effect, fadeAlpha, r, g, b, blur, quakeX, quakeY)` | Submit VFX |
| `stretch_blt` | `(dstTexId, dx,dy,dw,dh, srcTexId, sx,sy,sw,sh, filter)` | Stretch blit (0=Nearest,1=Linear,2=Aniso) |
| `affine_blt` | `(dstTexId, dx,dy,dw,dh, srcTexId, sx,sy,sw,sh, m0..m5)` | Affine 2×3 matrix blit |
| `fill_viewport` | `(vpId, r, g, b, a)` | Fill RTT with solid colour |

### Video Playback

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `video_play` | `(path)` | `handle` | Open and start playing video |
| `video_stop` | `(handle)` | `bool` | Close video and release resources |
| `video_update` | `(handle)` | `bool` | Decode next frame. True if new frame available. |
| `video_get_texture` | `(handle)` | `texId` | Get current frame as texture ID (0=no frame) |
| `video_is_playing` | `(handle)` | `bool` | Is video currently playing |
| `video_has_ended` | `(handle)` | `bool` | Has video reached the end |
| `video_get_size` | `(handle)` | `w, h` | Video dimensions |
| `video_pause` | `(handle)` | `bool` | Pause playback |
| `video_resume` | `(handle)` | `bool` | Resume playback |

### Resource Validation

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `is_valid_handle` | `(type, id)` | `bool` | Validate resource handle. type: 0=Texture,1=Shader,2=RTT,3=Audio,4=Video,5=Font,6=Model,7=Steam |
| `cancel_async_loads` | `()` | `bool` | Cancel all pending async loads |

---

## VFX (Particle System)

```lua
-- Global: VFX
-- Backend: BackendRegistry::instance().getParticleSystem()
```

### Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `particles_init` | `() → bool` | Initialize particle system |
| `particles_shutdown` | `()` | Shutdown and release all particles |
| `particles_is_initialized` | `() → bool` | Check if system is initialized |
| `particles_clear` | `()` | Destroy all emitters and particles |

### Emitters

| Function | Signature | Description |
|----------|-----------|-------------|
| `particles_create_emitter` | `(cfg) → id` | Create emitter. Returns -1 if parameters invalid. |
| `particles_destroy_emitter` | `(id)` | Destroy emitter by ID |
| `particles_emit` | `(id, count)` | Emit N particles from emitter |

Emitter config table (`cfg`):

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `x` | float | 0 | Spawn X position |
| `y` | float | 0 | Spawn Y position |
| `rate` | float | 10 | Particles per second (0 = manual emit only) |
| `lifeMin` | float | 0.5 | Minimum lifetime in seconds |
| `lifeMax` | float | 2.0 | Maximum lifetime in seconds |
| `speedMin` | float | 10 | Minimum initial speed |
| `speedMax` | float | 50 | Maximum initial speed |
| `angleMin` | float | 0 | Minimum emission angle (radians) |
| `angleMax` | float | 6.283 | Maximum emission angle (radians) |
| `sizeMin` | float | 2 | Minimum particle size |
| `sizeMax` | float | 8 | Maximum particle size |
| `r`, `g`, `b`, `a` | float | 1.0 | Colour channels |
| `gravityX` | float | 0 | Horizontal gravity |
| `gravityY` | float | 0 | Vertical gravity |

### Update & Render

| Function | Signature | Description |
|----------|-----------|-------------|
| `particles_update` | `(dt)` | Advance particle simulation by dt seconds |
| `particles_render` | `()` | Submit particle draw calls to GPU |
| `particles_alive_count` | `() → int` | Total active particle count |

---

## Debug

```lua
-- Global: Debug
-- Backend: BackendRegistry::instance().getDebugManager()
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `log` | `(level, message)` | Write to engine log (level: "trace"/"debug"/"info"/"warn"/"error"/"fatal") |
| `assert` | `(condition, message)` | Debug assertion check |
| `traceback` | `() → string` | Get Lua stack traceback |
| `get_fps` | `() → number` | Current frames per second |
| `get_memory` | `() → number` | Lua memory usage in KB |
| `profile_start` | `(name)` | Begin named profiling section |
| `profile_end` | `(name)` | End named profiling section |

---

## DevCore

```lua
-- Global: DevCore
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_version` | `() → string` | Engine version string |
| `get_backend_name` | `(subsystem) → string` | Active backend: "render"/"audio"/"platform" |
| `set_dev_mode` | `(bool)` | Toggle development mode (checkerboard placeholder textures) |
| `reload_scripts` | `()` | Trigger Lua hot reload |

---

## KAG (C++ Audio Bindings)

```lua
-- Global: KAG
-- Direct C++ bindings for audio operations called by KAG command handlers
```

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `play_bgm` | `(file, volume?, loop?)` | `bool` | Play background music |
| `stop_bgm` | `(fadeTime?)` | `bool` | Stop BGM with optional fade (ms) |
| `play_se` | `(file, volume?)` | `bool` | Play sound effect |
| `stop_se` | `()` | `bool` | Stop all sound effects |
| `play_voice` | `(file, volume?)` | `bool` | Play voice line |
| `stop_voice` | `()` | `bool` | Stop current voice |
| `set_global_volume` | `(vol)` | — | Master volume 0.0–1.0 |
| `get_global_volume` | `() → number` | — | Current master volume |
| `set_bus_volume` | `(bus, vol)` | — | Set bus volume: "bgm"/"voice"/"se" |
| `get_bus_volume` | `(bus) → number` | — | Get bus volume |
| `render_text` | `(text, x, y, scale, r, g, b, a)` | — | Render text |
| `clear_text` | `()` | — | Clear text layer |
| `line_height` | `() → number` | — | Current line height |
| `is_bgm_playing` | `() → bool` | — | Is BGM playing |
| `is_voice_playing` | `() → bool` | — | Is voice playing |
| `get_active_voices` | `() → int` | — | Active voice count |
| `log` | `(message)` | — | Write to engine log |

---

## Save

```lua
-- Global: Save
-- Backend: BackendRegistry::instance().getSaveManager()
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `save` | `(slot, data)` | Save game state to slot (int, Lua table) |
| `load` | `(slot) → table` | Load game state from slot |
| `list_saves` | `() → table` | List all save slots with metadata |
| `delete_save` | `(slot)` | Delete save slot |

---

## Steam (Conditional)

```lua
-- Global: Steam
-- Only available when compiled with CAESURA_HAS_STEAM
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `is_available` | `() → bool` | Steam API available |
| `get_user_name` | `() → string` | Steam display name |
| `unlock_achievement` | `(id)` | Unlock achievement by ID |
| `set_rich_presence` | `(key, value)` | Set Rich Presence key-value pair |