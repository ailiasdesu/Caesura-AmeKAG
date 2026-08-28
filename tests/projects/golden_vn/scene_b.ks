; =============================================================================
;  Caesura (AmeKAG) — Golden Project Scene B (v1 cross-scene jump target)
;
;  Reached from story.ks via [jump storage="golden_scene_b.ks"]. At runtime the
;  scheduler resolves a non-* storage target as assets/script/<target>.ks
;  (scripts/scheduler.lua is_safe_scene_path), so this fixture file maps to the
;  logical name through a documented headless seam in
;  tests/scripts/golden_vn_headless.lua. Scene B is a stand-alone ending:
;  once entered it runs to [end] (the game is over -- no jump back), which is
;  why the cross-scene block sits last in story.ks.
;
;  Gate:  lua scripts/ks_check.lua tests/projects/golden_vn/scene_b.ks
; =============================================================================

[cl]
[bg storage="assets/bg/hana.png"]
[wait time=300]
[ch name="Narrator" text="Scene B: the second file runs after the cross-scene jump. / 场景 B：跨场景跳转后，第二个文件开始运行。"]
[p]
[set var="f.sceneB" value=1]
[ch name="Narrator" text="Cross-scene jump complete. / 跨场景跳转完成。"]
[p]
[wait time=400]
[end]
