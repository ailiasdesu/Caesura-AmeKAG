# Caesura (AmeKAG) macOS Metal 已知问题

> 适配日期: 2026-06-07
> 测试环境: macOS 26 + Apple Silicon (arm64) + Metal 3
> 上游 commit: f7ee618

---

## 已通过

| 项目 | 状态 |
|------|------|
| cmake 配置 & 构建 | ✅ |
| C++ 单元测试 110/110 | ✅ |
| Metal 渲染器初始化 | ✅ |
| 嵌入式 Metal 着色器 (10个) | ✅ 全部 READY |
| RTT 创建 | ✅ |
| fillViewport | ✅ |
| 音频 (CoreAudio) | ✅ |
| Lua 脚本加载 & 运行 | ✅ |
| Esc / 窗口关闭退出 | ✅ |

---

## Bug #1 🔴: draw_viewport 导致 segfault

**症状:** 调用 `backend.draw_viewport()` 后第一个 `bgfx::frame()` 时崩溃。

**定位:** `BgfxRenderDevice::blitTexture()` → `bgfx::submit()` 路径。

**复现:**
```lua
local vp = backend.create_viewport(200, 100)
backend.fill_viewport(vp, 200, 40, 40, 255)  -- OK
backend.draw_viewport(vp, 50, 50, 200, 100)   -- SEGFAULT on next frame
```

**分析:**
- `fillViewport` 使用与 `blitTexture` 相同的 `m_fallbackProgram`，但 fill 正常而 blit 崩溃
- 差异: fill 将 RTT 设为当前 framebuffer 后提交，blit 直接提交到 VIEW_MAIN
- 可能原因: VIEW_MAIN 的 framebuffer 是默认 backbuffer，与 RTT framebuffer 的 Metal 纹理格式或生命周期管理有差异

---

## Bug #2 🟡: bgfx debug text 叠字

**症状:** `bgfx::dbgTextPrintf()` 输出的 ASCII 文字在 Metal 上重叠渲染，字母堆在一起无法辨认。

**原因:** bgfx 的 debug font 内置 8x16 位图字体在 Metal 后端渲染时，字符 quad 位置计算与 Metal NDC 坐标系不匹配。

**绕过:** 改用 FreeType TextRenderer（见 Bug #3）或终端输出。

---

## Bug #3 🟡: TextRenderer::renderText 崩溃

**症状:** 引擎自带的 FreeType 文字渲染器调用 `renderText()` 后立即 segfault。

**定位:** `TextRenderer::submitGlyphQuads()` → `bgfx::submit()` 路径。

**分析:**
- TextRenderer 初始化成功（日志: `[TextRenderer] Initialized. Font: 8x16, atlas: 32 cols`）
- 崩溃发生在提交 glyph quad 到 GPU 时
- 可能原因: Metal 上 bgfx 的 TransientVertexBuffer 或纹理采样器句柄与 OpenGL/D3D 行为不同

---

## 适配修改摘要

为让项目在 macOS 上编译通过，对上游代码做了以下修改（`MACOS_PORT_LOG.md` 有完整 diff）：

| 文件 | 修改 | 原因 |
|------|------|------|
| `src/Carc/CryptoEngine.cpp` | Windows BCrypt → 增加 OpenSSL 回退 | macOS 无 BCrypt |
| `src/Resource/DirAssetProvider.cpp` | `GetFileAttributesA` → `stat()` | POSIX 文件 API |
| `CMakeLists.txt` | bcrypt 条件链接, OpenSSL, SoLoud CoreAudio, SDL3 dylib | 跨平台构建 |
| `src/Render/BgfxRenderDevice.cpp` | 增加 Metal 着色器路径 (enum 值修正) | bgfx v11 RendererType 枚举差异 |
| `src/Render/EmbeddedShaders_Metal.cpp` | **新文件** — Metal 着色器源码嵌入 | bgfx Metal 后端需要 MSL 源码 |
| `shaders/embed_metal.sh` | **新文件** — 着色器嵌入脚本 | 辅助工具 |
| `src/main.cpp` | 增加 Esc 处理, `_exit(0)` | 退出流程 |
| `src/Core/Engine.cpp` | 增加 Esc/SDL_EVENT_QUIT 处理 | 窗口关闭和退出 |

---

## 建议上游修复方向

1. **draw_viewport 崩溃 (最高优先级)** — 在 Metal 后端测试 `blitViewport`/`blitTexture` 路径，可能与 bgfx framebuffer 绑定或 Metal texture 格式有关
2. **bgfx 内置 debug font** — 检查 bgfx Metal 后端的 debug text 字符坐标计算
3. **TextRenderer** — 测试 FreeType 字体纹理在 Metal 上的上传和采样
