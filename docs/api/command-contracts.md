# KAG Command Contracts (auto-generated)

> Generated from the declarative schema registry (`kag/schema.lua`) — do not edit.
> Regenerate: `lua scripts/schema_doc.lua > docs/api/command-contracts.md`

## Commands (66)

### `[auto]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `mode` | string | - | off,on,toggle | - |

### `[bg]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |

### `[bgm]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `volume` | number | - | 0..1.5 | - |

### `[br]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[camera]`

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

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `chars_per_line` | number | 0 | 0..512 | - |
| `max_width` | number | 0 | 0..4096 | - |
| `name` | string |  | - | - |
| `sprite` | string | - | - | - |
| `text` | string |  | - | - |
| `voice` | string |  | - | - |

### `[chapter]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `id` | string | - | - | - |
| `label` | string | - | - | - |

### `[cl]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string | all | - | - |

### `[close]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[emb]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `code` | string | - | - | - |
| `exp` | string | - | - | - |

### `[ending]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `id` | string | - | - | - |
| `name` | string | - | - | - |

### `[er]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[eval]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `code` | string | - | - | - |
| `exp` | string | - | - | - |

### `[fadebgm]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadein` | number | 0 | 0..30000 | - |
| `time` | number | 1000 | 0..30000 | - |
| `volume` | number | 0 | 0..1.5 | - |

### `[fadeout]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `alpha` | number | 0 | 0..1.0 | - |
| `duration` | number | 500 | 0..30000 | - |
| `opacity` | number | 0 | 0..1.0 | - |
| `time` | number | 500 | 0..30000 | - |

### `[fadevol]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `time` | number | 1000 | 0..30000 | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[fg]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |

### `[font]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | white | - | - |
| `face` | string | default | - | - |
| `size` | number | 22 | 4..256 | - |

### `[gallery]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `id` | string | - | - | - |

### `[history]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[hr]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[image]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `h` | number | - | 0..8192 | - |
| `layer` | string | fg | - | - |
| `storage` | string | - | - | - |
| `w` | number | - | 0..8192 | - |
| `x` | number | - | - | - |
| `y` | number | - | - | - |

### `[l]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[layopt]`

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

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `slot` | number | 0 | 0..99 | - |

### `[loadplace]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[move]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 300 | 0..30000 | - |
| `time` | number | 300 | 0..30000 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[music]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[nameplate]`

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

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[particles]`

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

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| **requires one of** | — | — | file, storage | yes |
| `fadein` | number | 0 | 0..30000 | - |
| `file` | string | - | - | - |
| `loop` | boolean | true | - | - |
| `storage` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[playbgmstop]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadein` | number | 0 | 0..30000 | - |
| `fadeout` | number | 0 | 0..30000 | - |
| `file` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[playse]`

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

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string |  | - | - |
| `name` | string |  | - | - |
| `pos` | string |  | - | - |
| `scale` | number | 1.0 | 0.01..16 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[pt]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speed` | number | 50 | 8..5000 | - |

### `[quake]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `amplitude` | number | 5 | 0..100 | - |
| `duration` | number | 300 | 0..30000 | - |
| `intensity` | number | 5 | 0..100 | - |
| `time` | number | 300 | 0..30000 | - |

### `[r]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[reset]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[ruby]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `ruby` | string |  | - | - |
| `ruby_scale` | number | 0.5 | 0.1..2.0 | - |
| `text` | string |  | - | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[s]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `ms` | number | 250 | 0..60000 | - |

### `[save]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `slot` | number | 0 | 0..99 | - |

### `[saveplace]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[scroll]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | white | - | - |
| `size` | number | 28 | 8..128 | - |
| `speed` | number | 60 | 1..1000 | - |
| `text` | string |  | - | - |

### `[setbgmvolume]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | - | 0..1.5 | - |

### `[setsevolume]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | - | 0..1.5 | - |

### `[setvoicevolume]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | - | 0..1.5 | - |

### `[shake]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `amplitude` | number | - | 0..100 | - |
| `intensity` | number | 6 | 0..100 | - |
| `time` | number | 500 | 0..30000 | - |

### `[sprite_fade]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speaker` | string | - | - | yes |
| `time` | number | 300 | 0..30000 | - |
| `to` | number | 255 | 0..255 | - |

### `[sprite_move]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `speaker` | string | - | - | yes |
| `time` | number | 400 | 0..30000 | - |
| `x` | number | 440 | - | - |
| `y` | number | 200 | - | - |

### `[stopbgm]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadeout` | number | 0 | 0..30000 | - |

### `[stopse]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fadeout` | number | 0 | 0..30000 | - |

### `[text]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `fade` | number | 0 | 0..30000 | - |
| `fade_time` | number | 0 | 0..30000 | - |
| `text` | string |  | - | - |

### `[textbox]`

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

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 500 | 0..30000 | - |
| `method` | string | crossfade | - | - |
| `time` | number | 500 | 0..30000 | - |

### `[vib]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `amplitude` | number | - | 0..50 | - |
| `intensity` | number | 3 | 0..50 | - |
| `time` | number | 300 | 0..30000 | - |

### `[video]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | yes |
| `h` | number | 0 | 0..8192 | - |
| `loop` | boolean | false | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |
| `w` | number | 0 | 0..8192 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[voice_off]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `on` | boolean | true | - | - |

### `[wait]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 1000 | 0..60000 | - |
| `ms` | number | 1000 | 0..60000 | - |
| `time` | number | 1000 | 0..60000 | - |

### `[waitforclick]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|

### `[xfadebgm]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `time` | number | 2000 | 0..30000 | - |

