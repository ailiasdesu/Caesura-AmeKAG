---
module: render
tags: [bgfx, shader, glsl, binary-format, hash-pairing]
problem_type: build-errors
---

# bgfx shader 二进制（VSH11/FSH11）手工 repack 的正确做法

## 背景

引擎的 GL 内嵌 shader（`EmbeddedShaders_GL.cpp`）是 `VSH11`/`FSH11`
格式的**文本 GLSL**（shaderc 输出）。为在 GL 4.3 core 上下文编译，
需要在 GLSL 源码前插入 `#version 430 core`。

## 二进制布局（version 11）

```
magic      u32   'VSH'+11 / 'FSH'+11
hashIn     u32
hashOut    u32   (ver >= 6)
count      u16   uniform 数量
[count × uniform]  nameSize u8 + name + type u8 + num u8 + regIndex u16
                   + regCount u16 + texInfo u16 (ver>=8) + texFormat u16 (ver>=10)
shaderSize u32
code       shaderSize 字节（GLSL 文本）
pad        u8
numAttrs   u8
attrs      2 × numAttrs u16
extra      u16
```

## 坑 1：repack 时误 append 新 shaderSize

第一次 repack 把头部 `data[:off]`（**已含原 shaderSize 字段**）与新
`shaderSize` 一起写出，导致**头部字段保持旧值 + code 前多 4 字节**。
bgfx 按旧 shaderSize 解析 → GLSL 源码截断/含垃圾字节 → 编译失败
（诊断特征：解析 shaderSize 得到超大值或 `0x74657874 = 'text'`）。

## 坑 2：fsh.hashIn 必须等于配对 vsh.hashOut（bgfx program 创建的配对检查）

**症状**：字节校验器（`isDirectFeedBinary` 同款 walk）**通过**、单
shader 创建成功，但 `bgfx::createProgram` 仍返回
`BGFX_INVALID_HANDLE`——引擎侧表现为
`[Renderer] XXX: program build failed.`（t79 实测：只补 uniform 记录
后 GL 跑出 "VFX: program build failed"，补 hashIn 后才 10/10 READY）。

**根因**：bgfx 在创建 program 时校验
`vsr.m_hashOut == fsr.m_hashIn`（bgfx_p.h:5140，
"Vertex shader output doesn't match fragment shader input."；失败时
`BX_TRACE` 不可见，直接返回 INVALID）。fsh 的 hashIn 编码其输入
varying 定义；配对 vsh 的 hashOut 编码其输出 varying 定义——同一套
`varying.def` 编译产物必然同值。陈旧数组若 hashIn=0（或任意异值），
vsh/fsh 配对检查即失败。

**修法**：repack 时把 fsh 的 `hashIn`（字节 4..7）同步写为配对 vsh 的
`hashOut`（字节 8..11）。GL 引擎侧全部 fullscreen FS
（fs_texture/fs_blend/fs_transition/fs_vfx/fs_postfx_*）以
`kEmbeddedGL_vs_fullscreen`（hashOut=0x3C3E1E6F）为配对 vsh；
VS 的 hashOut 取值以实际数组为准，勿照抄常量。

```python
out[4:8] = vs_data[8:12]   # fsh.hashIn = 配对 vsh.hashOut
```

## 正确做法

```python
codeSize_off = off  # shaderSize 字段位置
out = bytearray(data[:codeSize_off])     # 头部（不含 shaderSize）
out += new_size.to_bytes(4, 'little')    # 替换而非追加
# 坑 2：同步修复 fsh.hashIn = 配对 vsh.hashOut
out[4:8] = vs_data[8:12]
out += new_code
out += tail
```

## 验证

每次 repack 后用 bgfx 同款解析逻辑重放验证：

```python
off = 12  # magic(4) + hashIn(4) + hashOut(4)
count = int.from_bytes(data[off:off+2], 'little'); off += 2
for _ in range(count):
    ns = data[off]; off += 1
    off += ns + 1 + 1 + 4 + 2 + 2   # name + type + num + reg + texInfo + texFormat
sz = int.from_bytes(data[off:off+4], 'little'); off += 4
assert data[off:off+sz].startswith(b"#version 430 core\n")
```

注意：解析 uniform 表时**每个 uniform 固定 10 字节尾部**
（regIndex 2 + regCount 2 + texInfo 2 + texFormat 2），与类型无关。

## 校验清单

1. **字节 walk 重放**：shaderSize 域回读正确、code 段以 `#` 开头、
   code 尾 NUL 终止（见上"验证"，亦等于引擎
   `BgfxShaderManager::isDirectFeedBinary` 语义）；
2. **hash 配对核验（坑 2 专属）**：对每个与 VS 配对的 FS，
   `fsh.hashIn == vsh.hashOut`：
   ```python
   assert int.from_bytes(fs_data[4:8], 'little') == \
          int.from_bytes(vs_data[8:12], 'little')   # bgfx_p.h:5140
   ```
3. **实机断言**：`--backend opengl --frames 60` 日志中对应
   `program READY` 且无 `program build failed` / `rendering disabled
   (BGFX_DEBUG_IFH)` 标记；D3D11 同命令回归无 `[RENDER][ERROR]`。

## 实测案例：kEmbeddedGL_fs_vfx 两字段陈旧（t79 闭环）

2026-08 实测（GL --backend opengl 红绿）：fs_vfx 数组的 GLSL code 段
本身正确（从 3dab5fb6 同代 shaderc 输出、已含 `#version 430 core`），
但头部有两个陈旧字段：

1. **uniform 记录缺 texInfo/texFormat 对**（版本 >= 8/>=10 应有的 2+2 字节）：
   逐记录修复后 New-walk 解析即正确（诊断特征：Old-walk 可解析出
   count=2 [s_texture, VFXParams]、New-walk codeSize 错位读成
   `0x63646F62` / `0x74657874` 等文本值）；
2. **hashIn = 0**（详见坑 2）：vs_fullscreen 的 hashOut 为
   `0x3C3E1E6F`，fs_vfx 必须同值。只修字段 1 时表现为
   "VFX: program build failed"（createProgram 返回 BGFX_INVALID_HANDLE），
   字段 1+2 都修复后 GL 10/10 program READY。

可复用工具：`scripts/repack_gl_embed.py`（--array/--hashin/--hashin-from-vs/
--dry-run；自带引擎同款校验器；对已修复数组幂等（no-op）。已验证
reverse 还原旧布局再 repack 与 t79 结果逐字节一致）。

注意：修复后 code 段长度（1896 字节）原样保留；数组总长 1949→1957
（每条记录 +4 字节 ×2）。

## 备注：bgfx 内部 debugfont 覆盖层在 GL 下的编译告警（观察项）

GL 运行日志偶见 5 条 `[bgfx WARN] Failed to compile shader ... error
C7616: global variable gl_FragColor is removed after GLSL 3.30`（来源
bgfx 内部文本视频内存 blitter 的 debugfont shader，帧循环内首次使用
时触发，被引擎回调吞掉 + WARN，不崩溃、与业务程序无关）。观察项，
另案处理；排查时注意与业务数组（坑 1/坑 2）的症状区分——此告警
**不**伴随 `program build failed` 或 `[RENDER][ERROR]`。
