; =============================================================================
;  Caesura (AmeKAG) — KAG3 Migration Template
;
;  A scene written the way a KAG3 migration expects: legacy spellings and
;  aliases on top of KAG Neo-Genesis semantics (see the KAG3 chapter in
;  docs/compatibility.md §2 and docs/guides/kag3-migration.md).
;
;  Demonstrated here:
;    - %f.name%  legacy %var% text interpolation (still supported)
;    - [elsif]   KAG3 spelling alias for [elseif]
;    - [goto]    strict alias for [jump]
;    - bare positional params ( [wait 500] , [jump *label] , [goto *label] )
;    - [set f.x = ...] old dotted assignment + $f.x / literal interpolation
;
;  Run:   lua tools/project_templates/kag3/entry.lua
;  Check: lua scripts/ks_check.lua tools/project_templates/kag3/story.ks
; =============================================================================

[font face="default" size=22]
[pt speed=50]

; =============================================================================
;  Legacy variable setup (KAG3 dotted assignment)
; =============================================================================
*start
[cl]
[bg storage="assets/bg/classroom.png"]
[wait 500]                       ; bare positional time

[set var="f.name" value="Aoi"]
[set var="f.hp" value=30]
[set var="f.route" value=1]

[ch name="Narrator" text="%f.name% greets you. （%f.name% 向你问好。）"]
[p]
[ch name="Narrator" text="Her HP is %f.hp% — and the modern form says $f.hp."]
[p]

; =============================================================================
;  Branch — [elseif] chain (KAG3 spelling [elsif] also accepted at runtime)
; =============================================================================
*route_check
[if exp="f.route == 1"]
[ch name="Narrator" text="Route one: the forest path. （路线一：森林。）"]
[elseif exp="f.route == 2"]
[ch name="Narrator" text="Route two: the city lights. （路线二：城市。）"]
[elseif exp="f.route == 3"]
[ch name="Narrator" text="Route three: the shore. （路线三：海边。）"]
[else]
[ch name="Narrator" text="Unknown route. （未知路线。）"]
[endif]
; NOTE: the KAG3 spelling [elsif] is an alias for [elseif] and is accepted
; at runtime, but ks_check's offset parser does not yet normalize it — use
; [elseif] in templates and migrate legacy [elsif] (docs/compatibility.md §2.1).
[p]

; =============================================================================
;  Legacy navigation: [goto] is a strict alias for [jump]; both accept bare
;  positional targets.
; =============================================================================
[goto *legacy_hop]               ; bare positional + alias for [jump *legacy_hop]

*legacy_hop
[ch name="Narrator" text="You have been redirected by [goto], an alias for [jump]."]
[p]
[ch name="Narrator" text="KAG3 bare positional style also works: [wait 500] , [jump *credits]."]
[p]

; --- ending
[jump *credits]                  ; bare positional target

*credits
[cl]
[bg storage="assets/bg/hana.png"]
[scroll text="KAG3 MIGRATION TEMPLATE" size=40]
[scroll text="Built with the Caesura (AmeKAG) engine" speed=70]
[p]
[ch name="Narrator" text="— Migration scene complete / 迁移场景完成 —"]
[p]
[wait 800]
[end]