# KAG Neo-Genesis Command Contracts (auto-generated)

> Generated from the declarative schema registry (`kag/schema.lua`) — do not edit.
> Regenerate: `lua scripts/schema_doc.lua > docs/api/command-contracts.md`

## Commands (78)

### `[ai_dialog]`

_Category: system · Blocking: yes (waits for completion) · AI-driven dialogue line (LLM, async)_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fallback` | string |  | - | - |
| `max_wait_ms` | number | 15000 | 100..120000 | - |
| `model` | string |  | - | - |
| `name` | string |  | - | - |
| `prompt` | string | - | - | yes |
| `system` | string |  | - | - |

### `[assert]`

_Category: system · Blocking: no (fire-and-forget) · development-time assertion on an expression_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `exp` | string | - | - | yes |
| `msg` | string | - | - | - |

### `[auto]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible auto command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `mode` | string | - | off,on,toggle | - |

### `[bg]`

_Category: layer · Blocking: no (fire-and-forget) · KAG3-compatible bg command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `layer` | string | bg | - | - |
| `path` | string | - | - | - |
| `storage` | string | - | - | - |

### `[bgm]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `volume` | number | - | 0..1.5 | - |

### `[br]`

_Category: text · Blocking: no (fire-and-forget) · KAG3 line-break alias_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[camera]`

_Category: transition · Blocking: no (fire-and-forget) · KAG3-compatible camera command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `restore` | boolean | true | - | - |
| `time` | number | 500 | 0..30000 | - |
| `x` | number | 0 | 0..2000 | - |
| `y` | number | 0 | 0..2000 | - |

### `[cancel]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `all` | boolean | false | - | - |
| `layer` | string |  | - | - |

### `[ch]`

_Category: text · Blocking: yes (waits for completion) · KAG3-compatible ch command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `chars_per_line` | number | 0 | 0..512 | - |
| `max_width` | number | 0 | 0..4096 | - |
| `name` | string |  | - | - |
| `sprite` | string | - | - | - |
| `text` | string |  | - | - |
| `voice` | string |  | - | - |

### `[chapter]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible chapter command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `id` | string | - | - | - |
| `label` | string | - | - | - |

### `[cl]`

_Category: layer · Blocking: no (fire-and-forget) · KAG3-compatible cl command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string | all | - | - |

### `[close]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[delay]`

_Category: system · Blocking: yes (waits for completion) · KAG3-compatible delay command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | - | 0..60000 | - |
| `ms` | number | - | 0..60000 | - |
| `time` | number | 1000 | 0..60000 | - |

### `[emb]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible emb command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `code` | string | - | - | - |
| `exp` | string | - | - | - |

### `[ending]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible ending command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `id` | string | - | - | - |
| `name` | string | - | - | - |

### `[er]`

_Category: text · Blocking: no (fire-and-forget) · erase line_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[eval]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible eval command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `code` | string | - | - | - |
| `exp` | string | - | - | - |

### `[fadebgm]`

_Category: audio · Blocking: yes (waits for completion) · KAG3-compatible fadebgm command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadein` | number | 0 | 0..30000 | - |
| `time` | number | 1000 | 0..30000 | - |
| `volume` | number | 0 | 0..1.5 | - |

### `[fadeout]`

_Category: layer · Blocking: yes (waits for completion) · KAG3-compatible fadeout command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `alpha` | number | 0 | 0..1.0 | - |
| `duration` | number | 500 | 0..30000 | - |
| `layer` | string | bg | - | - |
| `opacity` | number | 0 | 0..1.0 | - |
| `time` | number | 500 | 0..30000 | - |

### `[fadevol]`

_Category: audio · Blocking: yes (waits for completion) · KAG3-compatible fadevol command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `time` | number | 1000 | 0..30000 | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[fg]`

_Category: layer · Blocking: no (fire-and-forget) · KAG3-compatible fg command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `clear` | boolean | false | - | - |
| `file` | string | - | - | - |
| `layer` | string | fg | - | - |
| `path` | string | - | - | - |
| `storage` | string | - | - | - |

### `[flash]`

_Category: vfx · Blocking: yes (waits for completion) · KAG3-compatible flash command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `b` | number | 255 | 0..255 | - |
| `g` | number | 255 | 0..255 | - |
| `r` | number | 255 | 0..255 | - |
| `time` | number | 200 | 0..10000 | - |

### `[font]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible font command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | white | - | - |
| `face` | string | default | - | - |
| `size` | number | 22 | 4..256 | - |

### `[gallery]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible gallery command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `id` | string | - | - | - |

### `[history]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible history command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[hr]`

_Category: text · Blocking: no (fire-and-forget) · horizontal rule_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[image]`

_Category: layer · Blocking: no (fire-and-forget) · KAG3-compatible image command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `h` | number | - | 0..8192 | - |
| `layer` | string | fg | - | - |
| `storage` | string | - | - | - |
| `w` | number | - | 0..8192 | - |
| `x` | number | - | - | - |
| `y` | number | - | - | - |

### `[inc]`

_Category: system · Blocking: no (fire-and-forget) · increment a numeric variable (by default 1)_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `by` | number | 1 | - | - |
| `var` | string | - | - | yes |

### `[l]`

_Category: text · Blocking: no (fire-and-forget) · line break_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[layopt]`

_Category: layer · Blocking: no (fire-and-forget) · KAG3-compatible layopt command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string |  | - | - |
| `opacity` | number | 1.0 | 0..1.0 | - |
| `visible` | boolean | true | - | - |

### `[ld]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string | - | - | - |
| `name` | string | - | - | - |

### `[listsaves]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[load]`

_Category: save · Blocking: yes (waits for completion) · KAG3-compatible load command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `slot` | number | - | -2..99 | - |

### `[loadplace]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[move]`

_Category: transition · Blocking: yes (waits for completion) · KAG3-compatible move command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 300 | 0..30000 | - |
| `layer` | string |  | - | - |
| `name` | string |  | - | - |
| `time` | number | 300 | 0..30000 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[moveto]`

_Category: layer · Blocking: yes (waits for completion) · KAG3-compatible moveto command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string | - | - | - |
| `left` | number | - | - | - |
| `scale` | number | 1.0 | 0.01..16 | - |
| `top` | number | - | - | - |
| `unit` | string | ndc | - | - |
| `x` | number | - | - | - |
| `y` | number | - | - | - |

### `[music]`

_Category: system · Blocking: no (fire-and-forget) · KAG3-compatible music command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[nameplate]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible nameplate command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | 0,0,0 | - | - |
| `h` | number | 36 | 16..256 | - |
| `opacity` | number | 220 | 0..255 | - |
| `text_color` | string | 255,255,255 | - | - |
| `w` | number | 220 | 32..1024 | - |
| `x` | number | 32 | - | - |
| `y` | number | 480 | - | - |

### `[p]`

_Category: text · Blocking: yes (waits for completion) · click-to-advance_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[particles]`

_Category: vfx · Blocking: no (fire-and-forget) · KAG3-compatible particles command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `a` | number | 1 | 0..255 | - |
| `alpha` | number | 1 | 0..255 | - |
| `angleMax` | number | 6.283 | - | - |
| `angleMin` | number | 0 | - | - |
| `angle_max` | number | 0 | - | - |
| `b` | number | 1 | 0..255 | - |
| `blue` | number | 1 | 0..255 | - |
| `g` | number | 1 | 0..255 | - |
| `green` | number | 1 | 0..255 | - |
| `lifeMax` | number | 2.0 | 0..60 | - |
| `lifeMin` | number | 0.5 | 0..60 | - |
| `life_max` | number | 0.5 | 0..60 | - |
| `r` | number | 1 | 0..255 | - |
| `rate` | number | 10 | 0..1000 | - |
| `red` | number | 1 | 0..255 | - |
| `sizeMax` | number | 8 | 0..512 | - |
| `sizeMin` | number | 2 | 0..512 | - |
| `size_max` | number | 2 | 0..512 | - |
| `speedMax` | number | 50 | 0..10000 | - |
| `speedMin` | number | 10 | 0..10000 | - |
| `speed_max` | number | 10 | 0..10000 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[play]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `bus` | string | - | bgm,se,voice | - |
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `volume` | number | - | 0..1.5 | - |

### `[playbgm]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible playbgm command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| **requires one of** | — | — | file, storage | yes |
| `fadein` | number | 0 | 0..30000 | - |
| `file` | string | - | - | - |
| `loop` | boolean | true | - | - |
| `storage` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[playbgmstop]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible playbgmstop command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadein` | number | 0 | 0..30000 | - |
| `fadeout` | number | 0 | 0..30000 | - |
| `file` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[playse]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible playse command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| **requires one of** | — | — | file, storage | yes |
| `fadein` | number | 0 | 0..30000 | - |
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[playstop]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadeout` | number | 0 | 0..30000 | - |

### `[position]`

_Category: layer · Blocking: no (fire-and-forget) · KAG3-compatible position command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string |  | - | - |
| `name` | string |  | - | - |
| `pos` | string |  | - | - |
| `scale` | number | 1.0 | 0.01..16 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[pt]`

_Category: text · Blocking: no (fire-and-forget) · point text at position_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speed` | number | 50 | 8..5000 | - |

### `[quake]`

_Category: transition · Blocking: yes (waits for completion) · KAG3-compatible quake command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `amplitude` | number | 5 | 0..100 | - |
| `duration` | number | 300 | 0..30000 | - |
| `intensity` | number | 5 | 0..100 | - |
| `time` | number | 300 | 0..30000 | - |

### `[r]`

_Category: text · Blocking: no (fire-and-forget) · carriage return_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[random]`

_Category: system · Blocking: no (fire-and-forget) · write a random integer into a variable_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `max` | number | 100 | - | - |
| `min` | number | 0 | - | - |
| `var` | string | - | - | yes |

### `[replay]`

_Category: system · Blocking: no (fire-and-forget) · input recording/playback control_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `mode` | string | - | - | yes |

### `[reset]`

_Category: text · Blocking: no (fire-and-forget) · reset text state_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[ruby]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible ruby command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `ruby` | string |  | - | - |
| `ruby_scale` | number | 0.5 | 0.1..2.0 | - |
| `text` | string |  | - | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[s]`

_Category: text · Blocking: yes (waits for completion) · KAG3 short-wait_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `ms` | number | 250 | 0..60000 | - |

### `[save]`

_Category: save · Blocking: yes (waits for completion) · KAG3-compatible save command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `slot` | number | - | -2..99 | - |

### `[saveplace]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[scroll]`

_Category: transition · Blocking: no (fire-and-forget) · KAG3-compatible scroll command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | white | - | - |
| `size` | number | 28 | 8..128 | - |
| `speed` | number | 60 | 1..1000 | - |
| `text` | string |  | - | - |

### `[set]`

_Category: system · Blocking: no (fire-and-forget) · typed variable assignment (f.x/sf.x/tf.x/mp.x/lf.x)_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `value` | string | - | - | yes |
| `var` | string | - | - | yes |

### `[setbgmvolume]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible setbgmvolume command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | - | 0..1.5 | - |

### `[setsevolume]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible setsevolume command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | - | 0..1.5 | - |

### `[setvoicevolume]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible setvoicevolume command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | - | 0..1.5 | - |

### `[shake]`

_Category: vfx · Blocking: yes (waits for completion) · KAG3-compatible shake command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `amplitude` | number | 6 | 0..100 | - |
| `frequency` | number | 20 | 1..120 | - |
| `time` | number | 500 | 0..10000 | - |

### `[sprite_fade]`

_Category: text · Blocking: yes (waits for completion) · KAG3-compatible sprite_fade command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speaker` | string | - | - | yes |
| `time` | number | 300 | 0..30000 | - |
| `to` | number | 255 | 0..255 | - |

### `[sprite_move]`

_Category: text · Blocking: yes (waits for completion) · KAG3-compatible sprite_move command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speaker` | string | - | - | yes |
| `time` | number | 400 | 0..30000 | - |
| `x` | number | 440 | - | - |
| `y` | number | 200 | - | - |

### `[sprite_scale]`

_Category: text · Blocking: yes (waits for completion) · KAG3-compatible sprite_scale command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `scale` | number | 1.0 | 0.1..4.0 | - |
| `speaker` | string | - | - | yes |
| `time` | number | 300 | 0..30000 | - |

### `[sprite_swap]`

_Category: text · Blocking: yes (waits for completion) · KAG3-compatible sprite_swap command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speaker` | string | - | - | yes |
| `sprite` | string | - | - | yes |

### `[stopbgm]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible stopbgm command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadeout` | number | 0 | 0..30000 | - |
| `time` | number | 0 | 0..30000 | - |

### `[stopse]`

_Category: audio · Blocking: no (fire-and-forget) · KAG3-compatible stopse command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadeout` | number | 0 | 0..30000 | - |
| `time` | number | 0 | 0..30000 | - |

### `[text]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible text command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fade` | number | 0 | 0..30000 | - |
| `fade_time` | number | 0 | 0..30000 | - |
| `text` | string |  | - | - |

### `[textbox]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible textbox command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | 0,0,0 | - | - |
| `h` | number | 200 | 32..1024 | - |
| `opacity` | number | 200 | 0..255 | - |
| `visible` | boolean | true | - | - |
| `w` | number | 1280 | 64..4096 | - |
| `x` | number | 0 | - | - |
| `y` | number | 520 | - | - |

### `[trans]`

_Category: transition · Blocking: yes (waits for completion) · KAG3-compatible trans command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 500 | 0..30000 | - |
| `method` | string | crossfade | - | - |
| `time` | number | 500 | 0..30000 | - |
| `type` | string | crossfade | - | - |

### `[vib]`

_Category: transition · Blocking: yes (waits for completion) · KAG3-compatible vib command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `amplitude` | number | - | 0..50 | - |
| `intensity` | number | 3 | 0..50 | - |
| `time` | number | 300 | 0..30000 | - |

### `[video]`

_Category: video · Blocking: yes (waits for completion) · KAG3-compatible video command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| **requires one of** | — | — | file, storage | yes |
| `file` | string | - | - | - |
| `h` | number | 0 | 0..8192 | - |
| `loop` | boolean | false | - | - |
| `storage` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |
| `w` | number | 0 | 0..8192 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[voice_off]`

_Category: text · Blocking: no (fire-and-forget) · KAG3-compatible voice_off command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `on` | boolean | true | - | - |

### `[voice_wait]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[wait]`

_Category: system · Blocking: yes (waits for completion) · KAG3-compatible wait command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | - | 0..60000 | - |
| `ms` | number | - | 0..60000 | - |
| `time` | number | 1000 | 0..60000 | - |

### `[waitforclick]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[xfadebgm]`

_Category: audio · Blocking: yes (waits for completion) · KAG3-compatible xfadebgm command_

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `time` | number | 2000 | 0..30000 | - |

