# KAG Command Reference — Complete

> Caesura (AmeKAG) KAG 3.0 script syntax. Commands use `[command param="value"]` format in `.ks` files.
> All commands are parsed by `scripts/tokenizer.lua` and dispatched by `scripts/scheduler.lua`.

---

## Audio Commands (13)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `playbgm` | `storage` (path), `volume` (0–1), `loop` (bool) | string, float, bool | Play background music. Default volume=1.0, loop=true. |
| `stopbgm` | `time` (fade ms) | int | Stop BGM with optional fade-out. |
| `fadebgm` | `volume` (target), `time` (ms) | float, int | Fade BGM to target volume over N milliseconds. |
| `xfadebgm` | `storage` (path), `time` (ms) | string, int | Cross-fade to new BGM. |
| `playse` | `storage` (path), `volume` (0–1) | string, float | Play sound effect. |
| `stopse` | — | — | Stop all sound effects. |
| `playvoice` | `storage` (path), `volume` (0–1) | string, float | Play voice line (interrupts previous voice). |
| `stopvoice` | — | — | Stop current voice. |
| `waitsound` | — | — | Block until current SE finishes playing. |
| `waitbgm` | — | — | Block until BGM fade completes. |
| `setbgmvolume` | `volume` (0–1) | float | Set BGM bus volume. |
| `setsevolume` | `volume` (0–1) | float | Set SE bus volume. |
| `setvoicevolume` | `volume` (0–1) | float | Set Voice bus volume. |

---

## Layer Commands (6)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `bg` | `storage` (path), `time` (ms) | string, int | Set background image. |
| `fg` | `storage` (path), `layer` (int), `clear` (bool) | string, int, bool | Set foreground character sprite. |
| `cl` | `layer` ("bg"/"fg"/"msg"/"all") | string | Clear specified layer(s). |
| `image` | `storage` (path), `layer`, `x`, `y`, `w`, `h` | string, string, int×4 | Place image at pixel position. |
| `position` | `layer`, `x`, `y`, `scale` | string, int×2, float | Position and scale a layer. |
| `layopt` | `layer`, `opacity` (0–1), `blend` (0=alpha) | string, float, int | Set layer render options. |

---

## Text Commands (13)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `ch` | `name` (speaker), `text` | string×2 | Character dialogue with speaker name. |
| `text` | `text` | string | Plain narration text. |
| `l` | — | — | Line break (newline within a text block). |
| `r` | — | — | Carriage return. |
| `er` | — | — | Erase all text from message layer. |
| `p` | — | — | Page break / click-to-advance. Blocks until user input. |
| `ruby` | `text`, `ruby` | string×2 | Render text with furigana annotation above. |
| `font` | `face`, `size`, `color` ("#RRGGBB") | string, int, string | Set font face, point size, and colour. |
| `skip` | — | — | Toggle skip mode (fast-forward through text). |
| `reset` | — | — | Reset text rendering state to defaults. |
| `pt` | `speed` (ms/char) | int | Typewriter effect speed in milliseconds per character. |
| `button` | `text`, `target` (`*label`) | string×2 | Choice button. Target is a label to jump to on selection. |
| `endbutton` | — | — | Finalize choice set. Blocks execution until a button is clicked. |

---

## System Commands (4)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `wait` | `time` (ms) | int | Block execution for N milliseconds. Supports cancellation. |
| `eval` | `exp` (Lua expr) | string | Evaluate Lua expression in `ctx.f` environment. Result stored in `ctx`. |
| `emb` | `exp` (Lua code) | string | Execute embedded Lua in sandboxed environment. |
| `history` | — | — | Open scrollable backlog overlay UI. Supports ↑↓ navigation, V=voice replay, Enter=jump. |

---

## Flow Control (11)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `if` | `exp` (Lua expr) | string | Conditional branch. Skip to `else`/`endif` if false. Supports nested depth. |
| `else` | — | — | Alternative branch. |
| `endif` | — | — | End conditional block. |
| `switch` | `expr` (Lua expr) | string | Multi-way branch. Matches `case` values within block. (Alpha: flat only.) |
| `case` | `value` | string | Case branch within switch. |
| `default` | — | — | Default case branch. |
| `endswitch` | — | — | End switch block. |
| `jump` | `target` (`*label`) | string | **Intra-scene** jump to label. Logs warning if label not found. |
| `call` | `target` (scene path) | string | **Inter-scene** subroutine call. Pushes return address to call stack. |
| `return` | — | — | Pop call stack and resume at the next command after the `call`. |
| `link` | `target` (scene path) | string | **Inter-scene** hard jump. Clears layers, backlog, and call stack. |
| `label` | (inline `*name`) | — | Define a jump target. Written as `*label_name` on its own line. |
| `macro` | `name` | string | Define a named, parameterised macro. Body is collected until `endmacro`. |
| `endmacro` | — | — | End macro definition. |
| `erasemacro` | `name` | string | Erase a previously defined macro. |
| `end` | — | — | End script execution. Returns from scheduler coroutine. |
| `stop` | — | — | Immediately stop execution. |

---

## Transition Commands (4)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `trans` | `type` (rule name), `time` (ms) | string, int | Apply named transition effect (fade, dissolve, wipe, etc). |
| `move` | `layer`, `x`, `y`, `time` (ms) | string, int×3 | Animate layer to target position over time. |
| `quake` | `time` (ms), `amplitude` (px) | int×2 | Screen shake effect. |
| `fade` | `type` ("in"/"out"), `time` (ms) | string, int | Fade entire screen in or out. |

---

## VFX Commands (1)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `vfx` | `type` (effect), `time` (ms), params... | string, int, ... | Apply visual effect. Types: shake, flash, blur, sepia, grayscale, negative. |

---

## Video Commands (2)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `video` | `storage` (path), `loop` (bool), `volume` (0–1) | string, bool, float | Play MPEG-1 video file. |
| `stopvideo` | — | — | Stop video playback. |

---

## Resource Commands (6)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `preload` | `storage` (path), `type` ("image"/"audio"/"script") | string×2 | Preload asset into cache. |
| `get_texture` | `storage` (path) | string | Get texture handle by file path. |
| `is_loaded` | `storage` (path) | string | Check if asset is fully loaded and ready. |
| `is_pending` | `storage` (path) | string | Check if asset is currently loading. |
| `flush_cache` | — | — | Clear all cached assets. |

---

## Save/Load Commands (3)

| Command | Parameters | Types | Description |
|---------|-----------|-------|-------------|
| `save` | `slot` (int) | int | Save game state (scene, token_index, variables, flags) to slot. |
| `load` | `slot` (int) | int | Load game state from slot. Resumes from saved token index. |
| `listsaves` | — | — | List all save slots. Outputs slot number and timestamp. |

---

## Total: 68 commands across 8 categories

| Category | Count | Commands |
|----------|-------|----------|
| Audio | 13 | playbgm, stopbgm, fadebgm, xfadebgm, playse, stopse, playvoice, stopvoice, waitsound, waitbgm, setbgmvolume, setsevolume, setvoicevolume |
| Layer | 6 | bg, fg, cl, image, position, layopt |
| Text | 13 | ch, text, l, r, er, p, ruby, font, skip, reset, pt, button, endbutton |
| System | 4 | wait, eval, emb, history |
| Flow | 16 | if, else, endif, switch, case, default, endswitch, jump, call, return, link, label, macro, endmacro, erasemacro, end, stop |
| Transition | 4 | trans, move, quake, fade |
| VFX | 1 | vfx |
| Video | 2 | video, stopvideo |
| Resource | 6 | preload, get_texture, is_loaded, is_pending, flush_cache |
| Save/Load | 3 | save, load, listsaves |