# Caesura (AmeKAG)

> 跨平台视觉小说引擎 · Cross-platform Visual Novel Engine

## 技术栈

| 层级 | 库 | 说明 |
|------|-----|------|
| 窗口/输入 | SDL3 | 跨平台窗口、多点触控、鼠标/键盘 |
| 图形渲染 | bgfx | 全平台 GPU 抽象（Vulkan/DX12/Metal/WebGPU） |
| 音频引擎 | SoLoud | 三总线架构（BGM/VOICE/SE）+ 3D 空间音频 |
| 脚本语言 | Lua 5.4 | KAG 演出命令库 + 协程调度 |

## 架构

```
.ks 剧本 → tokenizer.lua → scheduler.lua → kag.lua → C++ 后端（bgfx/SoLoud/SDL3）
```

## 构建

```bash
cmake -B build -S .
cmake --build build
```

## 运行

```bash
./build/CaesuraAmeKAG
```

## 开源协议

MIT License
