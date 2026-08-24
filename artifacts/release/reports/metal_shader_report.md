# iOS Metal Shader & Fallback Verification Report

- **Target Commit**: `62132e783dd238752659d4227ff26b0235258ea9`
- **Audit Tool**: `scripts/verify_metal_shaders.py`
- **Result**: **12/12 Metal Shaders & 2/2 Fallback Pathways Verified (PASS)**

## Verified Shader Assets
1. `kEmbeddedMetal_vs_sprite` (vertex, 608 bytes) — Sprite quad vertex transform
2. `kEmbeddedMetal_vs_fullscreen` (vertex, 659 bytes) — Fullscreen postfx quad vertex
3. `kEmbeddedMetal_stretch_blt_vs` (vertex, 630 bytes) — Viewport stretch blit vertex
4. `kEmbeddedMetal_affine_blt_vs` (vertex, 995 bytes) — Affine transform blit vertex
5. `kEmbeddedMetal_fs_texture` (fragment, 586 bytes) — Direct texture sampling
6. `kEmbeddedMetal_fs_blend` (fragment, 9925 bytes) — Multi-mode texture blending (16 blend modes)
7. `kEmbeddedMetal_fs_transition` (fragment, 2324 bytes) — Universal transition rule blending
8. `kEmbeddedMetal_fs_vfx` (fragment, 2004 bytes) — Dissolve, ripple, and noise effects
9. `kEmbeddedMetal_stretch_blt_fs` (fragment, 753 bytes) — Bilinear stretched blit fragment
10. `kEmbeddedMetal_affine_blt_fs` (fragment, 586 bytes) — Affine transform fragment
11. `kEmbeddedMSL_MiniGame_VS` (MSL vertex) — 3D MiniGame mesh vertex transformation
12. `kEmbeddedMSL_MiniGame_FS` (MSL fragment) — 3D MiniGame lighting and surface shading

## Verified Graceful Fallbacks
- **Post-FX Shaders**: Uncompiled vignette/lut/blur/bloom shaders automatically alias to `fsTexture` (identity blit) to avoid GPU pipeline stall.
- **SMA 3D Mesh Skinning**: If `BGFX_CAPS_COMPUTE` is not present, `SmaMeshRenderer` automatically falls back to CPU thread pool soft-skinning (`SmaSkinner`).
