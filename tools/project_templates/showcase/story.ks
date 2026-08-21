; =============================================================================
;  Caesura (AmeKAG) — Showcase VN Template
;
;  A feature tour rather than a real story: one scene per headline capability
;  so you can see what the engine can do before you start writing. A lighter,
;  "show" version of tests/projects/golden_vn (which is the long regression
;  fixture; this template is meant to be read and trimmed).
;
;  Covered here: select/branch · tween · layout (hbox) · layfade · i18n
;  hot-switch · nvl · trans · save · conditionals · flash · scroll · ending.
;
;  Run:   lua tools/project_templates/showcase/entry.lua
;  Check: lua scripts/ks_check.lua tools/project_templates/showcase/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=40]

; =============================================================================
;  Scene 1 — Entrance + a branch to pick what to see
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=300]

[playbgm storage="assets/bgm/daily.wav" volume=0.5]
[ch name="Narrator" text="Welcome to the Caesura showcase template. Pick a feature to explore." sprite="assets/fg/girl_uniform.png"]
[p]

*choice
[select]
[sel target=*fx text="Tween & Layout"]
[sel target=*nvl text="NVL & i18n"]
[sel target=*ending text="Trans / Save / Ending"]
[endselect]

; =============================================================================
;  Scene 2 — Declarative tween + layout container
; =============================================================================
*fx
[cl]
[trans method=dissolve]
[bg storage="assets/bg/hana.png"]
[i18n language=en]
[ch name="Narrator" text="A declarative tween slides the sprite across the foreground layer."]
[p]
[layfade layer="bg" to=0 time=200]
[tween target="fg" attr="x" from=0 to=320 dur=600 ease=ease_in_out wait=false]
[ch name="Heroine" text="This is a tween — smooth motion without manual frame math." sprite="assets/fg/girl_uniform.png"]
[p]
[tween target="fg" attr="x" from=320 to=0 dur=600 ease=ease_out wait=false]
[ch name="Narrator" text="And back home. [layout] then auto-places elements in a container."]
[p]
[layout name="panel" kind="hbox" gap=24 align=center x=80 y=520 w=1120 h=100]
[layout_slot parent="panel" layer="fg" index=1 size="200x100"]
[ch name="Narrator" text="Layout computes x/y for you — an hbox here."]
[p]
[jump *nvl]

; =============================================================================
;  Scene 3 — NVL + i18n hot-switch
; =============================================================================
*nvl
[cl]
[trans method=pushright]
[bg storage="assets/bg/classroom.png"]
[nvl]
[ch name="Narrator" text="NVL mode accumulates a full-screen block, Ren'Py style."]
[ch name="Narrator" text="Both paragraphs render together instead of clearing between lines."]
[nvl off]
[p]
[i18n language=en]
[ch name="Narrator" text="i18n hot-switches the UI language mid-scene — this line is English."]
[p]
[i18n language=ja]
[ch name="Narrator" text="今は日本語です。"]
[p]
[i18n language=zh]
[ch name="Narrator" text="这一行是中文。"]
[p]

; --- save + typed conditional ----------------------------------------------
[save slot=9]
[notify msg="Showcase autosaved / 展示存档已完成"]
[set var="f.caught" value=1]
[if exp="f.caught == 1"]
[ch name="Narrator" text="Save slot 9 is written, and typed conditionals work."]
[else]
[ch name="Narrator" text="This branch never runs (f.caught == 1)."]
[endif]
[p]
[jump *ending]

; =============================================================================
;  Scene 4 — Transition + save + ending
; =============================================================================
*ending
[cl]
[stopbgm fadeout=800]
[flash time=200 r=255 g=255 b=255]
[bg storage="assets/bg/hana.png"]
[scroll text="SHOWCASE TEMPLATE" size=40]
[scroll text="tween / layout / i18n / nvl / trans / save / conditionals" speed=70]
[p]
[ch name="Narrator" text="— A tour of the engine's headline features. Trim what you do not need —"]
[p]
[ending id=showcase_done name="Showcase Completed"]
[p]
[wait time=500]
[end]
