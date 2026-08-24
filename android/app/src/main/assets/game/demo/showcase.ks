; =============================================================================
; Caesura KAG command showcase — sample-library entry #1 (round 41).
; Exercises a broad command subset so both the engine and the web player
; are validated against real content, not just unit fixtures.
; =============================================================================
[font face="default" size=22]
[pt speed=60]

*start

; --- 1. background + music ---
[bg storage="assets/bg/classroom.png"]
[playbgm storage="assets/bgm/daily.wav" volume=0.7]
[wait time=300]

[ch name="Narrator" text="Welcome to the Caesura command showcase."]
[p]

; --- 2. character sprite + voice + se ---
[ch name="Narrator" text="First: a character sprite with voice."]
[playse storage="assets/se/click.wav"]
[ch name="Sakura" text="Hello! This is a voiced line with a sprite."
     voice="assets/voice/line01.wav" sprite="assets/fg/girl_uniform.png"]
[p]

*scene2
; --- 3. foreground layer + position ---
[cl]
[bg storage="assets/bg/hana.png"]
[trans time=400 method=dissolve]
[fg storage="assets/fg/girl_uniform.png"]
[position layer="fg" pos="right"]
[ch name="Sakura" text="Now the foreground layer is positioned right."]
[p]

; --- 4. sprite animation ---
[sprite_move speaker="Sakura" x=120 y=200 time=400]
[sprite_fade speaker="Sakura" to=200 time=300]
[ch name="Sakura" text="I just slid to the left and faded slightly."]
[p]

; --- 5. flash + vibration effects ---
[flash time=200 r=255 g=255 b=255]
[vib time=150 intensity=3]
[ch name="Narrator" text="A flash and a little shake — KAG effects work."]
[p]

; --- 6. branching: random + if + jump ---
; round 46 fix: [set] does not evaluate expressions; use [eval] for that
[eval exp="f.luck = math.random(2)"]
[if exp="f.luck == 1"]
[ch name="Narrator" text="The coin says HEADS — good luck path."]
[jump target=*ending_good]
[else]
[ch name="Narrator" text="The coin says TAILS — normal path."]
[endif]

[jump target=*ending_normal]

*ending_good
[ch name="Narrator" text="Lucky ending reached!"]
[p]
[jump target=*credits]

*ending_normal
[ch name="Narrator" text="Normal ending reached."]
[p]

*credits
; --- 7. credits + stop + ending unlock ---
[cl]
[bg storage="assets/bg/hana.png"]
[stopbgm]
[scroll text="Caesura Showcase" speed=80]
[scroll text="Every command shown here runs in the web player too" speed=80]
[ch name="Narrator" text="Thanks for watching the showcase."]
[ending id=showcase_done name="Showcase Completed"]
[p]
[wait time=500]
[end]

*unreachable
[ch name="Narrator" text="This line should never run."]
