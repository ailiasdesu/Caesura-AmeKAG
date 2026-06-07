# Caesura (AmeKAG) — 项目上下文记忆

> 自动压缩恢复用。每次重大变更后更新。
> 最后更新: 2026-06-07

---

## 身份

- **项目名:** Caesura (AmeKAG) — 跨平台视觉小说引擎
- **仓库:** git@github.com:ailiasdesu/Caesura-AmeKAG.git (master)
- **本地路径:** D:/文件存放处/codex/Caesura(AmeKAG)
- **许可:** MIT

## 技术栈

| 层级 | 库 | 用途 |
|------|-----|------|
| 窗口/输入 | SDL3 | 跨平台窗口、多点触控 |
| 渲染 | bgfx (Direct3D 11) | GPU 抽象层 |
| 音频 | SoLoud | 三总线 (BGM/Voice/SE*8) |
| 脚本 | Lua 5.4 + LPeg | KAG .ks 解析 + 协程调度 |
| 字体 | FreeType | TTF 光栅化 + CJK 回退 |
| 视频 | pl_mpeg | MPEG1 (Alpha) |
| 加密 | Windows BCrypt (CNG) | AES-256-GCM + Ed25519 |
| 压缩 | zstd | CARC 容器压缩 |
| 构建 | CMake >= 3.25 + MSVC 17.12 | C++17 |

## 架构速查

.ks 剧本 -> tokenizer.lua (LPeg) -> scheduler.lua (协程) -> kag.lua -> C++ Binding -> bgfx/SDL3/SoLoud

- **单线程模型:** 全部核心在主线程运行，CAESURA_ASSERT_MAIN_THREAD() 守护
- **服务定位器:** BackendRegistry 单例持有原始指针，Engine 拥有 unique_ptr 实例
- **9 个 src/ 模块:** Core -> Render/Audio -> Scripting; Carc/Resource/Platform 独立岛
- **禁止依赖:** Audio->Render, Render->Scripting, Carc->Core, Resource->Core/Render/Audio

## 当前状态

| 指标 | 值 |
|------|-----|
| P0 决策 | 13/13 (100%) |
| P1 决策 | 30/30 (100%) |
| 引擎测试 | 40/40 |
| Debug 构建 | OK |
| Release 构建 | OK |

Alpha 已知问题 (非阻塞):
1. Shader 64KB 上限未实现
2. Tokenizer 部分 token 丢失 (82/167)
3. AsyncLoader API 未绑定 Lua backend
4. ShaderCache 未预注册全部 10 种 blend
5. FFmpeg 未集成 (P2)

## 目录结构

src/                C++ 引擎 (单 CMake 目标 CaesuraAmeKAG)
  Core/             Engine, BackendRegistry, DebugManager
  Render/           bgfx, LayerManager, ShaderCache, Font, Particles, RTT
  Audio/            SoLoudAudioEngine, LRU WaveCache
  Scripting/        Lua bindings (KAG/Render/VFX/Debug/DevCore/Save)
  Carc/             CARC 容器: Crypto, CRL, Reader/Writer
  Resource/         IAssetProvider, ProviderChain, AsyncLoader
  System/           SaveManager, SaveBinding
  Debug/            DebugProtocol, HotReload
  Platform/         iOS/Android 适配 (P2 占位)
scripts/            Lua 运行时 (kag/, dev/, config.lua, layers.lua...)
shaders/            GPU 着色器 (dx11/, glsl/, metal/)
tests/              测试 (Lua 脚本 + CARC 测试数据)
tools/carc_pack/    CARC 打包工具 (独立 CMake 目标)
external/           第三方库
docs/               规格说明书 + 计划文档

## 关键文件索引

| 文件 | 用途 |
|------|------|
| Caesura_功能实现规格说明书_整合版.md | 478 行, 68 项决策 |
| CONCEPTS.md | 62 行, 领域词汇表 |
| ALPHA_NOTES.md | 80 行, Alpha 发布说明 |
| BUILD.md | 118 行, 构建指南 |
| README.md | 24 行, 项目简介 |
| docs/Caesura_Alpha补完开发计划_20260607.md | Alpha 补完记录 |
| docs/solutions/architecture-patterns/engine-architecture-reference.md | 架构参考 |

## 构建命令

cmake -B build -S .
cmake --build build --config Debug
cmake --build build --config Release

## 核心约定

- **文件命名:** PascalCase (.cpp/.h 同名同目录)
- **命名空间:** Caesura / caesura / caesura::carc
- **接口前缀:** I (IRenderDevice, IAudioBackend, IAssetProvider, IPlatformBackend)
- **Binding 模式:** Scripting/*Binding.cpp/h -> UnifiedBinding::registerAll(L)
- **Shader 管线:** HLSL -> DXBC -> C 数组嵌入 -> initEmbeddedShaders()
- **平台隔离:** #ifdef CAESURA_PLATFORM_WINDOWS/MACOS/LINUX
- **分支前缀:** codex/

## 最近提交

0886e6e docs: CONCEPTS.md + engine architecture reference
ca7b87f 结构优化
8f656ea 清理
64c0b18 Alpha 补完: P0 13/13, P1 30/30
5bfc871 docs: 说明书更新
fa4d8ca fix: ErrorUI garbled text
bb02343 G9 complete
6f6e1f1 G11-P1: 8 项 P1 补全
13ff36b G11: Alpha bug fixes
25eb6ac G10: Alpha RC — 40/40 tests
