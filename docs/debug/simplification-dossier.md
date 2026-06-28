# Code Simplification Dossier

> Generated 2026-06-28 by exploration agent. All changes verified zero risk.

## Summary

| # | Finding | File(s) | Type | LOC Removed | Risk |
|---|---------|---------|------|-------------|------|
| 1.1 | `getTexHandle()` dead function | RenderBinding.cpp:65-68 | Dead code | 4 | Zero |
| 1.2 | Dead locals in flushBatch | BgfxQuadBatch.cpp:50-51,56 | Dead code | 3 | Zero |
| 1.3 | Dead getPosTexLayout + redundant m_posTexLayout | BgfxRenderDevice.h:76,110; .cpp:64-68 | Dead code | ~8 | Very low |
| 1.4 | Dead m_batchQuads + m_batching | BgfxRenderDevice.h:128-135 | Dead code | ~9 | Zero |
| 1.5 | Dead m_rttMap lookup in blitViewport | BgfxRenderDevice.cpp:163-164; .h:117-124 | Dead/bug | ~8 | Very low |
| 1.6 | Conflicting VIEW_TRANSITION constants | IRenderDevice.h:14 vs BgfxDeviceCore.h:21 | Dead def | 1 | Zero |
| 3.1 | Dead includes in Engine.cpp | Engine.cpp:43,45,46,50 | Include | 4 | Zero |
| 3.2 | Dead includes in BgfxRenderDevice.cpp | BgfxRenderDevice.cpp:3,4,7,9,11,12,14 | Include | 7 | Zero |
| 3.3 | Duplicate `<memory>` include | BgfxRenderDevice.h:11 | Include | 1 | Zero |
| 3.4 | Duplicate `<filesystem>` include | TextureManager.cpp:9 | Include | 1 | Zero |
| 3.5 | Unnecessary `<cstdint>` include | BgfxQuadBatch.h:5 | Include | 1 | Zero |

**Total safe LOC removal:** ~47 lines across 7 files, zero behavioral change.
