; =============================================================================
;  Caesura (AmeKAG) — Golden Project cross-scene starter (v1)
;
;  Dedicated driver: story.ks keeps its choices on the main path, and the
;  cross-scene [jump] gets its OWN starter so the scheduler's deferred
;  choice pending-jump (consumed at the next coroutine death) never overlaps
;  the scene switch. At runtime the scheduler resolves a non-* storage target
;  as assets/script/<target>.ks (scripts/scheduler.lua is_safe_scene_path);
;  the headless golden driver maps that logical name to
;  tests/projects/golden_vn/scene_b.ks (documented test seam -- see
;  golden_vn_headless.lua), so this starter is also runnable with the plain
;  sample-game driver, where the jump takes its graceful-miss path and the
;  lines below it continue to [end].
;
;  Gate:  lua scripts/ks_check.lua tests/projects/golden_vn/golden_cross.ks
; =============================================================================

*start
[cl]
[bg storage="assets/bg/hana.png"]
[wait time=300]
[ch name="Narrator" text="Cross-scene starter: jumping into scene_b.ks. / 跨场景起点：跳入场景 B。"]
[p]
[ch name="Narrator" text="(storage target resolves under assets/script/ at runtime) / （运行时 storage 目标解析到 assets/script/ 下）"]
[p]
[jump storage="golden_scene_b.ks"]
[set var="f.crossFallback" value=1]
[ch name="Narrator" text="(fallback path: scene_b not loaded by this driver) / （回退路径：本驱动未加载场景 B）"]
[p]
[wait time=400]
[end]
