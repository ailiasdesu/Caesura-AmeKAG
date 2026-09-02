; =============================================================================
;  Caesura (AmeKAG) — New Project Template
;  A minimal two-scene skeleton with one player choice (a branching intro).
;
;  This is the starting point for your own visual novel. Replace the text
;  below with your story; add [bg]/[fg]/[playbgm]/[playse]/[playvoice] calls
;  as you go. Assets referenced here live in the shared repo pool
;  (assets/bg|fg|bgm|se|voice) so the template runs out of the box; for a
;  standalone project copy assets into your own <project>/assets/ tree (see
;  tools/project_templates/basic/assets/<category>/README.md).
;
;  Run:  lua tools/project_templates/basic/entry.lua
;  Check: lua scripts/ks_check.lua tools/project_templates/basic/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=50]
[textbox x=120 y=480 w=1040 h=200 color="20,24,32" opacity=210 visible=true]
[nameplate x=120 y=435 w=220 h=40 color="30,36,48" opacity=230 text_color="240,245,255"]

; =============================================================================
;  Scene 1 — Title / introduction
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[playbgm storage="assets/bgm/daily.wav" volume=0.6]
[ch name="Narrator" text="Welcome to your new Caesura project."]
[p]
[ch name="Narrator" text="This template gives you two scenes and one player choice to build on."]
[p]

; --- A character line with a shared sprite (real placeholder in assets/fg/) --
[playse storage="assets/se/click.wav"]
[ch name="Aina" text="Hi! I'm Aina, here to walk you through your first branching scene."
     sprite="assets/fg/girl_uniform.png"]
[p]

[ch name="Narrator" text="The road ahead forks. Which path will you follow?"]
[p]
[save slot=1]
[notify msg="Autosave complete / 自动存档已完成"]

; =============================================================================
;  Scene 2a / 2b — The branch (one player choice)
; =============================================================================
[select]
[sel target=*forest text="Follow the forest path"]
[sel target=*city text="Follow the city lights"]
[endselect]

*forest
[cl]
[trans method=dissolve]
[bg storage="assets/bg/hana.png"]
[wait time=400]
[ch name="Narrator" text="You chose the forest path. Birdsong, then silence."]
[p]
[ch name="Aina" text="A good start. Stories love a quiet turning point."]
[p]
[jump *credits]

*city
[cl]
[trans method=dissolve]
[bg storage="assets/bg/hana.png"]
[wait time=400]
[ch name="Narrator" text="You chose the city lights. The evening hums with promise."]
[p]
[ch name="Aina" text="Bright and restless. Another fine place to begin."]
[p]
[jump *credits]

; =============================================================================
;  Credits / ending
; =============================================================================
*credits
[cl]
[stopbgm fadeout=800]
[bg storage="assets/bg/hana.png"]
[scroll text="YOUR GAME TITLE" size=40]
[scroll text="Built with the Caesura (AmeKAG) engine" speed=70]
[p]

[ch name="Narrator" text="— End of template —"]
[p]
[ch name="Narrator" text="Edit tools/project_templates/basic/story.ks and make it yours."]
[p]
[wait time=800]
[end]

