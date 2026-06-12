# Lua Module API

Caesura (AmeKAG) exposes engine functionality through Lua binding modules. All modules are accessible via `require()` in game scripts.

## Render Module

```lua
local Render = require("Render")  -- or use global `Render`
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `render_text` | `(text, x, y, scale, r, g, b, a)` | Render text at position |
| `render_ruby` | `(text, ruby, x, y)` | Render text with furigana |
| `clear_text` | `()` | Clear text from message layer |
| `set_font` | `(face, size, color)` | Set font properties |
| `line_height` | `()` → `number` | Get current line height |
| `create_solid_texture` | `(r, g, b, a)` → `handle` | Create solid color texture |
| `load_texture` | `(path)` → `handle` | Load texture from file |
| `destroy_texture` | `(handle)` | Release texture resource |

## VFX Module

```lua
local VFX = require("VFX")
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `create_emitter` | `(cfg)` → `id` | Create particle emitter |
| `destroy_emitter` | `(id)` | Destroy particle emitter |
| `emit` | `(id, count)` | Emit N particles |
| `set_emitter_pos` | `(id, x, y)` | Set emitter position |
| `set_emitter_color` | `(id, r, g, b, a)` | Set emitter color |
| `set_emitter_rate` | `(id, rate)` | Set emission rate |
| `alive_count` | `()` → `number` | Get active particle count |
| `clear_all` | `()` | Clear all emitters and particles |

## Debug Module

```lua
local Debug = require("Debug")
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `log` | `(level, message)` | Write to engine log |
| `assert` | `(condition, message)` | Debug assertion check |
| `traceback` | `()` → `string` | Get Lua stack traceback |
| `get_fps` | `()` → `number` | Get current frame rate |
| `get_memory` | `()` → `number` | Get Lua memory usage (KB) |
| `profile_start` | `(name)` | Start profiling section |
| `profile_end` | `(name)` | End profiling section |

## DevCore Module

```lua
local DevCore = require("DevCore")
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_version` | `()` → `string` | Get engine version |
| `get_backend_name` | `(subsystem)` → `string` | Get active backend name |
| `set_dev_mode` | `(bool)` | Toggle development mode |
| `reload_scripts` | `()` | Trigger hot reload |

## KAG Module (C++ Bindings)

```lua
local KAG = require("KAG")  -- or use global `KAG`
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `play_bgm` | `(file, volume?, loop?)` | Play background music |
| `play_se` | `(file, volume?)` | Play sound effect |
| `play_voice` | `(file, volume?)` | Play voice line |
| `stop_bgm` | `(fadeTime?)` | Stop BGM |
| `stop_se` | `()` | Stop all SE |
| `stop_voice` | `()` | Stop voice |
| `set_global_volume` | `(vol)` | Set master volume [0-1] |
| `get_global_volume` | `()` → `number` | Get master volume |
| `set_bus_volume` | `(bus, vol)` | Set bus volume ("bgm"/"voice"/"se") |
| `get_bus_volume` | `(bus)` → `number` | Get bus volume |
| `render_text` | `(text, x, y, scale, r, g, b, a)` | Render text |
| `clear_text` | `()` | Clear text layer |
| `line_height` | `()` → `number` | Get line height |
| `is_bgm_playing` | `()` → `bool` | Check BGM playback |
| `is_voice_playing` | `()` → `bool` | Check voice playback |
| `get_active_voices` | `()` → `number` | Get active voice count |
| `log` | `(message)` | Log message |

## SaveBinding Module

```lua
-- Registered automatically, accessible via Lua global `Save`
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `save` | `(slot, data)` | Save game to slot |
| `load` | `(slot)` → `table` | Load game from slot |
| `list_saves` | `()` → `table` | List all saves |
| `delete_save` | `(slot)` | Delete save slot |

## Steam Module (Conditional)

```lua
-- Registered via SteamBinding when CAESURA_HAS_STEAM is defined
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `is_available` | `()` → `bool` | Check Steam availability |
| `get_user_name` | `()` → `string` | Get Steam display name |
| `unlock_achievement` | `(id)` | Unlock Steam achievement |
| `set_rich_presence` | `(key, value)` | Set rich presence data |
