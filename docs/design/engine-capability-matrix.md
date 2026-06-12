# Caesura (AmeKAG) 引擎能力矩阵

> 生成日期: 2026-06-12 | 测试: 295/295 | 接口: 26 | 模块: 16

## 渲染能力

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| 2D 纹理渲染 (PNG/JPG/TGA/BMP) | ✅ 完成 | `TextureManager` + stb_image | 最大 4096×4096 |
| 图层合成 (BG/FG/MSG 3 层) | ✅ 完成 | `LayerManager` | Z 序：背景→前景→消息框 |
| 粒子系统 (最大 1024 粒子) | ✅ 完成 | `ParticleSystem` | 多线程并行更新 |
| 文字渲染 (FreeType) | ✅ 完成 | `TextRenderer` + `FreeTypeContext` | TTF/OTF, Ruby 注音 |
| 视频播放 (MPEG1) | ✅ 完成 | `VideoPlayer` + pl_mpeg | FFmpeg 可选 |
| 过渡效果 (淡入/溶解/滑动/缩放) | ✅ 完成 | `BgfxDraw::submitTransition` | 8 种过渡 |
| 后处理 VFX (抖动/模糊/闪烁) | ✅ 完成 | `BgfxDraw::submitVFX` | 6 种效果 |
| Shader 缓存 (64 槽 LRU) | ✅ 完成 | `CompositeShaderCache` | 多后端 D3D11/OpenGL/Metal |
| RTT (渲染到纹理) | ✅ 完成 | `RTTManager` | 屏幕截图/后处理 |
| 多后端 (D3D11/OpenGL/Metal) | ✅ 完成 | bgfx | Windows/Linux/macOS |
| GPU 性能监控/降级 | ✅ 完成 | `GpuMonitor` + `NullGpuMonitor` | 3 级画质 |

## 音频能力

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| BGM 播放/停止/淡入淡出 | ✅ 完成 | `SoLoudAudioEngine` | WAV/FLAC/MP3/OGG |
| 音效播放 (SE) | ✅ 完成 | `SoLoudAudioEngine` | 多音效同时 |
| 语音播放 (Voice) | ✅ 完成 | `SoLoudAudioEngine` | 独立总线 |
| 全局音量控制 | ✅ 完成 | `SoLoudAudioEngine` | 0.0 ~ 1.0 |
| 总线独立音量 | ✅ 完成 | BGM/Voice/SE 三总线 | 持久化 |
| 静音 Fallback | ✅ 完成 | `NullAudioBackend` | headless/CI |

## 脚本与叙事

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| KAG 脚本解析 | ✅ 完成 | `tokenizer.lua` + `parser.lua` | ~68 命令 |
| 文本显示/换行/暂停 | ✅ 完成 | `@text`, `@l`, `@p`, `@er` | 逐字/逐行 |
| 角色对话 (名字+文本) | ✅ 完成 | `@ch` (character) | 字体/颜色/大小可切换 |
| 背景切换 | ✅ 完成 | `@bg` | 支持渐变过渡 |
| 前景/立绘控制 | ✅ 完成 | `@fg`, `@position`, `@layopt` | 位置/缩放/透明度 |
| 选项分支 | ✅ 完成 | `@choice`, `@button` | 多选项 |
| 条件分支/跳转 | ✅ 完成 | `@if`, `@jump`, `@call`, `@return` | 标签系统 |
| Ruby 注音 | ✅ 完成 | `@ruby` | 汉字注音 |
| BGM/SE 控制 | ✅ 完成 | `@playbgm`, `@stopbgm`, `@playse` | 脚本控制 |
| 视频播放 | ✅ 完成 | `@video`, `@stopvideo` | MPEG1 |
| 等待/延时 | ✅ 完成 | `@wait` | 毫秒级 |
| 粒子 VFX | ✅ 完成 | `@vfx` | 脚本触发粒子 |
| 存档/读档 | ✅ 完成 | `@save`, `@load`, `@listsaves` | JSON 格式 |
| 过渡效果 | ✅ 完成 | `@trans`, `@move`, `@quake`, `@fade` | 8 种效果 |
| 预加载资源 | ✅ 完成 | `@preload` | 异步 |
| 宏定义 | ✅ 完成 | `@macro`, `@endmacro` | 脚本复用 |
| Lua API (Render/VFX/Debug 等) | ✅ 完成 | 6 个模块, 40+ API | 脚本中调用引擎 |
| 热重载 (开发期) | ✅ 完成 | `HotReload` | 保存即刷新 |

## 资源管理

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| 图片加载 (PNG/JPG/TGA/BMP) | ✅ 完成 | `ImageDecoder` + stb_image | 8 格式 |
| 异步资源加载 | ✅ 完成 | `AsyncLoader` + `JobSystem` | 后台线程解码 |
| CARC 归档读取 | ✅ 完成 | `CARCReader` + `CarcAssetProvider` | AES + zstd |
| CARC 归档创建 | ✅ 完成 | `CARCWriter` | 加密+压缩+签名 |
| 资源配额 (内存预算) | ✅ 完成 | `SandboxQuota` | 纹理/音频/RTT 限制 |
| 纹理预算 (6 级自适应) | ✅ 完成 | `TextureBudget` | 128MB ~ 4GB |

## 存档与持久化

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| JSON 存档 (多 slot) | ✅ 完成 | `SaveManager` | schema 版本化 |
| 存档加密 (AES-256-GCM) | ✅ 完成 | `SaveManager` + `CryptoEngine` | 可选 |
| 存档列表/删除 | ✅ 完成 | `SaveManager` | 多 slot 管理 |
| 存档迁移 (版本升级) | ✅ 完成 | `SaveManager::registerMigration` | 旧档自动升级 |
| 云存档 (接口) | ⚠️ 接口 | `ISaveProvider::pushToCloud` | Steam 云待实现 |
| 缩略图截图 | ⚠️ 存根 | `SaveManager::captureThumbnailPNG` | RTT 读回待实现 |

## Live2D 动画

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| Cubism SDK 集成 | ⚠️ 条件编译 | `Live2DBackend` | 需 SDK license |
| PNG 静态立绘降级 | ✅ 完成 | `NullAnimationBackend` | 无 SDK 时可用 |
| 模型加载/卸载 | ✅ 完成 | `loadModel` / `unloadModel` | |
| 表情切换 | ⚠️ SDK only | `setExpression` | |
| 动作播放 | ⚠️ SDK only | `playMotion` | |
| 渲染路径 (D3D11/Metal/OpenGL) | ⚠️ SDK only | 4 个 RenderPath | |

## 3D 小游戏

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| 场景加载 (GLB) | ⚠️ 存根 | `BgfxMiniGameBackend::loadScene` | no-op |
| 几何体渲染 | ⚠️ 存根 | `MiniGeometry` | 结构定义就位 |
| 碰撞检测 | ⚠️ 存根 | `MiniCollision` | AABB 检测 |
| 材质/光照 | ⚠️ 存根 | `MiniMaterial`, `MiniLight` | 结构定义就位 |
| MiniGame Shader | ✅ 完成 | `EmbeddedShaders_MiniGame` | D3D11/GLSL/Metal |
| Null 降级 | ✅ 完成 | `NullMiniGameBackend` | 所有方法安全 |

## Steam 集成

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| SDK 初始化/销毁 | ⚠️ 条件编译 | `SteamBackend` | 需 Steamworks SDK |
| 成就解锁 | ⚠️ 条件编译 | `SteamBackend::unlockAchievement` | |
| 统计数据 | ⚠️ 条件编译 | `SteamBackend::setStat` | |
| 用户信息 | ⚠️ 条件编译 | `SteamBackend::getUserName` | |
| Null 降级 | ✅ 完成 | `NullSteamBackend` | 所有方法返回 false |

## 编辑器与工具链

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| Web 编辑器前端 | ✅ 完成 | `web-editor/` (React + Vite) | |
| 实时脚本编辑/执行 | ✅ 完成 | `EditorServer` + `CodeEditor.jsx` | |
| 场景预览 (StageView) | ✅ 完成 | `StageView.jsx` | |
| Live2D 模型列表/加载 | ✅ 完成 | `Live2DPanel.jsx` | 动态 API |
| 一键打包 (CARC) | ✅ 完成 | `AssetPanel.jsx` + `/api/build` | |
| 资产面板 | ✅ 完成 | `AssetPanel.jsx` | |
| CLI CARC 打包工具 | ✅ 完成 | `tools/carc_pack` | 独立命令行 |

## 调试与诊断

| 能力 | 状态 | 实现 | 备注 |
|------|------|------|------|
| 分级日志 (6 级) | ✅ 完成 | `DebugManager` | Trace→Fatal |
| 日志文件输出 | ✅ 完成 | `DebugManager` | 时间戳+子系统 |
| 环形缓冲区 (1024 条) | ✅ 完成 | `DebugManager` | 内存缓存 |
| 性能帧采样 | ✅ 完成 | `FrameProfile` + `PROFILE_SCOPE` | |
| GPU 时间/CPU 时间 | ✅ 完成 | `GpuMonitor` | |
| 子系统统计 | ✅ 完成 | `DebugManager::getSubsystemStats` | |
| 脚本断点调试 | ✅ 完成 | `DebugProtocol` | |
| Lua 指令预算 | ✅ 完成 | `LuaManager` | 防止死循环 |
| 内存分配 Hook | ✅ 完成 | `lua_Alloc` hook | SandboxQuota |

## 跨平台

| 平台 | 构建 | 测试 | CI |
|------|------|------|----|
| Windows (MSVC) | ✅ 绿 | ✅ 295/295 | ⚠️ 运行中 |
| macOS (Clang) | ⚠️ 本地未测 | ⚠️ 本地未测 | ⚠️ 运行中 |
| Linux (GCC) | ⚠️ 本地未测 | ⚠️ 本地未测 | ⚠️ 运行中 |

## 图例

| 符号 | 含义 |
|------|------|
| ✅ 完成 | 已实现，测试覆盖，可生产使用 |
| ⚠️ 部分 | 实现但不完整，或依赖外部条件 |
| ❌ 未实现 | 未开始 |

## 统计

| 类别 | 完成 | 部分 | 未实现 | 完成率 |
|------|------|------|--------|--------|
| 渲染 | 11 | 0 | 0 | 100% |
| 音频 | 6 | 0 | 0 | 100% |
| 脚本叙事 | 18 | 0 | 0 | 100% |
| 资源管理 | 6 | 0 | 0 | 100% |
| 存档持久化 | 4 | 2 | 0 | 67% |
| Live2D | 2 | 4 | 0 | 33% |
| 3D 小游戏 | 1 | 4 | 0 | 20% |
| Steam | 1 | 4 | 0 | 20% |
| 编辑工具 | 7 | 0 | 0 | 100% |
| 调试诊断 | 9 | 0 | 0 | 100% |
| **总计** | **65** | **14** | **0** | **82%** |
