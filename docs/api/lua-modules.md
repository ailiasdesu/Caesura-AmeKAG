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
| `text_set_font` | `(face, size, color?)` | Set font face/size; `"default"` resets to the built-in bitmap font |
| `text_reset_state` | `()` | Reset the text renderer's internal line/char state |

> The text entry-points `render_text`, `render_ruby`, `clear_text`, `set_font`,
> and `line_height` live on the **KAG** module (see [KAG section](#kag-c-audio-bindings)),
> not on Render.

### View & Resolution

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_resolution` | `() → w, h` | Get backbuffer width and height |
| `set_view_name` | `(viewId, name)` | Set bgfx view debug marker |
| `set_screen_offset` | `(dx, dy)` | Pan VIEW_MAIN (camera/quakes); fractional values are rounded |
| `create_viewport` | `(w, h) → handle` | Create an RTT viewport (0 on invalid dimensions) |
| `destroy_viewport` | `(handle)` | Destroy an RTT viewport |
| `draw_viewport` | `(handle, x, y, w?, h?)` | Blit an RTT viewport onto VIEW_MAIN |
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
| `w` | float | 128 | Quad width |
| `h` | float | 128 | Quad height |
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
| `set_color_filter` | `(preset)` | Accessibility colour filter: none/deuteranopia/protanopia/tritanopia/grayscale/high_contrast |

### Video Playback

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `video_play` | `(path)` | `handle` | Open and start playing video |
| `video_stop` | `(handle)` | `bool` | Close video and release resources |
| `video_update` | `(handle)` | `bool` | Decode next frame (manual drive; the engine frame loop already advances all playing videos automatically with frame-rate pacing). |
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
| `load_texture_async` | `(path, callback?)` | `int` | Enqueue an async texture load (`<0` on error); optional `(ok, path, texId)` callback |
| `cancel_async_loads` | `()` | `bool` | Cancel all pending async loads |
| `invalidate_handles` | `(type)` | `bool` | Invalidate cached resource generation handles by type |

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

## MiniGame (3D)

```lua
-- Global: mini_game
-- Backend: BackendRegistry::instance().getMiniGameBackend()
```

Programmatic 3D scenes: spawn objects/materials/lights from Lua, then
`enter(0)` to activate (renders the spawned object set without a JSON scene).
JSON scenes load via `load_scene(path)` + `enter(handle)`.

### Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `load_scene` | `(path) → handle` | Load a JSON scene descriptor; 0 on failure |
| `unload_scene` | `(handle)` | Unload a loaded scene |
| `enter` | `(handle)` | Activate a scene; `0` activates spawned objects (programmatic mode) |
| `leave` | `()` | Deactivate the active scene |
| `is_active` | `() → bool` | True while a scene is active |

### Objects

| Function | Signature | Description |
|----------|-----------|-------------|
| `spawn_cube` | `(x, y, z, scale?, r?, g?, b?, matId?) → id` | Spawn a cube |
| `spawn_sphere` | `(x, y, z, scale?, r?, g?, b?, matId?) → id` | Spawn a sphere |
| `spawn_plane` | `(x, y, z, w?, h?, r?, g?, b?, matId?) → id` | Spawn a plane |
| `remove_object` | `(id)` | Remove an object |
| `set_material` | `(objId, matId)` | Assign a material to an object |
| `set_velocity` | `(id, vx, vy, vz)` | Set linear velocity |
| `set_gravity` | `(id, enabled)` | Toggle gravity for an object |
| `set_camera` | `(eyeX, eyeY, eyeZ, atX, atY, atZ)` | Position the camera |

### Materials & Lighting

| Function | Signature | Description |
|----------|-----------|-------------|
| `create_material` | `(r, g, b, rough?, metal?, spec?, name?) → id` | Create a PBR material |
| `set_ambient` | `(r, g, b)` | Ambient light color |
| `set_directional` | `(x, y, z, r?, g?, b?, intensity?)` | Directional light |
| `add_point_light` | `(x, y, z, r?, g?, b?, intensity?, range?, name?) → id` | Add a point light |
| `remove_light` | `(id)` | Remove a point light |

### Physics

| Function | Signature | Description |
|----------|-----------|-------------|
| `check_collision` | `(idA, idB) → bool` | Test object pair collision |
| `set_collision` | `(enabled)` | Toggle the collision system |

## Debug

```lua
-- Global: Debug
-- Backend: BackendRegistry::instance().getDebugManager()
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `log` | `(level, message)` | Write to engine log (log_level + message) |
| `get_last_error` | `() → string` | Most recent engine error |
| `get_error_count` | `() → int` | Total error count |
| `get_subsystem_stats` | `(subsystem)` | Subsystem error/stat counters |
| `dump_report` | `()` | Dump a structured error/state report |
| `get_render_info` | `()` | Render backend diagnostics |
| `get_audio_info` | `()` | Audio backend diagnostics |
| `get_input_info` | `()` | Input backend diagnostics |
| `get_log_path` | `() → string` | Path of the engine log file |
| `get_stats` | `()` | Aggregate runtime stats |

---

## DevCore

```lua
-- Global: DevCore
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `set_input_focus` | `(bool)` | Route input to KAG vs. game focus |
| `get_input_focus` | `() → bool` | Whether input focus is on the game |
| `log` | `(message)` | Write a message to the engine log |
| `quit` | `()` | Request the engine to quit |
| `set_resolution` | `(w, h)` | Set the rendering resolution |
| `get_resolution` | `() → w, h` | Get the current resolution |
| `set_fullscreen` | `(bool)` | Toggle fullscreen |
| `get_window_size` | `() → w, h` | Get the OS window size |

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
| `play_se_3d` | `(file, x, y, z?)` | `bool` | Play a positional sound effect |
| `is_se_playing` | `()` | `bool` | Whether the SE bus is active |
| `audio_fade_volume` | `(bus, target, seconds)` | `bool` | Smooth volume change (bus: bgm/se/voice) |
| `audio_get_length` | `(bus)` | `number` | Current track length (seconds) |
| `audio_get_position` | `(bus)` | `number` | Current playback position (seconds) |
| `clear_screen` | `()` | `bool` | Clear the screen layers |
| `clear_text_layer` | `()` | — | Clear the text layer (delegates to clear_text) |
| `flush_wave_cache` | `()` | `bool` | Flush the decoded-wave cache |
| `quake` | `(duration_ms, amplitude?)` | `bool` | Screen shake (KAG3 classic) |
| `render_ruby` | `(text, ruby, x, y)` | `bool` | Furigana annotation for the current text line |
| `play_voice` | `(file, volume?)` | `bool` | Play voice line |
| `stop_voice` | `()` | `bool` | Stop current voice |
| `set_global_volume` | `(vol)` | — | Master volume 0.0–1.0 |
| `get_global_volume` | `() → number` | — | Current master volume |
| `set_bus_volume` | `(bus, vol)` | — | Set bus volume: "bgm"/"voice"/"se" |
| `get_bus_volume` | `(bus) → number` | — | Get bus volume |
| `render_text` | `(text, x, y, scale, r, g, b, a)` | — | Render text |
| `clear_text` | `()` | — | Clear text layer |
| `set_font` | `(face, size, color?)` | — | Set font face/size; `"default"` resets to the built-in bitmap font |
| `line_height` | `() → number` | — | Current line height |
| `is_bgm_playing` | `() → bool` | — | Is BGM playing |
| `is_voice_playing` | `() → bool` | — | Is voice playing |
| `get_active_voices` | `() → int` | — | Active voice count |
| `replay_voice` | `()` | `bool` | Replay the current voice line |
| `set_bgm_volume` | `(vol)` | — | Set BGM bus volume |
| `set_se_volume` | `(vol)` | — | Set SE bus volume |
| `set_voice_volume` | `(vol)` | — | Set Voice bus volume |
| `show_text` | `(text)` | — | Show text line |
| `show_image` | `(path, ...)` | — | Show an image onto a layer |
| `wait_click` | `()` | — | Wait for a click |
| `set_listener` | `(...)` | — | Set an audio listener |
| `log` | `(message)` | — | Write to engine log |

---

## Save (registered on the KAG module)

```lua
-- No separate `Save` global; the save/load bindings are registered onto
-- the KAG module (KAG.save_game, KAG.load_game, ...).
-- Backend: BackendRegistry::instance().getSaveManager()
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `save_game` | `(slot, data)` | Save game state to slot (int, Lua table) |
| `load_game` | `(slot) → table` | Load game state from slot |
| `list_saves` | `() → table` | List all save slots with metadata |
| `delete_save` | `(slot)` | Delete save slot |
| `save_exists` | `(slot) → bool` | Whether a slot has a save |
| `get_save_dir` | `() → string` | Directory where saves are written |
| `set_encryption_key` | `(key)` | Set/derive the save encryption key |
| `clear_encryption_key` | `()` | Clear the save encryption key |
| `capture_thumbnail` | `()` | Capture the current frame as the save thumbnail |
| `configure_cloud` | `(endpoint) → bool` | Configure HTTP cloud-save endpoint ("" = local only); offline-safe |
| `cloud_push` | `(slot) → bool` | Push slot file to the cloud |
| `cloud_pull` | `(slot) → bool` | Pull slot file from the cloud |

---

## Steam (unconditionally registered; safe Null defaults without the SDK)

```lua
-- Global: steam
-- Always present. Without the Steam SDK every call returns a safe
-- default (false / 0 / "" / {}) instead of a nil-function error.
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `unlock_achievement` | `(id)` | Unlock achievement by ID |
| `is_achievement_unlocked` | `(id) → bool` | Whether an achievement is unlocked |
| `reset_achievement` | `(id)` | Reset a single achievement |
| `reset_all_achievements` | `()` | Reset all achievements |
| `set_stat_int` | `(name, value)` | Set an integer statistic |
| `get_stat_int` | `(name) → int` | Get an integer statistic |
| `set_stat_float` | `(name, value)` | Set a float statistic |
| `get_stat_float` | `(name) → float` | Get a float statistic |
| `store_stats` | `()` | Commit statistic changes to Steam |
| `is_overlay_active` | `() → bool` | Whether the Steam overlay is active |
| `cloud_write` | `(name, data) → bool` | Write a cloud-save file (Remote Storage) |
| `cloud_read` | `(name) → string/nil` | Read a cloud file; nil when missing |
| `cloud_file_size` | `(name) → int` | Cloud file size in bytes |
| `cloud_file_exists` | `(name) → bool` | Cloud file existence |
| `cloud_delete` | `(name) → bool` | Delete a cloud file |
| `cloud_quota_total` | `() → int` | Total cloud quota bytes |
| `cloud_quota_used` | `() → int` | Used cloud bytes |
| `cloud_list` | `() → table` | Cloud file name list (up to 256) |
---

## i18n (Lua runtime localization module)

> Pure-Lua runtime module (`scripts/i18n.lua`), not a C++ binding. Exposed
> as the global `i18n` table like `scheduler`. Handles multi-language
> string tables (`assets/lang/<code>.lua`) and runtime template
> interpolation. Fallback chain: current language → default language → raw key.
> Plural support (round 80): a dictionary value may be a CLDR-style plural
> variant table (`{ one = "...", other = "..." }`); `translate(text, { n = … })`
> selects the variant for the count's `plural_category` and interpolates `{n}`
> with the literal number (see `plural_category` / `_plural_form` below).

| Function | Signature | Description |
|----------|-----------|-------------|
| `current_language` | `() → string` | Return the currently selected language code (`i18n.current`). Pairs with `set_language()`. |
| `set_language` | `(code, opts) → strings` | Select a per-language dictionary with fallback chain current → default → raw key. Loads `assets/lang/<code>.lua`, sets `i18n.current`, returns the active strings table. `opts.default` overrides the fallback default (updates `i18n.default_language`); `opts.reload=true` forces a re-read even when `code` equals the current language. |
| `translate` | `(text, params) → string` | Runtime template interpolation: resolve the template through the normal localization path, then fill `{name}` placeholders from `params`. Unknown placeholders and inline-markup tags are left intact. With no `params` behaves like `localize()`. Plural + numeric format (round 80): a string-table value may be a plural variant table (`items = { one = "{n} item", other = "{n} items" }`); with `params.n` the variant for that count's `plural_category` is picked and its `{n}` is interpolated to the literal number, without `params.n` the generic (`other`) form is resolved. Example: `i18n.translate("Hello, {name}!", { name = "Caesura" })` → `"Hello, Caesura!"`; `i18n.translate("items", { n = 1 })` → `"1 item"` (en one). |
| `plural_category` | `(count) → string` | Return the CLDR plural category for a count in the current language: `en` → `"one"` when count == 1 else `"other"`; `zh`/`ja` (and any unknown language) always → `"other"` (no singular/plural distinction). |
| `_plural_form` | `(entry, count?) → string` | Resolve a dictionary value that is a plain string (returned unchanged) or a plural variant table: picks the variant for `count`'s category, falling back to `other` then `one`; with `count` nil uses the generic `other` form (safe for `t()`/`expand()` with no `{n}`). |
| `reload` | `(langCode) → strings` | Hot-reload a language dictionary from disk (re-reads `assets/lang/<code>.lua` even if already current); preserves `i18n.current`, `i18n.default_language` and the cached fallback. |
| `default_language` | `(field) → string` | Fallback dictionary language (default `"en"`); configurable. |
