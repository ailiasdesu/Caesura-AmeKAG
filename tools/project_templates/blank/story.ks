; =============================================================================
;  Caesura (AmeKAG) — Blank VN Template
;
;  The smallest possible starting point: ONE scene, a line or two of opening
;  dialogue, one placeholder [ch] where your story drops in, then [end].
;
;  There is deliberately NO sample story text, NO branching, and NO save —
;  this is a "start from zero" skeleton. Open the game, confirm it boots,
;  then replace the lines below with your own story. Add
;  [bg]/[fg]/[playbgm]/[playse]/[playvoice]/[select] as you go (full command
;  reference: docs/api/command-contracts.md).
;
;  Run:   lua tools/project_templates/blank/entry.lua
;  Check: lua scripts/ks_check.lua tools/project_templates/blank/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=50]

; =============================================================================
;  Single scene
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]

; --- 1-2 opening dialogue lines (replace with your own) -------------------
[ch name="Narrator" text="Your story begins here."]
[p]

; --- Placeholder: this is where your opening scene goes -------------------
[ch name="Narrator" text="(Write your protagonist's first lines here, then make the story yours.)"]
[p]

[wait time=500]
[end]
