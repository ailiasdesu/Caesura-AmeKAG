; =============================================================================
;  Golden VN v2 — real save -> load roundtrip scene.
;
;  Location matters: the native [load] resume reloads the recorded scene via
;  flow.load_scene on the path the save captured, and SaveCommands._safeScenePath
;  allowlists scripts/ | assets/script/ | assets/scripts/ | demo/ | tests/scripts/
;  (+ .ks, no "..").  tests/projects/ is NOT allowlisted — a save taken from
;  tests/projects/golden_vn/story.ks cannot resume.  This scene lives under
;  tests/scripts/ (the repo's own test-scene convention, e.g. save_test.ks,
;  integration_test.ks) so the save -> load -> resume chain runs for real.
;
;  Driver: the load is issued through SaveCommands.load(ctx, {slot=9}) — the
;  EXACT handler the [load] tag dispatches — because a same-scene [load] TAG
;  re-enters the saved resume point, re-executes the downstream [load] token
;  and loops on the native runner (the cursor+1 self-reference guard exists
;  only in the web bridge, web/bridge.js:650).  The engine defect is reported
;  (t52 constraint: no engine-side fix here); the driver asserts the RESTORE
;  synchronously after load() — f.rtMarker returns to PRE_SAVE — then drives
;  the resume-to-save replay to [end].
;
;  Semantics under test:
;    forward: PRE_SAVE/1  ->  [save slot=9]  ->  POST_SAVE_MUTATED/2  ->  ready
;    load():  f.rtMarker back to PRE_SAVE, f.rtCounter back to 1
;             (no forward path re-writes PRE_SAVE after the save point)
;    replay:  resumes at the save point, re-saves, re-mutates, reaches [end]
;
;  ks_check gate: zero contract warnings.
; =============================================================================
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
