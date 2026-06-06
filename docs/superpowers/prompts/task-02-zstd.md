# Task 2: miniz → zstd 迁移

## 目标
将 CARC 容器和 XP3 解包工具从 miniz（zlib 兼容）迁移到 zstd，获得 3-5x 解压速度提升。

## 上下文
- 引擎: Caesura (AmeKAG) — C++17 + Lua 5.4 + bgfx + SDL3
- 说明书: `docs/Caesura_功能实现规格说明书_整合版.md` CARC-1 规定 zstd 为压缩后端
- CONTEXT_ANCHOR.md §2: "不使用 XP3 容器 | 自研 CARC (AES-256-GCM + Ed25519 + zstd)"
- 当前代码使用 miniz（从 bgfx/bimg/3rdparty/tinyexr/deps/miniz/ 构建）
- CMake 已链接 `libzstd_static`（`external/zstd/`）但从未调用 ZSTD_* API
- zstd 解压比 zlib/miniz 快 3-5x，压缩率高 10-20%

## 涉及文件

### 必须修改（改 API 调用）
| 文件 | miniz 调用数 | 操作 |
|------|-------------|------|
| `src/Carc/CARCReader.cpp` | 1 (`mz_uncompress`) | 替换 |
| `src/Carc/CARCWriter.cpp` | 2 (`mz_compressBound` + `mz_compress`) | 替换 |
| `src/Core/XP3Archive.cpp` | 6 (`mz_compressBound`×1, `mz_compress2`×1, `mz_uncompress`×4) | 替换 |

### 必须修改（移除依赖）
| 文件 | 操作 |
|------|------|
| `CMakeLists.txt` | 删除 miniz 库定义 + 链接 + include |

## API 映射表

```
miniz                                  → zstd
══════════════════════════════════════════════════════════
#include <miniz.h>                    → #include <zstd.h>
mz_ulong                              → size_t
mz_compressBound(size)                → ZSTD_compressBound(size)
mz_compress(dst, &dstLen, src, srcLen)→ ZSTD_compress(dst, dstLen, src, srcLen, level)
mz_compress2(dst, &dstLen, src, srcLen, level)
                                      → ZSTD_compress(dst, dstLen, src, srcLen, level)
mz_uncompress(dst, &dstLen, src, srcLen)
                                      → ZSTD_decompress(dst, dstLen, src, srcLen)
MZ_OK                                 → 返回值 >= 0 表示成功（无错误码宏）
```

**关键差异:**
- `mz_compress` 的 `dstLen` 是 in/out 参数（传入缓冲区大小，传出实际大小）
- `ZSTD_compress` 的 `dstLen` 是纯输入（传入缓冲区大小），返回值是实际压缩后大小
- `ZSTD_compress` 返回 `size_t`，成功时返回压缩后大小，失败返回 `ZSTD_isError(ret)`
- `ZSTD_decompress` 同理，成功返回解压后大小

## 逐文件实现

### CARCReader.cpp
**位置:** `readFileByHash()` 函数中的解压逻辑
```cpp
// 旧代码:
#include <miniz.h>
...
mz_ulong destLen = static_cast<mz_ulong>(info.originalSize);
std::vector<uint8_t> decompressed(info.originalSize);
int status = mz_uncompress(decompressed.data(), &destLen,
                           compressed.data(), static_cast<mz_ulong>(compressed.size()));
if (status != MZ_OK) { ... }

// 新代码:
#include <zstd.h>
...
size_t destLen = info.originalSize;
std::vector<uint8_t> decompressed(destLen);
size_t result = ZSTD_decompress(decompressed.data(), destLen,
                                compressed.data(), compressed.size());
if (ZSTD_isError(result)) { ... }
// result 是解压后实际大小
```

### CARCWriter.cpp
**位置:** `finalize()` 函数中的压缩逻辑
```cpp
// 旧代码:
#include <miniz.h>
...
mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(pf.originalSize));
std::vector<uint8_t> compressed(bound);
mz_ulong actual = bound;
int status = mz_compress(compressed.data(), &actual,
                          pf.data.data(), static_cast<mz_ulong>(pf.originalSize));
if (status != MZ_OK) { ... }

// 新代码:
#include <zstd.h>
...
size_t bound = ZSTD_compressBound(pf.originalSize);
std::vector<uint8_t> compressed(bound);
size_t actual = ZSTD_compress(compressed.data(), bound,
                               pf.data.data(), pf.originalSize,
                               ZSTD_CLEVEL_DEFAULT);  // 默认压缩级别
if (ZSTD_isError(actual)) {
    // 压缩失败: 回退到不压缩
    compressed.assign(pf.data.begin(), pf.data.end());
    actual = pf.originalSize;
}
```

注意: `ZSTD_CLEVEL_DEFAULT` 是 3。CARC 场景可用 `6`（平衡速度/压缩率）或 `ZSTD_CLEVEL_DEFAULT`。

### XP3Archive.cpp
**文件中有 3 个函数需要改:**

**1. ZlibCompress() 辅助函数:**
```cpp
// 旧:
#include <miniz.h>
static std::vector<uint8_t> ZlibCompress(const uint8_t* data, size_t size, int level = 6) {
    mz_ulong dstLen = mz_compressBound((mz_ulong)size);
    std::vector<uint8_t> out(dstLen);
    if (mz_compress2(out.data(), &dstLen, data, (mz_ulong)size, level) != MZ_OK)
        return {};
    out.resize(dstLen);
    return out;
}

// 新:
#include <zstd.h>
static std::vector<uint8_t> ZlibCompress(const uint8_t* data, size_t size, int level = 6) {
    size_t dstLen = ZSTD_compressBound(size);
    std::vector<uint8_t> out(dstLen);
    size_t result = ZSTD_compress(out.data(), dstLen, data, size, level);
    if (ZSTD_isError(result)) return {};
    out.resize(result);
    return out;
}
```

**2. ZlibDecompress() 辅助函数:**
```cpp
// 旧:
static std::vector<uint8_t> ZlibDecompress(const uint8_t* data, size_t size, size_t expectedOut) {
    mz_ulong dstLen = (mz_ulong)expectedOut;
    std::vector<uint8_t> out(dstLen);
    if (mz_uncompress(out.data(), &dstLen, data, (mz_ulong)size) != MZ_OK)
        return {};
    out.resize(dstLen);
    return out;
}

// 新:
static std::vector<uint8_t> ZlibDecompress(const uint8_t* data, size_t size, size_t expectedOut) {
    std::vector<uint8_t> out(expectedOut);
    size_t result = ZSTD_decompress(out.data(), expectedOut, data, size);
    if (ZSTD_isError(result)) return {};
    out.resize(result);
    return out;
}
```

**3. unpack() 函数中:** 直接调用 ZlibDecompress，无需额外修改（辅助函数已改）
**4. list() 函数中:** 同上

### CMakeLists.txt 变更
删除以下 3 处:
1. 库定义（找 `add_library(miniz STATIC` 开头的整个块，约 5 行）
2. include 路径中的 miniz 行:
   `${CMAKE_SOURCE_DIR}/external/bgfx/bimg/3rdparty/tinyexr/deps/miniz`
3. 链接行中的 `miniz`:
   从 `... soloud lua miniz ed25519 ...` 改为 `... soloud lua ed25519 ...`

## 验收标准
1. ✅ CMake 构建成功（MSVC，Debug 和 Release），无 miniz 链接警告
2. ✅ 用 rg 确认 src/ 目录下无 `#include <miniz.h>` 残留
3. ✅ 用 rg 确认 src/ 目录下无 `mz_compress` / `mz_uncompress` 残留
4. ✅ 用 rg 确认 src/ 目录下有 `#include <zstd.h>` 和 `ZSTD_compress` / `ZSTD_decompress`
5. ✅ CARC 打包→解包循环测试: 创建含文本+PNG 的 CARC，再读回，数据完全一致

## 注意事项
- zstd 使用**裸压缩**（非 zlib wrapper），解压时不需要窗口大小等参数
- `ZSTD_isError(result)` 是检查失败的宏，需要 `#include <zstd_errors.h>` 或直接用返回值比较
- XP3 是第三方格式（Kirikiri 引擎的容器），其 index 段可能是 zlib 压缩。切换到 zstd 后：
  - **解包（unpack）**: 不变（XP3 格式本身用 zlib，仍需 miniz/zlib 解压）—— **等等，这有问题！**
  
  **⚠️ 重要：XP3Archive.cpp 是 XP3 解包工具，用于读取 Kirikiri 引擎的 .xp3 文件。XP3 格式规范使用 zlib 压缩，切换 zstd 后无法解压 XP3 文件！**

  修正方案: **XP3Archive.cpp 保留 miniz**。只有 CARC（我们的自研格式）切换到 zstd。
  
  所以实际修改范围缩小为:
  - ✅ `src/Carc/CARCReader.cpp` → zstd
  - ✅ `src/Carc/CARCWriter.cpp` → zstd
  - ❌ `src/Core/XP3Archive.cpp` → **保留 miniz**（XP3 格式兼容性要求）
  - ✅ `CMakeLists.txt` → 保留 miniz 库（XP3Archive 仍需）

## 修正后的涉及文件

| 文件 | 操作 |
|------|------|
| `src/Carc/CARCReader.cpp` | `#include <zstd.h>`, `mz_uncompress` → `ZSTD_decompress` |
| `src/Carc/CARCWriter.cpp` | `#include <zstd.h>`, `mz_compressBound`/`mz_compress` → `ZSTD_*` |
| `CMakeLists.txt` | **保留 miniz**（XP3 需要） |

## 修正后验收标准
1. ✅ CMake 构建成功
2. ✅ CARCReader/CARCWriter 使用 zstd（rg 确认）
3. ✅ XP3Archive 仍使用 miniz（rg 确认）
4. ✅ miniz 和 zstd 同时链接，不冲突
5. ✅ CARC 打包→解包循环测试通过
