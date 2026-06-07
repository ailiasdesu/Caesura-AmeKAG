# Caesura (AmeKAG) — Complete KAG API Reference

> **Generated:** 2026-06-08
> **Engine version:** Alpha (C++20, SDL3 + bgfx + SoLoud + Lua 5.4)
> **Target audience:** AI coding assistants (Codex, Copilot, Cursor) and KAG script developers
> **Coverage:** 7 modules, 98+ functions, all parameter signatures, return values, side effects, and usage examples

## Quick Reference - By Use Case

| I want to... | Use |
|---|---|
| Play background music | KAG.play_bgm(path, fadeTime) |
| Play a voice line | KAG.play_voice(path) |
| Play a sound effect | KAG.play_se(path) |
| Show character dialogue | KAG.show_text(text) |
| Show a character sprite | KAG.show_image("fg", path, x, y, sx, sy, opacity) |
| Show a background | KAG.show_image("bg", path, 0, 0) |
| Clear the screen | KAG.clear_screen("bg") or KAG.clear_screen() |
| Wait for player click | KAG.wait_click() |
| Screen shake effect | KAG.quake(duration_ms, intensity) |
| Render text at position | KAG.render_text(text, x, y, r, g, b, a) |
| Ruby/furigana text | KAG.render_ruby(text, ruby, x, y, r, g, b, a) |
| Change font size | KAG.set_font(fontId) - 0=Small, 1=Large, 2=TTF |
| Log a debug message | KAG.log("info", message) |
| Load a texture manually | Render.load_texture(path) |
| Batch-render quads | Render.submit_batch({...}) |
| Blend two textures | Render.submit_blend(baseTex, blendTex, mode, ...) |
| Create particle emitter | VFX.particles_create_emitter({...}) |
| Change resolution | DevCore.set_resolution(w, h) |
| Toggle fullscreen | DevCore.set_fullscreen(true) |
| Quit the game | DevCore.quit() |
| Save game | KAG.save_game(slot, jsonData, sceneName, tokenIndex) |
| Load game | KAG.load_game(slot) |
| List all saves | KAG.list_saves() |
| Engine debug info | Debug.get_stats() -> all subsystems at once |
| Full error report | Debug.dump_report() -> JSON |

---
## Module 1: KAG - Core Visual Novel Commands

> **Global table:** ``KAG``
> **Functions:** 35 registered (audio, visual, save, utility)
> **File paths** go through IAssetProvider chain (directory -> XP3 -> CARC).

### 1.1 Audio - BGM

#### KAG.play_bgm(path, fadeTime)
Play background music with optional cross-fade.
- **path** (string, required) - Audio file (WAV/MP3/OGG)
- **fadeTime** (number, default 1.0) - Cross-fade seconds
- **Returns:** number - Audio handle, or false if backend unavailable
- **Side effects:** Starts playback on "bgm" bus. Cross-fades if already playing.

```lua
KAG.play_bgm("assets/bgm/morning.ogg")
KAG.play_bgm("assets/bgm/battle.ogg", 0.5)
```

#### KAG.stop_bgm(fadeTime)
Stop BGM with fade-out.
- **fadeTime** (number, default 1.0) - Fade seconds
- **Returns:** boolean

#### KAG.set_bgm_volume(volume)
- **volume** (number) 0.0-1.0
- **Returns:** boolean

#### KAG.is_bgm_playing()
- **Returns:** boolean

### 1.2 Audio - Voice

#### KAG.play_voice(path)
Play voice. **Absolute interrupt** - stops any playing voice.
- **path** (string, required)
- **Returns:** number - Audio handle
- **Side effects:** Kills active voice on "voice" bus.

#### KAG.stop_voice()
- **Returns:** boolean

#### KAG.replay_voice()
Replay last voice line from beginning.
- **Returns:** boolean

#### KAG.is_voice_playing()
- **Returns:** boolean

#### KAG.get_active_voices()
- **Returns:** number - count of active voice handles

#### KAG.set_voice_volume(volume)
- **volume** (number) 0.0-1.0
- **Returns:** boolean

### 1.3 Audio - Sound Effects (SE)

#### KAG.play_se(path)
Play 2D sound effect.
- **path** (string, required)
- **Returns:** number - Audio handle

```lua
KAG.play_se("assets/se/click.wav")
```

#### KAG.play_se_3d(path, x, y, z)
Play spatial sound at 3D position.
- **path** (string, required)
- **x, y, z** (number) - World position
- **Returns:** number - Audio handle

#### KAG.stop_se()
Stop all active SE immediately.
- **Returns:** boolean

#### KAG.set_se_volume(volume)
- **volume** (number) 0.0-1.0
- **Returns:** boolean

### 1.4 Audio - Bus Control (Advanced)

#### KAG.set_global_volume(volume)
Master volume for all audio.
- **volume** (number) 0.0-1.0
- **Returns:** number - new volume

#### KAG.get_global_volume()
- **Returns:** number

#### KAG.set_bus_volume(bus, volume)
- **bus** (string) - "bgm", "voice", or "se"
- **volume** (number) 0.0-1.0
- **Returns:** boolean

#### KAG.get_bus_volume(bus)
- **bus** (string) - "bgm", "voice", or "se"
- **Returns:** number

#### KAG.audio_get_position(bus)
Get playback position in seconds.
- **bus** (string) - "bgm" or "voice"
- **Returns:** number - seconds, 0.0 if unavailable

#### KAG.audio_get_length(bus)
Get total track length in seconds.
- **bus** (string) - "bgm" or "voice"
- **Returns:** number

#### KAG.audio_fade_volume(bus, targetVolume, fadeTime)
Smoothly fade bus volume.
- **bus** (string) - "bgm", "voice", or "se"
- **targetVolume** (number) 0.0-1.0
- **fadeTime** (number) - seconds
- **Returns:** boolean

```lua
KAG.audio_fade_volume("bgm", 0.3, 3.0)  -- fade to 30%
KAG.audio_fade_volume("bgm", 1.0, 2.0)  -- fade back to full
```

#### KAG.set_listener(posX, posY, posZ, atX, atY, atZ, upX, upY, upZ)
Update 3D audio listener.
- **posX/Y/Z** (number, required) - Listener position
- **atX/Y/Z** (number, required) - Look-at direction
- **upX/Y/Z** (number, default 0,1,0) - Up vector
- **Returns:** boolean

#### KAG.flush_wave_cache()
Clear audio waveform cache to free memory.
- **Returns:** none

### 1.5 Visual - Image Display

#### KAG.show_image(layer, path, x, y, sx, sy, opacity, blend)
Display image on a layer.
- **layer** (string, required) - "bg", "fg", or "msg"
- **path** (string, required) - Image file path
- **x, y** (number, required) - Position in pixels
- **sx, sy** (number, default 1.0) - Scale
- **opacity** (number, default 1.0) - 0.0 to 1.0
- **blend** (number, default 0) - 0=Normal, 1=Multiply, 2=Screen, 3=Overlay
- **Returns:** boolean

```lua
KAG.show_image("bg", "assets/bg/classroom.png", 0, 0)
KAG.show_image("fg", "assets/char/heroine.png", 640, 360, 0.8, 0.8)
KAG.show_image("msg", "assets/ui/panel.png", 0, 540, 1.0, 1.0, 0.7)
```

#### KAG.clear_screen(layer)
Clear layers.
- **layer** (string, optional) - "bg", "fg", "msg", or nil (all)
- **Returns:** none

#### KAG.clear_text_layer()
Alias for KAG.clear_text().

### 1.6 Visual - Text Rendering

#### KAG.show_text(text)
Display text in message box.
- **text** (string, required)
- **Returns:** boolean

```lua
KAG.show_text("Heroine: Hello!")
KAG.wait_click()
```

#### KAG.render_text(text, x, y, r, g, b, a)
Render text at screen position (NOT message box).
- **text** (string, required)
- **x, y** (number, required) - Screen position
- **r, g, b, a** (number, 0.0-1.0) - Color/alpha
- **Returns:** boolean

```lua
KAG.render_text("-42", 500, 200, 1.0, 0.2, 0.2, 1.0)
KAG.render_text("Yuki", 100, 30, 0.9, 0.9, 1.0, 0.8)
```

#### KAG.render_ruby(text, ruby, x, y, r, g, b, a)
Render text with ruby/furigana annotation.
- **text** (string, required) - Main text
- **ruby** (string, required) - Ruby reading
- **x, y** (number, required)
- **r, g, b, a** (number, 0.0-1.0)
- **Returns:** boolean

#### KAG.clear_text()
Clear rendered text from font page.
- **Returns:** none

#### KAG.set_font(fontId)
Set active font.
- **fontId** (number) - 0=Small, 1=Large, 2=TTF
- **Returns:** boolean

#### KAG.line_height()
- **Returns:** number - Line height in pixels

### 1.7 Visual - Effects

#### KAG.quake(durationMs, intensity)
Screen shake effect.
- **durationMs** (number, required) - Duration in ms
- **intensity** (number, default 5.0) - Pixel offset
- **Returns:** boolean

```lua
KAG.quake(500, 3.0)   -- light emphasis
KAG.quake(1000, 12.0) -- heavy impact
```

### 1.8 Input Control

#### KAG.wait_click()
Pause script until user clicks/presses key.
- **Returns:** none
- **Side effects:** Blocks coroutine until InputRouter detects click with KAG focus.

### 1.9 Utility

#### KAG.log(level, message)
Log message through engine debug.
- **level** (string) - "trace", "debug", "info", "warn", "error", "fatal"
- **message** (string)
- **Returns:** none

### 1.10 Save/Load System

> Registered by SaveBinding on the KAG table.

#### KAG.save_game(slot, jsonData, sceneName, tokenIndex, thumbnail)
Save game state.
- **slot** (number, required) - Slot 1+
- **jsonData** (string, required) - JSON state
- **sceneName** (string, required) - Scene ID
- **tokenIndex** (number, required) - Resumption point
- **thumbnail** (string, optional) - Base64 thumbnail
- **Returns:** boolean

#### KAG.load_game(slot)
Load game state.
- **slot** (number, required)
- **Returns:** string, table - JSON data + metadata {slot, timestamp, scene, token_index, schema_version}, or nil, error

#### KAG.list_saves()
- **Returns:** table - Array of {slot, timestamp, scene, token_index, schema_version}

#### KAG.delete_save(slot)
- **Returns:** boolean

#### KAG.save_exists(slot)
- **Returns:** boolean

#### KAG.get_save_dir()
- **Returns:** string - "saves/"

---
## Module 2: Render - Low-Level Rendering

> **Global table:** ``Render``
> **Functions:** 28 registered (20 active + 8 video stubs)
> **Purpose:** Texture management, viewports, batch quads, blend ops, transitions.

### 2.1 Texture Management

#### Render.load_texture(path)
Load image as GPU texture.
- **path** (string, required)
- **Returns:** number - Texture ID, or nil, "Failed to load texture"

#### Render.load_texture_async(path)
Enqueue async texture load. Returns request ID immediately.
- **path** (string, required)
- **Returns:** number - Request ID

#### Render.cancel_async_loads()
Cancel all pending async loads.
- **Returns:** boolean

#### Render.create_solid_texture(r, g, b, a)
Create 1x1 solid-color GPU texture.
- **r, g, b** (number, 0-255, required)
- **a** (number, 0-255, default 255)
- **Returns:** number - Texture ID, or nil, "GPU solid tex failed"

```lua
local white = Render.create_solid_texture(255, 255, 255, 255)
local red   = Render.create_solid_texture(255, 0, 0, 128)
```

#### Render.destroy_texture(texId)
Release GPU texture.
- **texId** (number, required)
- **Returns:** boolean

### 2.2 Viewport Control

#### Render.create_viewport(name, x, y, w, h, scale, clearColor)
Create named render-to-texture viewport.
- **name** (string, required) - Unique name
- **x, y, w, h** (number) - Rectangle in pixels
- **scale** (number, default 1.0)
- **clearColor** (number, default 0x000000FF) - RGBA8 packed
- **Returns:** number - Viewport ID

#### Render.destroy_viewport(viewportId)
Destroy viewport and RTT resources.
- **viewportId** (number, required)
- **Returns:** boolean

#### Render.draw_viewport(viewportId, x, y, w, h, opacity)
Blit viewport contents to screen.
- **viewportId** (number, required)
- **x, y, w, h** (number) - Dest rectangle
- **opacity** (number, default 255) - 0-255
- **Returns:** boolean

#### Render.fill_viewport(viewportId, color)
Fill viewport with solid color.
- **viewportId** (number, required)
- **color** (number) - RGBA8 packed
- **Returns:** boolean

### 2.3 Batch Rendering

#### Render.submit_batch(quadTable)
Submit multiple quads in single draw call.
- **quadTable** (table, required) - Array of {tex, x, y, w, h, opacity}
- **Returns:** boolean

```lua
Render.submit_batch({
    { tex = panel, x = 0,   y = 540, w = 1280, h = 180, opacity = 200 },
    { tex = btn,   x = 1000, y = 600, w = 160,  h = 80,  opacity = 255 },
})
```

#### Render.submit_blend(baseTex, blendTex, mode, baseAlpha, blendAlpha, globalAlpha)
Blend two textures with mode.
- **baseTex, blendTex** (number, required)
- **mode** (number, required) - Blend mode index
- **baseAlpha, blendAlpha** (number, default 255) - 0-255
- **globalAlpha** (number, default 255) - 0-255
- **Returns:** boolean

#### Render.submit_transition(fromTex, toTex, progress, type)
Animated transition between textures.
- **fromTex, toTex** (number, required)
- **progress** (number, 0.0-1.0)
- **type** (number) - Transition type index
- **Returns:** boolean

#### Render.submit_vfx(effectTex, params)
Post-processing VFX pass.
- **effectTex** (number, required)
- **params** (table) - Effect parameters
- **Returns:** boolean

### 2.4 Texture Blitting (Low-Level)

#### Render.stretch_blt(texId, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH)
Blit texture region with stretch.
- **texId** (number, required)
- **srcX/Y/W/H** (number) - Source rect
- **dstX/Y/W/H** (number) - Dest rect
- **Returns:** boolean

#### Render.affine_blt(texId, x, y, w, h, angle, cx, cy)
Blit with rotation around center.
- **texId** (number, required)
- **x, y, w, h** (number) - Dest rect
- **angle** (number) - Radians
- **cx, cy** (number) - Rotation center (relative to dest)
- **Returns:** boolean

```lua
Render.affine_blt(logoTex, 640, 360, 256, 256, math.pi * 0.25, 128, 128)
```

### 2.5 Resolution and Info

#### Render.get_resolution()
- **Returns:** number, number - Backbuffer width, height

#### Render.resize(w, h)
Resize renderer (bgfx::reset + view re-setup).
- **w, h** (number, required)
- **Returns:** boolean

#### Render.set_view_name(viewId, name)
Set debug name for bgfx view.
- **viewId** (number, required)
- **name** (string, required)
- **Returns:** boolean

### 2.6 Resource Validation

#### Render.is_valid_handle(handleType, id)
Validate resource handle.
- **handleType** (number, required) - 0=TEXTURE, 1=VIEWPORT, 2=RTT
- **id** (number, required)
- **Returns:** boolean

#### Render.invalidate_handles(handleType)
Invalidate all handles of type (e.g. after backend switch).
- **handleType** (number, required)
- **Returns:** boolean

### 2.7 Video Placeholders (NOT READY)

> **Warning:** Video not available in Alpha. video_play throws error. Others return safe defaults.

| Function | Returns |
|---|---|
| Render.video_stop() | true |
| Render.video_update() | true |
| Render.video_get_texture() | 0 |
| Render.video_is_playing() | false |
| Render.video_has_ended() | false |
| Render.video_get_size() | 0, 0 |
| Render.video_pause() | true |
| Render.video_resume() | true |

---

## Module 3: VFX - Particle System

> **Global table:** ``VFX``
> **Functions:** 10 registered
> **Lifecycle:** init -> create_emitter -> update(per-frame) -> render(per-frame) -> shutdown
> **Singleton:** One ParticleSystem managed by VFXBinding.

### 3.1 Lifecycle

#### VFX.particles_init()
Initialize particle system. Safe to call multiple times.
- **Returns:** boolean
- **Side effects:** Allocates GPU particle buffers.

#### VFX.particles_shutdown()
Shut down and release GPU resources.
- **Returns:** boolean
- **Side effects:** Destroys all emitters and particles.

#### VFX.particles_clear()
Reset: shutdown + re-init. Clears all particles, keeps system warm.
- **Returns:** boolean

#### VFX.particles_is_initialized()
- **Returns:** boolean

### 3.2 Emitter Control

#### VFX.particles_create_emitter(cfg)
Create particle emitter with config table.

| Field | Type | Default | Description |
|---|---|---|---|
| x, y | number | 0 | Emitter screen position |
| rate | number | 10 | Particles per second |
| lifeMin | number | 0.5 | Min lifetime (seconds) |
| lifeMax | number | 2.0 | Max lifetime (seconds) |
| speedMin | number | 10 | Min initial speed |
| speedMax | number | 50 | Max initial speed |
| angleMin | number | 0 | Min emission angle (radians) |
| angleMax | number | 6.283 | Max emission angle (2*pi) |
| sizeMin | number | 2 | Min particle size (px) |
| sizeMax | number | 8 | Max particle size (px) |
| r, g, b, a | number | 1.0 | Color (0.0-1.0) |
| gravityX | number | 0 | Horizontal gravity |
| gravityY | number | 0 | Vertical gravity |

- **Returns:** number - Emitter ID

```lua
VFX.particles_init()
-- Cherry blossom petals
local petals = VFX.particles_create_emitter({
    x = 640, y = -10,  rate = 5,
    lifeMin = 3.0, lifeMax = 6.0,
    speedMin = 20, speedMax = 60,
    angleMin = 1.2, angleMax = 2.0,
    sizeMin = 3, sizeMax = 7,
    r = 1.0, g = 0.7, b = 0.8, a = 0.8,
    gravityY = 10,
})
-- Burst effect
VFX.particles_emit(petals, 50)
```

#### VFX.particles_destroy_emitter(emitterId)
- **emitterId** (number, required)
- **Returns:** boolean

#### VFX.particles_emit(emitterId, count)
Manual burst emission.
- **emitterId** (number, required)
- **count** (number, required)
- **Returns:** boolean

### 3.3 Per-Frame Update

#### VFX.particles_update(dt)
Update all particles. **Must call every frame.**
- **dt** (number, required) - Delta time seconds
- **Returns:** boolean

#### VFX.particles_render(viewId)
Render active particles to bgfx view.
- **viewId** (number, required) - Usually 0
- **Returns:** boolean

### 3.4 Query

#### VFX.particles_alive_count()
- **Returns:** number - Total alive particles

---

## Module 4: _CAESURA_BACKEND - Unified Proxy

> **Global table:** ``_CAESURA_BACKEND``
> **Functions:** 7 registered (delegation proxy)
> **Purpose:** Single entry point delegating to Render/KAG/DevCore through BackendRegistry.

### 4.1 Subsystem Commands

#### _CAESURA_BACKEND.render(cmd, ...)
Route to Render or VFX table.
- **cmd** (string, required)
- **...** - Forwarded args
- **Returns:** depends on target

Supported: create_viewport, destroy_viewport, draw_viewport, fill_viewport, load_texture, destroy_texture, create_solid_texture, get_resolution, submit_batch, submit_blend, submit_transition, submit_vfx, stretch_blt, affine_blt, clear_text, render_ruby, set_font, line_height, resize, load_texture_async, cancel_async_loads, particles_init, particles_shutdown, particles_create_emitter, particles_destroy_emitter, particles_emit, particles_update, particles_render, particles_alive_count, particles_clear

#### _CAESURA_BACKEND.audio(cmd, ...)
Route to KAG audio commands with backend-aware routing.
- **cmd** (string, required)
- **...** - Forwarded args

Supported: play_bgm, stop_bgm, play_voice, stop_voice, play_se, play_se_3d, stop_se, set_global_volume, get_global_volume, set_bus_volume, get_bus_volume, flush_wave_cache, is_playing (bus-routed), get_position, get_length, fade_volume

#### _CAESURA_BACKEND.platform(cmd, ...)
Route to DevCore commands.
- **cmd** (string, required)

Supported: log, get_resolution, set_input_focus, get_input_focus, set_fullscreen

### 4.2 Convenience Methods

#### _CAESURA_BACKEND.show_text(text)
Canonical text display implementation.
- **text** (string, required)
- **Returns:** boolean

#### _CAESURA_BACKEND.show_image(layer, path, x, y, sx, sy, opacity, blend)
- Same signature as KAG.show_image

#### _CAESURA_BACKEND.clear_screen(layer)
- Same as KAG.clear_screen

#### _CAESURA_BACKEND.wait_click()
- Same as KAG.wait_click

### 4.3 Async Callback System

```lua
_CAESURA_BACKEND.load_texture_async("assets/bg/huge.png", function(texId)
    if texId then
        _CAESURA_BACKEND.render("submit_batch", {
            { tex = texId, x = 0, y = 0, w = 1280, h = 720 }
        })
    end
end)
```

---

## Module 5: DevCore - Engine Control

> **Global table:** ``DevCore``
> **Functions:** 8 registered

### 5.1 Display

#### DevCore.set_resolution(w, h)
Change resolution at runtime. Resizes bgfx + SDL window.
- **w, h** (number, required) - Pixels
- **Returns:** boolean (false if w/h <= 0)
- **Side effects:** bgfx::reset() + SDL_SetWindowSize()

#### DevCore.get_resolution()
- **Returns:** number, number - width, height

#### DevCore.get_window_size()
**Alias for get_resolution().**

#### DevCore.set_fullscreen(enabled)
Toggle fullscreen.
- **enabled** (boolean, required)
- **Returns:** boolean
- **Side effects:** SDL_SetWindowFullscreen()

### 5.2 Input Focus

#### DevCore.set_input_focus(mode)
Switch input routing: KAG (visual novel) or GAME (mini-game).
- **mode** (string, required) - "KAG" or "GAME" (case-insensitive)
- **Returns:** boolean, [error]

```lua
DevCore.set_input_focus("GAME")  -- entering mini-game
DevCore.set_input_focus("KAG")   -- back to dialogue
```

#### DevCore.get_input_focus()
- **Returns:** string - "KAG" or "GAME"

### 5.3 Lifecycle

#### DevCore.quit()
Request graceful engine shutdown.
- **Returns:** none
- **Side effects:** Sets _CAESURA_QUIT = true

#### DevCore.log(msg)
Log with [DevCore] prefix.
- **msg** (string, required)
- **Returns:** none

---

## Module 6: Debug - Diagnostics and Telemetry

> **Global table:** ``Debug``
> **Functions:** 10 registered

### 6.1 Error Inspection

#### Debug.get_last_error()
- **Returns:** table | nil - {message, code, code_name, subsystem, level}

#### Debug.get_error_count()
- **Returns:** number

### 6.2 Subsystem Stats

#### Debug.get_subsystem_stats(name)
- **name** (string) - "render", "audio", "scripting", "input", "platform", "engine"
- **Returns:** table - {total_calls, errors, warns, last_error_code, last_error_msg}

### 6.3 Structured Info

#### Debug.get_render_info()
- **Returns:** table - {backend, width, height, views, textures, rtts, shader_ready}

#### Debug.get_audio_info()
- **Returns:** table - {initialized, active_voices, active_bgm, global_volume, bgm_bus_ready, voice_bus_ready, se_bus_ready}

#### Debug.get_input_info()
- **Returns:** table - {focus, kag_callbacks, game_callbacks, click_pending}

### 6.4 Bulk Reporting

#### Debug.get_stats()
Aggregate all subsystems in one call. **Preferred for AI diagnostics.**
- **Returns:** table - {error_count, render={...}, audio={...}, input={...}}

```lua
local stats = Debug.get_stats()
if stats.error_count > 0 then
    KAG.log("warn", "Errors: " .. stats.error_count)
end
```

#### Debug.dump_report()
Full JSON diagnostic report.
- **Returns:** string - JSON report

#### Debug.get_log_path()
- **Returns:** string - Path to engine log file

### 6.5 Logging

#### Debug.log(level, message)
Log through DebugManager.
- **level** (string) - "info", "warn"/"warning", "error"
- **message** (string)
- **Returns:** none

---

## Module 7: Engine Globals

> Read-only environment variables set by C++ via lua_setglobal.

| Global | Type | Source | Description |
|---|---|---|---|
| _GAME_MOUSE_X | number | SDL poll | Mouse X per frame |
| _GAME_MOUSE_Y | number | SDL poll | Mouse Y per frame |
| _GAME_MOUSE_DOWN | boolean | SDL poll | Mouse pressed |
| _GAME_KEY_F5 | boolean | SDL poll | Quick save key |
| _GAME_KEY_F6 | boolean | SDL poll | Quick load key |
| _CAESURA_GPU_VENDOR | string | bgfx caps | GPU vendor |
| _CAESURA_GPU_RENDERER | string | bgfx caps | GPU name |
| _CAESURA_VOICE_COMPLETE | boolean | Audio backend | Voice finished |
| _CAESURA_CONFIG | table | config.lua | Engine config |
| _CAESURA_QUIT | boolean | Engine signal | Set by DevCore.quit() |
| _ASYNC_CALLBACKS | table | Async loader | Maps reqId -> callback |

---

## Sandbox Constraints

> Full rules: ``scripts/sandbox.lua``. Design: DEFAULT DENY, EXPLICIT ALLOW.

### Disabled
- loadfile, dofile - arbitrary file exec
- os.execute, os.remove, os.rename, os.exit - FS mutation
- io.open, io.popen - direct I/O

### Restricted
- os.clock/date/time/difftime - allowed (non-I/O)
- debug.* - read-only subset only
- require - only preloaded modules
- package.searchers - empty (no FS search)

### _G Write Protection
- __newindex metamethod blocks new globals
- Whitelist: KAG, Render, VFX, DevCore, _CAESURA_BACKEND, _CAESURA_CONFIG, _SANDBOX_RESOURCES, _SANDBOX_CHECK, math, string, table, coroutine
- Unlisted: "Sandbox: cannot create global 'X'" error

### Resource Budgets
```lua
_SANDBOX_RESOURCES = {
    textures_loaded   = 0,  textures_max  = 256,
    audio_handles     = 0,  audio_max     = 64,
    rtt_canvases      = 0,  rtt_max       = 8,
    particles_emitters = 0, particles_max = 16,
}
_SANDBOX_CHECK("textures")  -- errors at limit
```

### CPU Budget
- 500,000 Lua instructions per frame
- Exceeding: "Sandbox: instruction budget exceeded"

---

## Quick Scene Template

```lua
-- Copy for every new scene
local scene_name = "scene_id"

function scene_start()
    KAG.log("info", "Scene: " .. scene_name .. " started")
    KAG.clear_screen()
    KAG.show_image("bg", "assets/bg/classroom.png", 0, 0)
    KAG.play_bgm("assets/bgm/morning.ogg", 1.0)
    KAG.show_image("fg", "assets/char/heroine_smile.png", 640, 360, 0.8, 0.8)
    KAG.show_text("Heroine: I've been waiting for you.")
    KAG.wait_click()
    KAG.show_text("Heroine: The cherry blossoms are beautiful today.")
    KAG.wait_click()
    KAG.play_se("assets/se/bell.wav")
    KAG.quake(300, 3.0)
end

function scene_end()
    KAG.stop_bgm(1.0)
    KAG.clear_screen()
end
```

---

## API Coverage Summary

| Module | Functions | Status |
|---|---|---|
| KAG (Core VN) | 35 audio + visual + save | Complete |
| Render | 20 active + 8 stubs | Complete (video=Alpha stub) |
| VFX | 10 particles | Complete |
| _CAESURA_BACKEND | 7 proxy methods | Complete |
| DevCore | 8 engine control | Complete |
| Debug | 10 diagnostics | Complete |
| Engine Globals | 11 variables | Documented |

**Total documented API surface:** 98 functions + 11 globals across 7 modules.
