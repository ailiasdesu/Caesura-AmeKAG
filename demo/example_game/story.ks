; =============================================================================
;  Caesura (AmeKAG) — Example Game: "The Last Letter"
;  A complete short visual novel demonstrating KAG Neo-Genesis features:
;    · multi-chapter flow with [jump]/[call] and *labels
;    · player choices via [select]/[sel]/[endselect]
;    · three endings (good / normal / bad) + [ending] unlock
;    · variables (f./tf.), ${expr} interpolation, [if]/[while] control flow
;    · parameterized macros, [iscript] Lua hybrid, rollback, save/load
;  Run with:  lua scripts/kag_runner_demo.lua  (see demo/example_game/entry.lua)
; =============================================================================

[font face="default" size=22]
[pt speed=50]

; --- Parameterized macro: a reusable "narrate" block with interpolated args
[macro scene_intro args="bg,title"]
[cl]
[bg storage="assets/bg/%bg%"]
[wait time=400]
[ch name="Narrator" text="%title%"]
[p]
[endmacro]

; --- Story start ------------------------------------------------------------
[playbgm storage="assets/bgm/daily.wav" volume=0.7]

[scene_intro bg="classroom.png" title="Spring, first day of school."]

[ch name="Yuki" text="You're the new transfer student, right? I'm Yuki." 
     sprite="assets/fg/girl_uniform.png"]
[p]

[ch name="Yuki" text="The teacher asked me to show you around. Follow me!"]
[p]

; --- First choice: where to go -------------------------------------------------
[ch name="Narrator" text="Where do you want to visit first?"]
[p]

[select]
[sel target=*route_library text="The old library"]
[sel target=*route_rooftop text="The rooftop garden"]
[sel target=*route_gate text="Just walk home"]
[endselect]

; --- Route: Library (leads to good ending) ------------------------------------
*route_library
[scene_intro bg="classroom.png" title="The library smells of old paper."]

[ch name="Yuki" text="I found an old notebook... it's full of letters."]
[p]

[ch name="Narrator" text="The notebook belongs to a student from 30 years ago."]
[p]

[if exp="tf.curiosity == nil"]
[set tf.curiosity 1]
[endif]

[ch name="Yuki" text="If we return it to the owner... maybe we'll learn their story."]
[p]

; --- Second choice: investigate or leave -------------------------------------
[ch name="Narrator" text="What do you do with the notebook?"]
[p]

[select]
[sel target=*investigate text="Investigate the letters"]
[sel target=*leave_library text="Put it back and leave"]
[endselect]

*investigate
[ch name="Yuki" text="Look, the last letter says: 'meet me at the rooftop, spring festival day'."]
[p]
[scene_intro bg="hana.png" title="You read every letter. The story touches you."]

[jump target=*ending_good]

*leave_library
[scene_intro bg="classroom.png" title="You return the notebook. Some mysteries stay mysteries."]
[jump target=*ending_normal]

; --- Route: Rooftop (leads to normal ending) -----------------------------------
*route_rooftop
[scene_intro bg="hana.png" title="The rooftop garden is in full bloom."]

[ch name="Yuki" text="Wow, you can see the whole town from here!"]
[p]

[ch name="Narrator" text="The wind carries the cherry blossoms across the sky."]
[p]

[if exp="tf.curiosity == 1"]
[ch name="Yuki" text="By the way... you're the curious type, aren't you?"]
[p]
[endif]

[jump target=*ending_normal]

; --- Route: Gate (leads to bad ending) -----------------------------------------
*route_gate
[scene_intro bg="classroom.png" title="You walk home alone."]

[ch name="Narrator" text="Yuki watches you go, a little disappointed."]
[p]

[ch name="Yuki" text="...See you tomorrow, I guess."]
[p]

[jump target=*ending_bad]

; --- Endings ------------------------------------------------------------------
*ending_good
[cl]
[bg storage="assets/bg/hana.png"]
[trans time=500 method=dissolve]
[wait time=400]

[ch name="Narrator" text="Spring festival day. You wait on the rooftop."]
[p]

[ch name="Narrator" text="The notebook's owner — now an old woman — arrives with Yuki."]
[p]

[ch name="Yuki" text="She said: 'thank you for finding my story.'"]
[p]

[ch name="Narrator" text="Some stories end. Some stories begin again."]
[p]
[ending id=good_end name="Good End — The Last Letter"]
[p]
[jump target=*credits]

*ending_normal
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[ch name="Narrator" text="The season passes. You and Yuki become friends."]
[p]

[ch name="Yuki" text="Hey, let's write our own notebook someday."]
[p]
[ending id=normal_end name="Normal End — New Beginnings"]
[p]
[jump target=*credits]

*ending_bad
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[ch name="Narrator" text="You never speak to Yuki again."]
[p]

[ch name="Narrator" text="The notebook stays in the library, waiting."]
[p]
[ending id=bad_end name="Bad End — The Untold Story"]
[p]
[jump target=*credits]

; --- Credits -------------------------------------------------------------------
*credits
[stopbgm]
[scroll text="Caesura (AmeKAG) — Example Game" speed=80]
[scroll text="The Last Letter" speed=80]
[scroll text="A demonstration of KAG Neo-Genesis" speed=80]
[p]

[ch name="Narrator" text="Thank you for playing. You can rollback ([rollback]) or load a save."]
[p]

[history]
[p]

[end]
