; =============================================================================
;  Caesura (AmeKAG) — Sample Game: "The One-Way Reply" / 《单程回信》
;  v1 skeleton (round 102): scenes 0-2 complete, 3-6 labeled stubs to fill.
;  Design authority: demo/example_game/DESIGN.md
;  Run with:  lua scripts/kag_runner_demo.lua  (see demo/example_game/entry.lua)
; =============================================================================

[font face="default" size=22]
[pt speed=50]
[textbox x=120 y=480 w=1040 h=200 color="20,24,32" opacity=210 visible=true]
[nameplate x=120 y=435 w=220 h=40 color="30,36,48" opacity=230 text_color="240,245,255"]

; --- Parameterized macro: scene entry with backdrop + title -----------------
[macro scene_open args="bg,title,trans"]
[cl]
[bg storage="assets/bg/%bg%"]
[wait time=400]
[ch name="Narrator" text="%title%"]
[p]
[endmacro]

; ===========================================================================
;  Scene 0 — Title / opening (0.5 min)
; ===========================================================================
[playbgm storage="assets/bgm/daily.wav" volume=0.6]

[scene_open bg="classroom.png" title="Spring. A transfer student arrives at a school that time forgot."]

[ch name="Narrator" text="The old wing of the campus was supposed to be off-limits."]
[p]
[ch name="Narrator" text="But on the first morning, a single door was already open."]
[p]

; --- First i18n hot-switch demo: the game speaks both languages --------------
[i18n language=en]
[ch name="Narrator" text="This sample game speaks two languages. {settings}"]
[p]
[i18n language=zh]
[ch name="Narrator" text="这个示例游戏支持中英双语。{settings}"]
[p]
[i18n language=en]

; ===========================================================================
;  Scene 1 — Morning classroom (2.5 min) — Mio debut, save point 1
; ===========================================================================
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=300]

[playvoice storage="assets/voice/line01.wav"]
[playse storage="assets/se/click.wav"]
[ch name="Mio" text="...You're the new student? This seat is taken. By no one, really. But I sit here."
     sprite="assets/fg/girl_uniform.png"]
[p]

[position layer=fg pos=right]
[ch name="Mio" text="I'm Mio. I keep the keys to the old wing."]
[p]
[tween target="Mio" attr="x" from=1280 to=480 dur=600 ease=ease_out]
[tween target="Mio" attr="alpha" from=255 to=120 dur=400 ease=ease_out]
[ch name="Mio" text="If you hear a mailbox at night... don't answer it."]
[p]
[tween target="Mio" attr="alpha" from=120 to=255 dur=400 ease=ease_out]

; --- i18n plural demo ({items} expands one/other by n) -----------------------
[set tf.letters 1]
[ch name="Mio" text="I counted ${tf.letters} ${tf.letters == 1 and 'item' or 'items'} in the box this morning."]
[p]
[set tf.letters 3]
[ch name="Mio" text="Today there are ${tf.letters} ${tf.letters == 1 and 'item' or 'items'}. The box only grows on its own."]
[p]

; --- Save point 1 -------------------------------------------------------------
[save slot=1]
[notify msg="Autosave complete / 自动存档已完成"]

; ===========================================================================
;  Scene 2 — The attic and the old mailbox (2.5 min) — atmosphere + loops
; ===========================================================================
[cl]
[trans method=dissolve]
[bg storage="assets/bg/hana.png"]
[vfx postfx="vignette" amount=0.4]
[blur amount=0.25]
[particles rate=12 sizeMax=2 sizeMin=1 speedMax=1 speedMin=0.5]
[wait time=500]
[blur amount=0]

[ch name="Narrator" text="The attic breathed dust. In the corner: a tin mailbox, bolted to a desk."]
[p]
[vib intensity=1 time=200]
[ch name="Narrator" text="It had no address. No flag. No owner's name."]
[p]

; --- Loop demo: reading the three letter slots ---------------------------------
[set f.clues 0]
[for var=i start=1 end=3]
[eval exp="f.clues = f.clues + 1"]
[ch name="Narrator" text="Slot ${f.i}: an envelope. ${f.clues} clue${f.clues > 1 and 's' or ''} found so far."]
[p]
[endfor]

[ch name="Mio" text="The letters here are replies to letters that were never sent."]
[p]
[ruby text="澪" ruby="mio"]
[ch name="Mio" text="My name has the water radical. Water never answers back."]
[p]

; --- The first letter ---------------------------------------------------------
[ch name="Narrator" text="On the back of the last envelope, in faded ink, a date:"]
[p]
[textspeed cps=30]
[ch name="Narrator" text="Seventeen years ago. Tomorrow."]
[p]
[textspeed cps=50]

; ===========================================================================
;  Scene 3 — First one-way reply (2.5 min) — first player choice
; ===========================================================================
*scene3
[ch name="Narrator" text="You hold a pen over the blank page."]
[p]
[ch name="Narrator" text="What do you write back?"]
[p]

[select]
[sel target=*probe text="Ask about the impossible date"]
[sel target=*poke text="Joke that the mailbox is haunted"]
[sel target=*drop text="Put the letter back untouched"]
[endselect]

*probe
[set f.trust 1]
[ch name="Mio" text="You noticed the date too... good."]
[p]
[jump *post_choice]

*poke
[set f.trust 0]
[ch name="Mio" text="Haunted? Maybe. The ghost is just slower than us."]
[p]
[jump *post_choice]

*drop
[set f.trust -1]
[ch name="Narrator" text="You slide the letter back under the flap."]
[p]
[ch name="Mio" text="...Okay. The box waits."]
[p]

*post_choice
[ch name="Narrator" text="The mailbox is quiet again."]
[p]

; ===========================================================================
;  Scene 4 — The investigation (3 min) — trust-driven diff, save point 2
; ===========================================================================
*scene4
[cl]
[vfx postfx="none"]
[trans method=dissolve]
[bg storage="assets/bg/classroom.png"]
[preload storage="assets/bg/hana.png"]
[wait time=400]

; --- trust deepens from the investigation itself (scene 3 set the seed) -------
[add name="f.trust" value=1]

[ch name="Narrator" text="The records room. Seventeen years of dust over one folder labeled 'missing, spring term'."]
[p]
[ch name="Narrator" text="You pull the register for the old building. A name is circled in red ink."]
[p]

; --- trust-driven differential text: how Mio opens up depends on trust ------
[if exp="f.trust >= 1"]
[ch name="Mio" text="That circle is my handwriting. I've read this page so many times it went grey."]
[p]
[else]
[ch name="Mio" text="That red circle... I always wondered who else would ever find it."]
[p]
[endif]

[if exp="f.trust >= 2"]
[ch name="Mio" text="...You trust me this far. Then I can tell you the part I never say aloud."]
[p]
[else]
[ch name="Narrator" text="Mio hesitates, then looks away. Trust is still a thin thread here."]
[p]
[endif]

; --- i18n hot-switch: one banner line spoken in both languages ---------------
[i18n language=en]
[ch name="Narrator" text="A stiff wind rattles the window. {investigate_sigh}"]
[p]
[i18n language=zh]
[ch name="Narrator" text="一阵风撞在窗上。{investigate_sigh}"]
[p]
[i18n language=en]

; --- clue accumulation via [add]/[sub] ---------------------------------------
[add name="f.clues" value=1]
[ch name="Narrator" text="Clue one: the missing student was seventeen this spring — the same age Mio is now."]
[p]
[add name="f.clues" value=1]
[ch name="Narrator" text="Clue two: their last, unsent letter was addressed to the attic mailbox."]
[p]
[add name="f.clues" value=1]
[ch name="Narrator" text="Clue three: a class register marks the day before graduation as their last attendance."]
[p]
[sub name="f.clues" value=1]
[ch name="Narrator" text="A red-herring note checks out as a prank — the real pile of evidence stands at ${f.clues}."]
[p]

; --- [while] loop: page through the missing-person archive -------------------
[set f.progress 0]
[set f.archive_done false]
[while exp="f.archive_done == false"]
[add name="f.progress" value=1]
[eval exp="f.archive_done = (f.progress >= 3)"]
[ch name="Narrator" text="Archive page ${f.progress} of the old case: testimony, a photograph, a blank line for 'address'."]
[p]
[endwhile]
[set f.archive_done nil]

; --- Ushio, the caretaker, flashes in as a witness ---------------------------
[flash r=255 g=235 b=190 time=180]
[csp name="Ushio" storage="assets/fg/girl_uniform.png" x=900 y=120]
[csl name="Ushio" x=960 y=150]
[ch name="Ushio" text="You two. The attic box rings at midnight — I've heard it for seventeen years. It calls for one name."]
[p]
[ch name="Ushio" text="When she answers and nothing comes back... that's when a girl walks out into the rain."]
[p]
[csd name="Ushio"]

; --- a diary snippet unlocks a CG; chapter select is available ----------------
[unlock type="cg" id="crippled_diary" name="The Empty Address"]
[chapter id="ch4" label="The Empty Address"]

[ch name="Mio" text="The box isn't cursed. It just carries one letter that never reached its writer."]
[p]
[ch name="Narrator" text="It is late. The storm line crawls toward the school."]
[p]

; --- Save point 2 before the branch diverges ----------------------------------
[save slot=2]
[notify msg="Save point 2 / 存档点② —— branch point (信任 f.trust=${f.trust})"]

; --- Branch B: trust >= 2 -> truth lead; otherwise -> companion lead ----------
[if exp="f.trust >= 2"]
[jump *truth_lead]
[else]
[jump *companion_lead]
[endif]

; --- truth lead: Mio trusts you with the real secret --------------------------
*truth_lead
[ch name="Mio" text="Seventeen years ago, my sister wrote one last letter and never came back to post it."]
[p]
[ch name="Mio" text="The mailbox answered her silence with all my replies. I have been writing to her ever since."]
[p]
[add name="f.trust" value=1]
[jump *post_investigate]

; --- companion lead: you walk the case together without the deepest secret ---
*companion_lead
[ch name="Mio" text="I only know the box answers. I never knew who was on the other side of it."]
[p]
[ch name="Mio" text="Stay with me a little longer? The rain is close."]
[p]
[jump *post_investigate]

*post_investigate
[ch name="Narrator" text="Night falls on the old wing. Somewhere a door to the attic stands open."]
[p]

; ===========================================================================
;  Scene 5 — Rain-night climax (4 min) — storm fx + SMA phantom + timed choice
; ===========================================================================
*scene5
[cl]
[trans method=crossfade]
[bg storage="assets/bg/hana.png"]
[palette effect="night" intensity=0.6]
[wait time=400]

; --- storm ambience: low rumble + looping click as rain/thunder --------------
[playbgm storage="assets/bgm/daily.wav" volume=0.3]
[setbgmvolume volume=0.35]

; --- sister's diary narration scrolls over the downpour -----------------------
;   ([scroll] renders raw text — the diary is written bilingually here, matching
;    the credits [scroll] convention; i18n key-token expansion applies to [ch] lines.)
[scroll text="もし、卒業の前夜—— / If, on the night before graduation —" speed=55]
[scroll text="あの手紙を、あんなに強く握っていなければ…… / I had not held that letter so tightly..." speed=55]
[wait time=300]

[flash r=230 g=230 b=255 time=150]
[playse storage="assets/se/click.wav" volume=0.7]
[vfx postfx="bloom" strength=0.8]
[quake intensity=8 time=350]
[shake time=400 amplitude=7]
[vib intensity=5 time=200]
[vibrate time=150]
[ch name="Narrator" text="Thunder. The whole old wing rattles in its bones."]
[p]

[nameplate]
[ch name="Mio" text="That was the diary she left in the box. She wrote it for me — so I would know it wasn't my fault." sprite="assets/fg/girl_uniform.png"]
[p]

; --- the messenger phantom — a skeletal shape in the corner of the attic -----
;   Skeletal mesh animation (SMA) fusion (design §4.5). Headless-safe: the
;   asset is registered via [iscript]; if the SMA binding is absent every
;   [sma_*] call below degrades to an inert no-op.
[iscript]
local ok_sma, sma_m = pcall(require, "kag.sma")
if ok_sma and type(sma_m) == "table" and type(sma_m.load) == "function" then
    local fc = io.open("demo/assets/sma/hero.json", "r")
    if fc then
        local s = fc:read("*a"); fc:close()
        local ok2, asset = pcall(sma_m.load, s)
        if ok2 and type(asset) == "table" then sma_m.register("hero", asset) end
    end
end
[/endscript]
[sma_play name="phantom" asset="hero" anim="idle" x=0.5 y=0.5 scale=1.4 tex=0 loop=true opacity=0.55]
[ch name="Narrator" text="In the dark of the attic a thin, grey shape begins to breathe — idle bones, waiting."]
[p]
[sma_anim name="phantom" anim="wave" blend_time=0.4 loop=true]
[flash r=255 g=255 b=255 time=120]
[ch name="Mio" text="It waves. Seventeen years ago, that was how she said goodbye from the window."]
[p]
[sma_variant name="phantom" part="eyes" variant="happy"]
[ch name="Narrator" text="The hollow eyes soften — happy, the way she looked before the rain."]
[p]
[sma_ik name="phantom" bone0=5 bone1=6 tx=0.95 ty=0.25 l2=0.3]
[ch name="Mio" text="Now it points... toward the mailbox. Toward the last letter."]
[p]
[sma_stop name="phantom"]

; --- the red act of choice: storm gives you a few beats to decide ------------
[ch name="Narrator" text="Mio hands you her sister's final letter. The postbox is one step away."]
[p]
[scroll text="雨音は、郵便が扉を叩く音。 / The rain is the postman knocking." speed=55]
[until exp="f.lightning == true" timeout=6000]
[ch name="Narrator" text="{final_choice}"]
[p]

[button text="Throw the letter in — send it back to her" target="*ending_zero" cond="f.trust >= 2"]
[button text="Tear it up — let the box go silent" target="*ending_promise"]
[button text="Keep it — carry her story into today" target="*ending_companion"]
[endbutton]
[p]

; ===========================================================================
;  Endings 6a / 6b / 6c — three endings ([ending] unlock wired)
; ===========================================================================
*ending_zero
[cl]
[vfx postfx="none"]
[bg storage="assets/bg/hana.png"]
[palette effect="night" intensity=0.4]
[trans time=700 method=crossfade]
[wait time=400]
[ch name="Narrator" text="{end_zero_a}"]
[p]
[ch name="Narrator" text="{end_zero_b}"]
[p]
[ch name="Mio" text="{end_zero_c}"]
[p]
[ch name="Narrator" text="{end_zero_d}"]
[p]
[unlock type="cg" id="zero_hour_cg" name="Seventeen Years Reunited"]
[ending id="zero_hour" name="Zero Hour / 归零"]
[p]
[ch name="Narrator" text="Replay? You can revisit any route from this branch with [save slot=2], browse the [gallery], or open [history]."]
[p]
[stopbgm fadeout=1500]
[jump *credits]

*ending_companion
[cl]
[vfx postfx="none"]
[bg storage="assets/bg/classroom.png"]
[trans time=600 method=dissolve]
[wait time=400]
[ch name="Narrator" text="{end_companion_a}"]
[p]
[ch name="Mio" text="{end_companion_b}"]
[p]
[ch name="Narrator" text="{end_companion_c}"]
[p]
[ch name="Narrator" text="{end_companion_d}"]
[p]
[unlock type="cg" id="companion_cg" name="The Sealed Postbox, Together"]
[ending id="companion" name="Companion / 同行"]
[p]
[ch name="Narrator" text="Another ending? [save slot=2] holds the branch — or roll back with [rollback] to choose again."]
[p]
[stopbgm fadeout=1500]
[jump *credits]

*ending_promise
[cl]
[vfx postfx="none"]
[bg storage="assets/bg/classroom.png"]
[trans time=600 method=dissolve]
[wait time=400]
[ch name="Narrator" text="{end_promise_a}"]
[p]
[ch name="Narrator" text="{end_promise_b}"]
[p]
[ch name="Narrator" text="{end_promise_c}"]
[p]
[ch name="Mio" text="{end_promise_d}"]
[p]
[unlock type="cg" id="promise_cg" name="The Silent Postbox"]
[ending id="promise" name="Promise / 守约"]
[p]
[ch name="Narrator" text="You can load the save at [save slot=2] and try the other two endings anytime."]
[p]
[stopbgm fadeout=1500]
[jump *credits]

; ===========================================================================
;  Credits
; ===========================================================================
*credits
[cl]
[stopbgm]
[bg storage="assets/bg/hana.png"]
[scroll text="THE ONE-WAY REPLY" size=40]
[scroll text="《单程回信》" size=40]
[scroll text="A short visual novel built on the Caesura (AmeKAG) engine" speed=70]
[p]

[ch name="Narrator" text="— Cast —"]
[p]
[ch name="Mio" text="Mio — the girl who keeps the keys to the old wing. (girl_uniform.png placeholder)"]
[p]
[ch name="Ushio" text="Ushio — the caretaker who heard the box ring for seventeen years. (placeholder sprite)"]
[p]
[ch name="Narrator" text="— Engine —"]
[p]
[ch name="Narrator" text="Caesura (AmeKAG): C++20 + bgfx render + SDL3 + SoLoud audio + Lua 5.4 scripting."]
[p]
[ch name="Narrator" text="KAG Neo-Genesis — 118 contract-verified commands; SMA skeletal-mesh animation; i18n."]
[p]
[ch name="Narrator" text="Thank you for playing. / 感谢游玩。"]
[p]

[notify msg="History is in the system menu (H) / 历史见系统菜单"]
[notify msg="Gallery: endings unlocked / 画廊：结局已解锁"]
[wait time=800]
[end]