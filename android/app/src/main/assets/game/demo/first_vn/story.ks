; =============================================================================
;  Caesura (AmeKAG) — First-VN E2E Project story.ks
;
;  The "first visual novel" a new author builds with Caesura Studio: two
;  scenes (*start -> *ending), one background, one sprite character,
;  dialogue, a 2-way player choice, BGM + SE, save/load, i18n hot-switch
;  and an ending both branches can reach ([end]).
;
;  Unlike tests/projects/golden_vn/ (Runtime release regression over the
;  FULL feature surface), this fixture models the COMPLETE USER CREATION
;  FLOW (task book §6): template -> project -> script -> assets ->
;  headless run -> choices -> save/load -> package -> packaged launch.
;
;  Headless-safety rules honored (see golden_vn README):
;    - no blocking [history] / [replay record] / [tween wait=true]
;    - [set] always uses the var=/value= contract
;    - [load slot=8] intentionally targets an EMPTY slot: the graceful
;      miss path exercises the load command flow without rewinding the
;      token cursor (a real round-trip would loop headless; restore
;      correctness is owned by golden_vn + C++ SaveManager suites)
;
;  Run:   lua tests/projects/first_vn/entry.lua
;  Gate:  bash scripts/verify_first_vn.sh
;  Check: lua scripts/ks_check.lua tests/projects/first_vn/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=40]

; =============================================================================
;  Scene 1 — *start : arrival (background + BGM + dialogue + SE + i18n + save)
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[playbgm storage="assets/bgm/daily.wav" volume=0.6]
[ch name="Narrator" text="Your first VN opens on an ordinary afternoon. / 你的第一部 VN 在一个平凡的午后开场。"]
[p]

[playse storage="assets/se/click.wav"]
[ch name="Aina" text="Hi! I'm Aina. Every Caesura story starts with a single scene."
     sprite="assets/fg/girl_uniform.png"]
[p]

[ch name="Narrator" text="Dialogue, a background, music -- you already have the pieces. / 对白、背景、音乐，素材已经凑齐了。"]
[p]

; --- i18n hot-switch (the demo/example_game pattern: inline bilingual text) --
[i18n language=en]
[ch name="Narrator" text="(en) Language switches are just another command. / （en）语言切换只是又一条命令。"]
[p]
[i18n language=ja]
[ch name="Narrator" text="(ja) Three locales, zero reloads. / （ja）三种语言，零重载。"]
[p]
[i18n language=zh]
[ch name="Narrator" text="(zh) Back to Chinese for the choice ahead. / （zh）切回中文，选择就在前方。"]
[p]

; --- autosave: the Studio Build Manager "Run" session saves like a real game -
[save slot=7]
[notify msg="First-VN autosaved / 首部 VN 已自动存档"]

; =============================================================================
;  Branch point — the player choice (2 routes, both reach *ending)
; =============================================================================
*choice_moment
[ch name="Aina" text="So -- where does YOUR story go next?" sprite="assets/fg/girl_uniform.png"]
[p]

[select]
[sel target=*branch_sun text="Walk home under the evening sun / 迎着夕阳走回家"]
[sel target=*branch_rain text="Wait out the sudden rain / 躲一场突如其来的雨"]
[endselect]

*branch_sun
[set var="f.route" value="sun"]
[set var="f.is_sun" value="1"]
[cl]
[trans method=dissolve]
[wait time=300]
[ch name="Narrator" text="You chose the sunset road. The sky turns amber. / 你选了夕阳的路，天空染成琥珀色。"]
[p]
[ch name="Aina" text="Warm endings usually start warm." sprite="assets/fg/girl_uniform.png"]
[p]
[jump *ending]

*branch_rain
[set var="f.route" value="rain"]
[set var="f.is_sun" value="0"]
[cl]
[trans method=dissolve]
[wait time=300]
[ch name="Narrator" text="You chose the bus-stop shelter. Rain drums on the roof. / 你选了公交站台，雨点敲着顶棚。"]
[p]
[ch name="Aina" text="Rainy scenes are for quiet confessions." sprite="assets/fg/girl_uniform.png"]
[p]
[jump *ending]

; =============================================================================
;  Scene 2 — *ending : both branches converge here, then [end]
; =============================================================================
*ending
[cl]
[trans method=dissolve]
[bg storage="assets/bg/classroom.png"]
[wait time=300]

; --- expression conditional proves the branch variable survived the jump -----
[if exp="f.is_sun == 1"]
[ch name="Narrator" text="The sun-route epilogue: amber light on your desk. / 日线路后日谈：书桌上是一片琥珀色。"]
[else]
[ch name="Narrator" text="The rain-route epilogue: the storm has passed. / 雨线路后日谈：暴风雨已经过去。"]
[endif]
[p]

; --- load command flow (empty slot 8: graceful miss, story continues) --------
; See file header: a REAL restore here would rewind the token cursor into a
; loop under automation; the miss path still drives [load]'s full pipeline.
[load slot=8]
[ch name="Narrator" text="(A load of empty slot 8 was attempted and handled gracefully.) / （对空存档位 8 的读取已被优雅处理。）"]
[p]

[playse storage="assets/se/click.wav"]
[ch name="Aina" text="That's a wrap on VN number one. Number two will be easier."
     sprite="assets/fg/girl_uniform.png"]
[p]

[stopbgm fadeout=800]
[ch name="Narrator" text="-- First VN complete / 第一部 VN 完成 --"]
[p]
[wait time=800]
[end]
