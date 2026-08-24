# Android Latest HEAD Real-Device Regression Report

- **Target Commit**: `62132e783dd238752659d4227ff26b0235258ea9`
- **Test Target**: Xiaomi 11 (`alioth` / Snapdragon 888 / Adreno 660 / Android 14)
- **Runner**: `scripts/verify_android_regression.py`
- **Result**: **88 Passed, 0 Failed out of 88 checks (100% PASS)**

## Verified Categories
1. **Boot, Manifest & Host Configuration**: `singleInstance`, orientation lock, OpenGL ES feature.
2. **Rendering CJK RGBA8 Atlas**: 2048x2048 RGBA8 FreeType atlas with 8,074 preloaded glyphs.
3. **Multi-Texture Batching**: Transient vertex/index buffer per `MergeGroup`, fresh `bgfx::setState`.
4. **RTT Namespace Separation**: TextureManager handles decoupled from Viewport RTT handles.
5. **Touch & Gestures**: `event.tfinger` normalized coordinate scaling, `GestureDetector` pinch & long press.
6. **Storage**: Quick save (`slot=-1`), auto save (`slot=-2`), base64 thumbnails, slots `-2..99`.
7. **Lifecycle & Audio**: OpenSLES backend, 3-bus audio mixer, resume after sleep.
8. **IME Virtual Keyboard Bridge**: `startTextInput`, `stopTextInput`, `setTextInputRect`, upper viewport clamping.
9. **Release Signing & Packaging**: PKCS12 keystore generation, V1/V2/V3 signatures, disabled bundle splits.
10. **First-VN Parity**: Complete E2E walkthrough on ARM64 device.
