; =============================================================================
;  save_test.ks — Save/Load round-trip test (12 tags)
; =============================================================================

*start
[bg file="assets/bg/room.jpg"]
[fg file="assets/fg/npc.png" layer=1]
[play_bgm file="assets/bgm/room.ogg" loop=true]

; Save state at this point
[save slot=1]

; Change state
[bg file="assets/bg/park.jpg" time=300]
[play_bgm file="assets/bgm/park.ogg"]

; Load back
[load slot=1]

; Verify recovery — bg should be back to room
[wait time=500]
[end]
