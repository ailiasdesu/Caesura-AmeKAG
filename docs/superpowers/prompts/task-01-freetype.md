# Task 1: stb_truetype → FreeType 迁移

## 目标
将 FontRenderer 从 stb_truetype 单头文件库迁移到 FreeType 2，提升 CJK 文字渲染质量（auto-hinting）。

## 上下文
- 引擎: Caesura (AmeKAG) — C++17 + Lua 5.4 + bgfx + SDL3
- 说明书: `docs/Caesura_功能实现规格说明书_整合版.md` Part 5.2 规定 FreeType 为字体后端
- CONTEXT_ANCHOR.md §4 技术栈: Font = FreeType
- 当前代码使用 stb_truetype.h，CMake 已链接 freetype 但从未调用 FT_* API
- FreeType 源码在 `external/freetype/`，已通过 `add_subdirectory` 构建

## 涉及文件

### 必须修改
| 文件 | 修改类型 |
|------|----------|
| `src/Render/FontRenderer.h` | 更新注释 + 添加 FT_Library/FT_Face 前向声明 |
| `src/Render/FontRenderer.cpp` | 完全重写 FreeType API 调用 |

### 应删除/清理
| 文件 | 操作 |
|------|------|
| `src/Render/stb_impl.cpp` | 删除（仅含 `#define STB_TRUETYPE_IMPLEMENTATION`） |

### 不应修改
- `src/Render/TextRenderer.cpp` — 它通过 FontRenderer 间接使用字体，不直接调用 stb API
- `CMakeLists.txt` — freetype 已正确链接，stb include 路径需保留（stb_image 仍在使用）
- `external/stb/` 目录 — stb_image.h / stb_image_write.h 仍被其他地方使用

## API 映射表

```
stb_truetype                          → FreeType 2
═══════════════════════════════════════════════════════
stbtt_fontinfo                        → FT_Face
stbtt_InitFont(&font, data, 0)        → FT_New_Memory_Face(library, data, size, 0, &face)
stbtt_ScaleForPixelHeight(&font, px)  → FT_Set_Pixel_Sizes(face, 0, px)
stbtt_GetFontVMetrics                 → face->ascender/descender/height (26.6 fixed)
stbtt_GetCodepointBitmap              → FT_Load_Char + face->glyph->bitmap
stbtt_FreeBitmap(bitmap, nullptr)     → 不需要（FreeType 自管位图内存）
stbtt_GetCodepointHMetrics(&font, cp, &adv, &lsb)
  → adv = face->glyph->advance.x >> 6  (26.6 → px)
  → lsb = face->glyph->metrics.horiBearingX >> 6
```

## 实现大纲

### FontRenderer.h 变更
1. 移除注释中的 "stb_truetype" → 改为 "FreeType"
2. 添加前向声明（或直接 include）:
```cpp
#include <ft2build.h>
#include FT_FREETYPE_H
```
3. 成员变量变更:
```cpp
// 删除:
std::vector<uint8_t> m_fontData;   // Raw TTF bytes
// 新增:
FT_Library m_ftLibrary = nullptr;
FT_Face    m_ftFace    = nullptr;
```
4. 接口保持不变（getGlyph / renderText / renderTextCached 签名不变）

### FontRenderer.cpp 变更
**init() 重写:**
1. `FT_Init_FreeType(&m_ftLibrary)` 初始化 FreeType
2. 读字体文件到 `std::vector<uint8_t> fontData`（本地变量）
3. `FT_New_Memory_Face(m_ftLibrary, fontData.data(), (FT_Long)fontData.size(), 0, &m_ftFace)`
4. `FT_Set_Pixel_Sizes(m_ftFace, 0, (FT_UInt)fontSize)`
5. 度量: `m_ascender = m_ftFace->size->metrics.ascender / 64.0f` (26.6 → float)
   `m_descender = m_ftFace->size->metrics.descender / 64.0f`
   `m_lineHeight = m_ftFace->size->metrics.height / 64.0f`
6. 创建 atlas 纹理（不变）
7. 预填充 ASCII 32-126（不变）

**rasterizeGlyph() 重写:**
1. `FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace, (FT_ULong)cp)`
2. `FT_Load_Glyph(m_ftFace, glyphIndex, FT_LOAD_DEFAULT)`
3. `FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL)`
4. 从 `m_ftFace->glyph->bitmap` 读取:
   - `bitmap.width`, `bitmap.rows`, `bitmap.buffer`
   - `bitmap_left`, `bitmap_top` (等同 stb 的 xoff, yoff)
5. 提前量: `m_ftFace->glyph->advance.x >> 6`
6. atlas 打包逻辑不变（用 bitmap.width/rows 替代 stb 的 w/h）

**shutdown():**
1. `FT_Done_Face(m_ftFace)` 
2. `FT_Done_FreeType(m_ftLibrary)`
3. 其余清理不变

**getGlyph() / renderText() / renderTextCached():**
- 签名和逻辑不变，只改内部调用的 API

### stb_impl.cpp
- 直接删除文件
- 从 CMakeLists.txt 的 SOURCES 列表中移除 `src/Render/stb_impl.cpp` 这一行

## 验收标准
1. ✅ CMake 构建成功（MSVC，Debug 和 Release）
2. ✅ FreeType 初始化成功，日志输出 `[FontRenderer] Initialized: xxx.ttf`
3. ✅ ASCII 32-126 预填充完成
4. ✅ CJK 字符（如 "你好世界"）能正确光栅化
5. ✅ `m_ftLibrary` / `m_ftFace` 在 shutdown 时正确释放
6. ✅ 没有 stb_truetype 残留（rg "stb_truetype" src/Render/FontRenderer.* 返回空）

## 注意事项
- FreeType 的 `face->glyph->bitmap.buffer` 是 FreeType 内部管理的内存，**不要 free**
- FreeType 的度量值单位是 26.6 定点数（`value / 64.0f` = 像素值）
- 空 glyph（如空格）: `FT_Load_Char` 成功但 `bitmap.width == 0`，仍需记录 metrics
- 必须添加 `#include <ft2build.h>` 和 `#include FT_FREETYPE_H`（注意 FT_FREETYPE_H 是宏）
