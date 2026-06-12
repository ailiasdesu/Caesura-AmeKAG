# KAG Command Reference

Caesura (AmeKAG) supports KAG 3.0 script syntax. Commands are written as `[command param="value"]` in `.ks` files.

## Audio Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `playbgm` | `storage` (string), `volume` (number, default 1.0), `loop` (bool) | Play background music |
| `stopbgm` | `time` (number, ms fade) | Stop BGM with optional fade |
| `fadebgm` | `volume` (number), `time` (number, ms) | Fade BGM to target volume |
| `xfadebgm` | `storage` (string), `time` (number) | Cross-fade to new BGM |
| `playse` | `storage` (string), `volume` (number) | Play sound effect |
| `stopse` | — | Stop all sound effects |
| `playvoice` | `storage` (string), `volume` (number) | Play voice line |
| `stopvoice` | — | Stop current voice |
| `waitsound` | — | Block until current SE finishes |
| `waitbgm` | — | Block until BGM fade completes |
| `setbgmvolume` | `volume` (number) | Set BGM bus volume [0-1] |
| `setsevolume` | `volume` (number) | Set SE bus volume [0-1] |
| `setvoicevolume` | `volume` (number) | Set voice bus volume [0-1] |

## Layer Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `bg` | `storage` (string), `time` (number) | Set background image |
| `fg` | `storage` (string), `layer` (number), `clear` (bool) | Set foreground character image |
| `cl` | `layer` (string, "bg"/"fg"/"msg") | Clear specified layer |
| `image` | `storage` (string), `layer` (string), `x`, `y`, `w`, `h` | Place image at position |
| `position` | `layer` (string), `x`, `y`, `scale` | Position a layer |
| `layopt` | `layer` (string), `opacity` (number) | Set layer render options |

## Text Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `ch` | `name` (string), `text` (string) | Character dialog with speaker name |
| `text` | `text` (string) | Plain narration text |
| `l` | — | Line break |
| `r` | — | Carriage return |
| `er` | — | Erase all text from message layer |
| `p` | — | Page break / click-to-advance |
| `ruby` | `text` (string), `ruby` (string) | Render text with furigana annotation |
| `font` | `face` (string), `size` (number), `color` (string) | Set font face/size/color |
| `skip` | — | Toggle skip mode |
| `reset` | — | Reset text rendering state |
| `pt` | `speed` (number) | Typewriter speed (ms per char) |
| `button` | `text` (string), `target` (string) | Choice button for selection menu |
| `endbutton` | — | Finalize choice button set, block until selection |

## System Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `wait` | `time` (number, ms) | Block execution for N milliseconds |
| `eval` | `exp` (string) | Evaluate Lua expression, store result in ctx |
| `emb` | `exp` (string) | Execute embedded Lua in sandboxed environment |
| `history` | — | Open backlog overlay UI |

## Flow Control (handled by scheduler)

| Command | Parameters | Description |
|---------|-----------|-------------|
| `if` | `exp` (string) | Conditional branch |
| `else` | — | Alternative branch |
| `endif` | — | End conditional block |
| `jump` | `target` (string) | Jump to label in current scene |
| `call` | `target` (string) | Call subroutine (with return) |
| `return` | — | Return from subroutine |
| `link` | `target` (string) | Cross-scene jump |
| `label` | (inline `*name`) | Define a jump target label |
| `macro` | `name` (string) | Define a parameterized macro |
| `endmacro` | — | End macro definition |
| `end` | — | End script execution |

## Transition Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `trans` | `type` (string), `time` (number) | Apply named transition effect |
| `move` | `layer` (string), `x`, `y`, `time` | Animate layer position |
| `quake` | `time` (number), `amplitude` (number) | Screen shake effect |
| `fade` | `type` ("in"/"out"), `time` (number) | Fade screen in/out |

## VFX Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `vfx` | `type` (string), `time` (number), params... | Apply visual effect (shake, flash, blur, sepia, etc.) |

## Video Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `video` | `storage` (string), `loop` (bool), `volume` (number) | Play video file |
| `stopvideo` | — | Stop video playback |

## Resource Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `preload` | `storage` (string), `type` (string) | Preload asset into cache |
| `get_texture` | `storage` (string) | Get texture handle by path |
| `is_loaded` | `storage` (string) | Check if asset is loaded |
| `is_pending` | `storage` (string) | Check if asset is pending load |
| `flush_cache` | — | Clear asset cache |

## Save/Load Commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `save` | `slot` (number) | Save game to slot |
| `load` | `slot` (number) | Load game from slot |
| `listsaves` | — | List all save slots |
