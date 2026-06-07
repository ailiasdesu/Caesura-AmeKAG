; =============================================================================
;  smoke_test.ks — Minimal engine smoke test (15 tags)
; =============================================================================

*start
[bg file="assets/bg/school.jpg" time=500]
[fg file="assets/fg/hero.png" layer=1]
[play_bgm file="assets/bgm/title.ogg" loop=true]
[wait time=1000]

; Text display
Hello, this is a smoke test.

; Clear and transition
[stop_bgm time=500]
[fg clear=true layer=1]
[transition type=fade time=300]

; Voice test
[play_voice file="assets/voice/line001.ogg"]
[wait time=2000]

[end]
