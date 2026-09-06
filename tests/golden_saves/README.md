# Save fixture provenance

`golden_save_v1.json` through `golden_save_v5.json` are synthetic envelopes.
They exercise the real C++ migration path, but no historical producer binary or
capture log accompanied their initial addition. Do not describe them as saves
captured from older releases.

`release-v1.0.1-kag-save.json` was produced by the actual Windows v1.0.1 release
executable, its bundled Lua runner and native SaveManager. The release ZIP's
SHA-256 matches the digest published for GitHub asset 523231008. Its tag is
associated with commit `9aa299d4b78e5a62e25786d8771539bdc1adb65d`; this is not an
independent attestation of the binary's build inputs.

The matching provenance JSON records ZIP, executable, input, scene and save
digests, the process ID and successful exit. The producer-input JSON contains
the exact scene and RPC requests. The source scene is retained under
`tests/projects/u11_legacy_release/`. The first unsuccessful preparation attempt
and successful capture logs remain in `artifacts/validation/u11-native/`.

This real release sample has **envelope schema 5 and Lua data schema 2**. Its
embedded engine-version string is `1.0.0`, as written by that binary. It proves
compatibility with an actual older KAG producer; it is not an envelope-v1
migration sample. A supplied thumbnail avoided GPU capture, so the fixture does
not prove rendering or audio behavior.
