---
module: render
tags: [bgfx, shader, glsl, binary-format]
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

## 坑：repack 时误 append 新 shaderSize

第一次 repack 把头部 `data[:off]`（**已含原 shaderSize 字段**）与新
`shaderSize` 一起写出，导致**头部字段保持旧值 + code 前多 4 字节**。
bgfx 按旧 shaderSize 解析 → GLSL 源码截断/含垃圾字节 → 编译失败
（诊断特征：解析 shaderSize 得到超大值或 `0x74657874 = 'text'`）。

## 正确做法

```python
codeSize_off = off  # shaderSize 字段位置
out = bytearray(data[:codeSize_off])     # 头部（不含 shaderSize）
out += new_size.to_bytes(4, 'little')    # 替换而非追加
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
