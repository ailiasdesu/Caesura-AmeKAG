# KAG API Reference

> Auto-generated from C++ bindings. Use this as the canonical API reference for AI-assisted KAG scripting.

## Overview

All KAG APIs live under the global KAG table. Convenience methods (show_text, show_image, clear_screen, wait_click) are also accessible via _CAESURA_BACKEND table (canonical path).

## Audio APIs

### play_bgm(path, fadeTime)
Play background music with optional cross-fade.
- path: string — audio file path (WAV/MP3/OGG)
- fadeTime: number (default 1.0) — cross-fade duration in seconds
- Returns: handle (number) — audio handle for later control

### stop_bgm(path, fadeTime)
Stop background music with fade-out.

### play_voice(path)
Play a voice line (absolute interrupt — stops any currently playing voice).
- Returns: handle (number)

### stop_voice()
Stop current voice playback immediately.

### play_se(path)
Play a sound effect (2D).
- Returns: handle (number)

### play_se_3d(path, x, y, z)
Play a spatial sound effect at 3D position.
- Returns: handle (number)

### stop_se()
Stop all active sound effects.

### set_se_volume(handle, volume)
Set volume for a specific SE handle (0.0 to 1.0).

### set_listener(posX, posY, posZ, atX, atY, atZ)
Update the 3D audio listener position and orientation for spatial audio.

### set_global_volume(volume)
Set master volume (0.0 to 1.0). Returns the new volume.

### get_global_volume()
Get current master volume.

### set_bgm_volume(volume)
### set_se_volume(volume)
### set_voice_volume(volume)
Per-bus volume control (0.0 to 1.0).

### set_bus_volume(bus, volume)
Set volume for a named bus. bus: "bgm" | "voice" | "se".

### get_bus_volume(bus)
Get current volume for a named bus.

### audio_get_position(bus)
Get playback position in seconds for bus "bgm" | "voice".

### audio_get_length(bus)
Get total length in seconds for the current track on bus "bgm" | "voice".

### audio_fade_volume(bus, targetVolume, fadeTime)
Smoothly fade bus volume to target over fadeTime seconds.

### flush_wave_cache()
Clear the audio waveform cache (frees memory).

### replay_voice()
Replay the last voice line from beginning.

### is_voice_playing()
Returns true if a voice line is currently playing.

### is_bgm_playing()
Returns true if BGM is currently playing.

### get_active_voices()
Returns count of currently active voice handles.

## Visual APIs

### show_text(text)
Display text in the message box. Canonical path: _CAESURA_BACKEND.show_text(text).

### show_image(layer, path, x, y, sx, sy, opacity, blend)
Display an image on a layer. Canonical path: _CAESURA_BACKEND.show_image(...).
- layer: "bg" | "fg" | "msg"
- path: string — image file path
- x, y: number — position in pixels
- sx, sy: number (default 1.0) — scale
- opacity: number (0.0 to 1.0)
- blend: number (default 0) — blend mode index (0=Normal, 1=Multiply, etc.)

### clear_screen(layer)
Clear a layer. Canonical path: _CAESURA_BACKEND.clear_screen(layer).
- layer: "bg" | "fg" | "msg" | nil (clear all)

### wait_click()
Pause script execution until user clicks. Canonical path: _CAESURA_BACKEND.wait_click().

### clear_text_layer()
Clear the text/message layer specifically.

### clear_text()
Clear text from the message layer.

### render_text(text, x, y, r, g, b, a)
Render text at a specific position with color.

### render_ruby(text, ruby, x, y, r, g, b, a)
Render text with ruby/furigana annotation.

### set_font(fontId)
Set active font. fontId: 0=Small, 1=Large, 2=TTF.

### line_height()
Returns current font line height in pixels.

### quake(intensity, duration)
Screen shake effect. intensity: pixel offset, duration: seconds.

## Utility APIs

### log(level, message)
Log a message through the engine debug system.
- level: "trace" | "debug" | "info" | "warn" | "error" | "fatal"

## Sandbox Constraints

The following Lua globals are disabled in KAG scripts:
- loadfile, dofile (arbitrary file loading)
- os.execute, os.remove, os.rename, os.exit (filesystem mutation)
- io.open, io.popen (direct file I/O)
- debug.setupvalue, debug.setlocal, debug.setmetatable, debug.sethook (debug mutation)

All file access must go through KAG API functions (play_bgm, show_image, etc.) which route through the C++ IAssetProvider chain.
