; Golden save/load continuation fixture. The storage used by
; golden_vn_headless.lua is a Lua value-copy fixture; native encrypted disk
; and independent-process coverage lives in tests/headless_save_restore.py.
; The load handler must commit a new session, restore PRE_SAVE/1 immediately,
; and continue to POST_SAVE_MUTATED/2 without re-executing the consumed save.
; The .ks entry is allowlisted and its actual token positions are validated.
*start
[set var="f.rtMarker" value="PRE_SAVE"]
[set var="f.rtCounter" value="1"]
[ch name="A" text="Roundtrip save point line"]
[save slot=9]
[set var="f.rtMarker" value="POST_SAVE_MUTATED"]
[set var="f.rtCounter" value="2"]
[set var="f.rtReady" value="1"]
[wait time=6000]
[end]
