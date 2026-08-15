# KAG Expression Language (Neo-Genesis)

> Reference for the expression syntax accepted by `[if]`, `[while]`, `[switch exp=]`,
> `[eval]`, `[assert]`, `${expr}` interpolation and `%tbl.key%` / `$tbl.key` variables.
> `[switch exp="f.tier"]` selects a case by evaluating an expression (round 55); the
> KAG3 positional form `[switch mode]` remains a bare variable name.
> Implementation: `scripts/kag/expr.lua` (TJS→Lua translation layer).

## Compatibility with legacy KAG3 / TJS

Legacy KAG3 scripts use TJS-style operators. The engine translates them to
Lua before compilation, **string-literal aware** (operators inside `'...'`
and `"..."` are never touched):

| TJS / KAG3          | Lua translation | Example                     |
|---------------------|-----------------|-----------------------------|
| `a && b`            | `a and b`       | `[if exp="f.hp > 0 && tf.flag"]` |
| `a \|\| b`          | `a or b`        | `[if exp="sf.x == 1 \|\| f.y"]`   |
| `!a`                | `not a`         | `[if exp="!tf.locked"]`     |
| `a != b`            | `a ~= b`        | `[if exp="f.name != 'Aoi'"]`|
| `cond ? a : b`      | `(cond and (a) or (b))` | `[ch text="Rank ${f.hp > 20 ? 'high' : 'low'}"]` |
| `a ?? b`            | `a or b`        | `[if exp="f.missing ?? 42"]` — null-coalesce: 0 / "" survive (Lua truthy) |

Nested ternaries and parenthesised sub-expressions are supported. The
ternary form is exact for numbers and strings; for booleans it follows
`and/or` semantics (`false` then-branch falls through — use explicit
`?? ` forms or parenthesised comparisons when that matters).

Ternaries may also appear **inside `[...]` index brackets and `(...)`
groups** — the translator flattens those innermost-out first, so
`f.arr[f.flag ? 1 : 2]` and `f.arr[1] + (f.flag ? f.arr[2] : f.arr[3])`
compile (rounds 61/68).

## Variable scopes

Both KAG3-style table access and bare identifiers resolve against the
current scene context:

| Scope | Meaning | Example |
|-------|---------|---------|
| `f`    | global flags (persist across scenes) | `f.hp` |
| `sf`   | scene flags (per scene)             | `sf.chapter` |
| `tf`   | temporary flags (per command chain) | `tf.flag` |
| `mp`   | message parameters (`[call ... mp=]`) | `mp.arg` |
| `lf`   | local frame (`[call]` pushes, `[return]` pops) | `lf.inner` |
| bare   | resolves in `f` | `score > 5` ≡ `f.score > 5` |

## Interpolation

Command params with `interpolate = true` (text fields) expand:

| Form | Example | Result |
|------|---------|--------|
| `$tbl.key` | `"HP: $f.hp"` | `HP: 30` |
| `%tbl.key%` | `"%f.name% says hi"` (KAG3 form) | `Aoi says hi` |
| `${expr}` | `"Score ${f.score * 10}"` | `Score 300` |

Bare `%ident%` is left untouched — it belongs to the macro placeholder
domain (`[macro m args="x"]` … `%x%` … `[endmacro]`).

`${expr}` spans are **balanced-brace aware**: table constructors and
Lua long strings inside the expression work — `${ {a=1,b=2}.b }` → `2`,
`${ [[x}}]] .. f.name }` → `x}}Ame` (rounds 54/62).

## [switch exp=...] — expression selectors

`[switch exp="expr"]` evaluates the expression and matches `[case v]`
by `tostring` equality — numbers and booleans work: `[case 2]` matches
`exp="f.tier"` with `f.tier = 2`, `[case true]` matches a boolean
condition (round 55). No matching case falls to `[default]`; a
failed/nil expression also lands on default. The positional form
`[switch mode]` remains a bare variable name (KAG3 form).

## [eval] — expression statements

`[eval exp="..."]` executes a statement in a whitelisted env
(`f`/`sf`/`tf`/`mp`/`lf` plus `math`/`string`/`table`/...). TJS
operators translate before load (round 61), and a ternary on the right
of an assignment works too — the translator splits at the first
top-level `=` and runs the full pipeline on the RHS (round 68):

```
[eval exp="f.score = f.base + 50"]          -- arithmetic assignment
[eval exp="f.ok = f.hp > 10 && f.flag"]     -- operators translate
[eval exp="f.pick = f.flag ? 1 : 2"]        -- ternary RHS (round 68)
[eval exp="f.arr = {10, 20, 30}"]          -- table constructor
```

A bare expression (no assignment) stores its result in `tf.eval_result`.

## Errors are visible

Compile or runtime errors print a `scene:line` diagnostic instead of
silently taking the else branch:

```
[KAG] expression error in scene.ks:12: kag_expr:1: unexpected symbol near ')' (source: f.hp == )
[KAG] expression runtime error in scene.ks:12: attempt to index a nil value (global 'f') (source: f.hp)
```

## Static validation

`ks_check.lua` pre-compiles every `exp=` it finds (translated) so broken
expressions fail the contract check before the scene runs.
