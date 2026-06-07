# KAG API Reference

> Canonical API reference for AI-assisted KAG scripting. 35 functions total.
> Convenience methods (show_text, show_image, clear_screen, wait_click) have canonical implementation in `_CAESURA_BACKEND`.

## Audio

| Function | Params | Returns | Notes |
|----------|--------|---------|-------|
| `play_bgm` | path:string, fadeTime:number(1.0) | handle:number | Cross-fade BGM |
| `stop_bgm` | fadeTime:number(1.0) | — | Fade-out and stop |
| `play_voice` | path:string | handle:number | Absolute interrupt |
| `stop_voice` | — | — | Stop immediately |
| `play_se` | path:string | handle:number | 2D sound effect |
| `play_se_3d` | path:string, x,y,z:number | handle:number | Spatial SE |
| `stop_se` | — | — | Stop all SE |
| `set_se_volume` | handle:number, volume:number(0-1) | — | Per-handle volume |
| `set_listener` | posX,Y,Z, atX,Y,Z:number | — | Update 3D listener |
| `set_global_volume` | volume:number(0-1) | volume | Master volume |
| `get_global_volume` | — | volume:number | Query master |
| `set_bgm_volume` | volume:number(0-1) | — | BGM bus |
| `set_se_volume` | volume:number(0-1) | — | SE bus |
| `set_voice_volume` | volume:number(0-1) | — | Voice bus |
| `set_bus_volume` | bus:string, volume:number | — | bus = "bgm"/"voice"/"se" |
| `get_bus_volume` | bus:string | volume:number | Query bus |
| `audio_get_position` | bus:string | seconds:number | Playback position |
| `audio_get_length` | bus:string | seconds:number | Track length |
| `audio_fade_volume` | bus:string, target:number, time:number | — | Smooth fade |
| `flush_wave_cache` | — | — | Free waveform memory |
| `replay_voice` | — | — | Restart last voice |
| `is_voice_playing` | — | bool | Voice active? |
| `is_bgm_playing` | — | bool | BGM active? |
| `get_active_voices` | — | count:number | Active handles |

## Visual

| Function | Params | Returns | Notes |
|----------|--------|---------|-------|
| `show_text` | text:string | — | Display in message box. Canonical: `_CAESURA_BACKEND.show_text()` |
| `show_image` | layer:string, path:string, x,y:number, sx,sy:number(1), opacity:number(1), blend:number(0) | — | layer = "bg"/"fg"/"msg". Canonical: `_CAESURA_BACKEND.show_image()` |
| `clear_screen` | layer:string? | — | nil = all layers. Canonical: `_CAESURA_BACKEND.clear_screen()` |
| `wait_click` | — | — | Pause until click. Canonical: `_CAESURA_BACKEND.wait_click()` |
| `clear_text_layer` | — | — | Clear message layer |
| `clear_text` | — | — | Clear text from message |
| `render_text` | text:string, x,y:number, r,g,b,a:number(0-255) | — | Direct text render |
| `render_ruby` | text:string, ruby:string, x,y,r,g,b,a:number | — | Ruby annotation |
| `set_font` | fontId:number | — | 0=Small, 1=Large, 2=TTF |
| `line_height` | — | pixels:number | Current font line height |
| `quake` | intensity:number, duration:number | — | Screen shake |

## Utility

| Function | Params | Notes |
|----------|--------|-------|
| `log` | level:string, message:string | level = "trace"/"debug"/"info"/"warn"/"error"/"fatal" |

## Sandbox constraints

Disabled globals: `loadfile`, `dofile`, `os.execute`, `os.remove`, `os.rename`, `os.exit`, `io.open`, `io.popen`.
Debug library is read-only subset.
`require()` only returns preloaded modules.
Full rules in `scripts/sandbox.lua`.