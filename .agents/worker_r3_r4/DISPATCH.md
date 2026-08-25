## 2026-08-25T00:58:02Z

You are Worker R3/R4 for the Caesura (AmeKAG) Web Player PWA and Creator Tools Polish Milestone.
Working Directory: d:\文件存放处\code\Caesura(AmeKAG)\.agents\worker_r3_r4

Inputs:
- ORIGINAL_REQUEST: d:\文件存放处\code\Caesura(AmeKAG)\.agents\ORIGINAL_REQUEST.md
- AGENTS.md: d:\文件存放处\code\Caesura(AmeKAG)\AGENTS.md
- Explorer 3 Handoff: d:\文件存放处\code\Caesura(AmeKAG)\.agents\explorer_survey_3\handoff.md

Your Exclusive Write Boundaries:
- web/ (sw.js, manifest.webmanifest, index.html, main.mjs, vite.config.js, dom-renderer.js)
- assets/icon-192.png, assets/icon-512.png
- scripts/package_game.sh
- demo/example_game/ (story.ks and associated assets if needed)
- tools/project_templates/

Your Task:
1. Implement Pillar R3 (Web Player PWA & Mobile Web Offline Experience):
   - Generate valid PWA icons (assets/icon-192.png, assets/icon-512.png) with Caesura engine branding / visual icon.
   - Update web/sw.js to implement Cache-First static caching for offline gameplay, including web-assets/glue.wasm, JS bundle, runtime data, and manifest.
   - Update web/vite.config.js and scripts/package_game.sh so sw.js and manifest.webmanifest are properly bundled and copied into web/dist/ and dist/<game>/.
   - Update web/index.html and web/main.mjs:
     - Add programmatic orientation lock helper: screen.orientation?.lock('landscape').catch(()=>{}) on user gesture.
     - Add CSS orientation overlay prompt when device is in portrait mode (@media (orientation: portrait)).
     - Ensure audio autoplay unlock works seamlessly.
2. Implement Pillar R4 (Creator Tools & Sample Game Polish):
   - Enhance demo/example_game/story.ks with:
     - Declarative tweening: [tween target=" Mio\ attr=\x\ from=1280 to=480 dur=600 ease=ease_out]
 - Post-processing VFX: [vfx postfx=\vignette\ amount=0.4] in attic scene, [vfx postfx=\bloom\ strength=0.8] in thunder lightning scene, [vfx postfx=\none\] to clear.
 - UI style presets: [textbox] and [nameplate] styling.
 - Ensure all scripts in demo/ and tools/project_templates/ pass scripts/ks_check.lua with 0 syntax or contract errors.
 - Run verification scripts:
 - bash scripts/verify_sample_game.sh (5/5 PASS)
 - bash scripts/verify_template.sh (4/4 PASS)
 - bash scripts/verify_golden_vn.sh (PASS)
 - cd web && npm test (unit/audio unlock tests PASS)
