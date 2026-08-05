# KAG Command Contracts (auto-generated)

> Generated from the declarative schema registry (`kag/schema.lua`) — do not edit.
> Regenerate: `lua scripts/schema_doc.lua > docs/api/command-contracts.md`

## Commands (32)

### `[auto]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `mode` | string | - | off,on,toggle | - |

### `[bgm]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `file` | string | - | - | - |
| `storage` | string | - | - | - |
| `volume` | number | 1.0 | 0..1.5 | - |

### `[ch]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `chars_per_line` | number | 0 | 0..512 | - |
| `max_width` | number | 0 | 0..4096 | - |
| `name` | string |  | - | - |
| `sprite` | string |  | - | - |
| `text` | string |  | - | - |
| `voice` | string |  | - | - |

### `[cl]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string | all | - | - |

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

### `[font]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `color` | string | white | - | - |
| `face` | string | default | - | - |
| `size` | number | 22 | 4..256 | - |

### `[layopt]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `layer` | string |  | - | - |
| `opacity` | number | 1.0 | 0..1.0 | - |
| `visible` | boolean | true | - | - |

### `[load]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `slot` | number | 0 | 0..99 | - |

### `[move]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 300 | 0..30000 | - |
| `time` | number | 300 | 0..30000 | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

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
| `volume` | number | 1.0 | 0..1.5 | - |

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

### `[ruby]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `ruby` | string |  | - | - |
| `ruby_scale` | number | 0.5 | 0.1..2.0 | - |
| `text` | string |  | - | - |
| `x` | number | 0 | - | - |
| `y` | number | 0 | - | - |

### `[save]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `slot` | number | 0 | 0..99 | - |

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
| `volume` | number | 1.0 | 0..1.5 | - |

### `[setsevolume]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | 1.0 | 0..1.5 | - |

### `[setvoicevolume]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `volume` | number | 1.0 | 0..1.5 | - |

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

### `[trans]`

| Param | Type | Default | Range / Choices | Required |
|---|---|---|---|---|
| `duration` | number | 500 | 0..30000 | - |
| `method` | string | crossfade | - | - |
| `time` | number | 500 | 0..30000 | - |

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

