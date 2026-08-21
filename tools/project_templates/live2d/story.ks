; =============================================================================
;  Caesura (AmeKAG) — Live2D VN Template
;
;  A Live2D-oriented skeleton. The engine has NO dedicated [live2d] command;
;  Live2D models are loaded THROUGH the foreground layer ([fg]) and driven by
;  the animation backend. Without the Cubism SDK (CAESURA_LIVE2D=OFF), the
;  same [fg] call degrades gracefully to a static PNG placeholder — see
;  docs/guides/live2d-setup.md.
;
;  Model assets belong under assets/live2d/<name>/ (<name>.moc3 + model3.json
;  + textures/ + motions/ + expressions/). The @motion / @expression
;  directives below are COMMENTED OUT because they are KAG3-style directives,
;  not Neo-Genesis contract commands — uncomment them once Live2D is enabled
;  in your build (they are the model's "entry" hooks).
;
;  Run:   lua tools/project_templates/live2d/entry.lua
;  Check: lua scripts/ks_check.lua tools/project_templates/live2d/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=50]

; =============================================================================
;  Scene 1 — Loading the Live2D model
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=300]

[playbgm storage="assets/bgm/daily.wav" volume=0.5]
[ch name="Narrator" text="Live2D template. With the Cubism SDK this sprite breathes and moves."]
[p]

; --- Load the Live2D model onto the foreground layer -----------------------
; assets/live2d/character/character.model3.json
[fg storage="assets/live2d/character/character.model3.json"]

; --- Live2D model entry hooks (COMMENTED — KAG3 directives, not contract
;     commands; uncomment when Live2D is enabled in your build) -------------
; @motion name="idle"          ; play the idle motion loop
; @expression name="happy"     ; set a face expression
; @motion name="tap_body"      ; a tap reaction

[ch name="Narrator" text="Live2D 模型入口在此 / The Live2D model entry is here — a [fg] load above and @motion / @expression hooks below."]
[p]

; --- Swap comments for live directives to bring the character to life ------
[ch name="Narrator" text="Replace the commented @motion/@expression lines with the real ones and your character will speak, blink and bow on demand."]
[p]

[cl]
[wait time=500]
[end]