; =============================================================================
;  smoke_test.ks — Minimal engine smoke test (15 tags)
; =============================================================================

*start
[bg file="assets/bg/school.jpg" time=500]
[fg file="assets/fg/hero.png" layer=1]
[playbgm file="assets/bgm/title.ogg" loop=true]
[wait time=1000]

; Text display
Hello, this is a smoke test.

; Clear and transition
[stopbgm time=500]
[fg clear=true layer=1]
[trans type=fade time=300]

; Voice test
[playvoice file="assets/voice/line001.ogg"]
[wait time=2000]

[end]
