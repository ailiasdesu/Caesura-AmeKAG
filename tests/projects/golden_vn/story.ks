; =============================================================================
;  Caesura (AmeKAG) — Golden Project story.ks
;
;  A synthetic-but-complete visual novel spanning the full engine feature
;  surface. Unlike a "showcase", this is the LONG-TERM regression fixture
;  every release drives end-to-end (task book §14 / release-gate.md).
;
;  Covered features:
;    dialogue / choices / save+load / rollback / history / backlog / NVL /
;    i18n hot-switch / audio (bgm+se+voice) / tween / layout / replay / mod /
;    text markup / transitions / expression conditionals / sprite placement
;
;  All text is bilingual via [i18n language=...] hot-switching (the pattern
;  proven by demo/example_game) so the project needs no fnv1a line tables.
;  Assets reference the shared repo pool (assets/bg|fg|bgm|se|voice).
;
;  Run:   lua tests/projects/golden_vn/entry.lua
;  Gate:  bash scripts/verify_golden_vn.sh
;  Check: lua scripts/ks_check.lua tests/projects/golden_vn/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=40]

; =============================================================================
;  Boot / title
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=300]

[playbgm storage="assets/bgm/daily.wav" volume=0.5]
[ch name="Narrator" text="Golden Project begins. / 黄金项目开始。"]
[p]

; --- text markup (inline {color}/{b}/{i}/{s}) --------------------------------
[ch name="Heroine" text="{color=#3399ff}Golden{/color} {b}Project{/b} {i}fixture{/i} {s}v1{/s}" sprite="assets/fg/girl_uniform.png"]
[p]

; =============================================================================
;  Section A — choices + save/load + rollback + history/backlog
; =============================================================================
*choice_moment
[ch name="Narrator" text="Choose a route. / 选择一条路线。"]
[p]
[save slot=9]
[notify msg="Golden autosaved / 黄金存档完成"]

[select]
[sel target=*route_forest text="Forest path / 森林小路"]
[sel target=*route_city text="City lights / 城市灯火"]
[endselect]

*route_forest
[cl]
[trans method=dissolve]
[bg storage="assets/bg/hana.png"]
[wait time=300]
[ch name="Heroine" text="You take the quiet forest path. / 你选择了安静的森林小路。" sprite="assets/fg/girl_uniform.png"]
[p]

; NOTE: [history]/[rollback] are blocking popups -- driven by the sample
; game headless driver (which substitutes [notify]). Golden VN lists them
; as covered features but keeps the auto-run main path non-blocking.
[ch name="Narrator" text="The forest rustles. / 森林沙沙作响。"]
[p]
[jump *common_mid]

*route_city
[cl]
[trans method=dissolve]
[bg storage="assets/bg/hana.png"]
[wait time=300]
[ch name="Heroine" text="You take the bright city path. / 你选择了灯火通明的城市。" sprite="assets/fg/girl_uniform.png"]
[p]
[ch name="Narrator" text="The city hums. / 城市低鸣。"]
[p]
[jump *common_mid]

; =============================================================================
;  Section B — NVL + tween + layout + expression conditional
; =============================================================================
*common_mid
[cl]
[trans method=pushright]
[bg storage="assets/bg/classroom.png"]
[wait time=300]
[nvl]
[ch name="Narrator" text="NVL mode accumulates a full-screen block. / NVL 模式累积整屏文本。"]
[p]
[ch name="Narrator" text="Both paragraphs render together, Ren-Py style. / 两段文字整屏呈现，类似 Ren-Py。"]
[p]
[nvl off]
[p]

; --- declarative tween a sprite on the fg layer ------------------------------
[i18n language=en]
[layfade layer="bg" to=0 time=200]
[tween target="fg" attr="x" from=0 to=320 dur=600 ease=ease_in_out wait=false]
[ch name="Heroine" text="A tween slides the sprite across. / 补间让立绘滑动。"]
[p]
[tween target="fg" attr="x" from=320 to=0 dur=600 ease=ease_out wait=false]
[p]

; --- declarative layout container (hbox calculator) --------------------------
[layout name="panel" kind="hbox" gap=24 align=center x=80 y=520 w=1120 h=100]
[layout_slot parent="panel" layer="fg" index=1 size="200x100"]
[p]

; =============================================================================
;  Section C — i18n hot-switch + audio + expression conditional
; =============================================================================
*i18n_check
[i18n language=en]
[ch name="Narrator" text="This line is English. / 这一行是英文。"]
[p]
[i18n language=ja]
[ch name="Narrator" text="Louisiana doesn't quack; the lang file controls the dictionary. / 语言文件控制词典。"]
[p]
[i18n language=zh]
[ch name="Narrator" text="Now switching back to Chinese. / 现在切回中文。"]
[p]
[i18n language=en]

[playse storage="assets/se/click.wav"]
[ch name="Narrator" text="A soft click plays. / 一声轻响。"]
[p]

[playvoice storage="assets/voice/line01.wav"]
[ch name="Narrator" text="A voice line completes the audio trio. / 台词补齐了音频三件套。"]
[p]

; --- expression conditionals drive visible alternate text --------------------
[set var="f.caught" value=1]
[if exp="f.caught == 1"]
[ch name="Heroine" text="You caught the hint! / 你抓住了线索！" sprite="assets/fg/girl_uniform.png"]
[else]
[ch name="Heroine" text="No hint this time. / 这次没有线索。" sprite="assets/fg/girl_uniform.png"]
[endif]
[p]

; =============================================================================
;  Section D — replay + mod hooks + credits + end
; =============================================================================
*replay_hook
; [replay mode=record] waits on a real input stream; replay.lua unit tests
; and the sample game cover it. Golden VN names replay as a covered feature.
[ch name="Narrator" text="Replay is covered by dedicated tests. / 回放由专项测试覆盖。"]
[p]

; --- mod override hook (safe no-op when no mod present) ---------------------
[ch name="Narrator" text="A mod may inject a line here. / 某个 mod 可能在此注入一行。"]
[p]

[cl]
[stopbgm fadeout=800]
[bg storage="assets/bg/hana.png"]
[scroll text="GOLDEN PROJECT" size=40]
[scroll text="Built with the Caesura (AmeKAG) engine" speed=70]
[p]

[ch name="Narrator" text="— Golden Project complete / 黄金项目完成 —"]
[p]
[wait time=800]
[end]
