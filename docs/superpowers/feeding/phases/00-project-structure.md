## 项目文件结构

```
Caesura(AmeKAG)/
├── CMakeLists.txt                    # 根构建文件
├── cmake/                            # CMake 模块
│   ├── Findbgfx.cmake
│   ├── FindSoLoud.cmake
│   └── FindSDL3.cmake
├── src/                              # C++ 引擎核心
│   ├── main.cpp                      # 入口点
│   ├── Engine.h/cpp                  # 引擎主类 (初始化+主循环)
│   ├── render/                       # 渲染子系统
│   │   ├── RenderBackend.h/cpp       # bgfx 初始化和帧管理
│   │   ├── LayerManager.h/cpp        # 图层管理器 (bg/fg/message + RTT)
│   │   ├── TextureManager.h/cpp      # 纹理加载/缓存/生命周期
│   │   ├── FontRenderer.h/cpp        # FreeType 字体渲染 (TextureArray)
│   │   ├── TextLayoutEngine.h/cpp    # 文本布局 (换行/对齐/ruby/CJK)
│   │   ├── ShaderCache.h/cpp         # 着色器缓存 (CompositeShaderCache)
│   │   ├── TransitionManager.h/cpp   # 转场效果 (Rule Image + easing)
│   │   └── GpuMonitor.h/cpp          # GPU 帧时间监控 + 自适应降级
│   ├── audio/                        # 音频子系统
│   │   ├── AudioBackend.h/cpp        # SoLoud 初始化 + 全局 Direct 模式
│   │   ├── AudioManager.h/cpp        # BGM/Voice/SE Bus 管理
│   │   └── AudioCallback.h/cpp       # 音频结束回调 (SDL_PushEvent)
│   ├── script/                       # 脚本引擎
│   │   ├── LuaEngine.h/cpp           # Lua 5.4 嵌入 (lua_State + 绑定)
│   │   ├── KagExecutor.h/cpp         # KAG 标签执行器 (kag[cmd] -> C++)
│   │   └── GameState.h/cpp           # GameState 单例 (ctx 管理)
│   ├── resource/                     # 资源管理
│   │   ├── IAssetProvider.h           # 资源提供者抽象接口
│   │   ├── ProviderChain.h/cpp        # 责任链: CARC > patch > dir
│   │   ├── DirAssetProvider.h/cpp     # 目录资源提供者
│   │   ├── AsyncLoader.h/cpp          # 异步加载 (后台线程+SDL_PushEvent)
│   │   └── ResourcePool.h/cpp         # 对象池 (RTT/SE Bus/render_batch)
│   ├── carc/                         # CARC 容器
│   │   ├── CARCReader.h/cpp          # CARC 读取器 (验证+解密+解压)
│   │   ├── CARCWriter.h/cpp          # CARC 构建器 (car_pack.exe 使用)
│   │   ├── CryptoEngine.h/cpp        # AES-256-GCM + Ed25519
│   │   └── CarcAssetProvider.h/cpp   # CARC 资源提供者 (IAssetProvider)
│   ├── system/                       # 系统设施
│   │   ├── Logger.h/cpp              # 日志系统 (10MB 轮转)
│   │   ├── Config.h/cpp              # 配置管理 (JSON)
│   │   ├── SaveManager.h/cpp         # 存档管理 (JSON 序列化 + 缩略图)
│   │   ├── I18nManager.h/cpp         # 国际化 (_T + 翻译表)
│   │   └── ErrorUI.h/cpp             # 错误界面 (硬编码几何 + 级联保护)
│   ├── input/                        # 输入系统
│   │   └── InputManager.h/cpp        # SDL 事件 → Lua 事件分发
│   └── debug/                        # 调试工具
│       ├── DebugProtocol.h/cpp       # lua_sethook 断点/单步/变量
│       ├── HotReload.h/cpp           # 文件监控 + 协程热重载
│       └── DevSocket.h/cpp           # Dev socket 9229 结构化日志
├── scripts/                          # Lua 脚本层
│   ├── init.lua                      # Lua 初始化入口
│   ├── kag/                          # KAG 命令实现
│   │   ├── init.lua                  # KAG 命令注册表
│   │   ├── tokenizer.lua             # LPeg .ks → Token 序列
│   │   ├── scheduler.lua             # 协程调度器 (遍历 tokens)
│   │   ├── flow.lua                  # 流程控制 (jump/call/return/link)
│   │   └── commands/                 # 各标签实现
│   │       ├── text.lua              # [ch]/[text]/[p]/[r]/[er]
│   │       ├── layer.lua             # [bg]/[fg]/[cl]/[image]
│   │       ├── audio.lua             # [playbgm]/[playse]/[playvoice]
│   │       ├── transition.lua        # [trans]/[move]/[quake]/[fade]
│   │       ├── control.lua           # [if]/[else]/[endif]/[switch]/[case]
│   │       ├── system.lua            # [eval]/[emb]/[wait]/[save]/[load]
│   │       └── video.lua             # [video]/[stopvideo]
│   ├── layers.lua                    # 图层渲染管理
│   ├── audio.lua                     # 音频管理 (BGM/Voice/SE Bus)
│   ├── backend.lua                   # C++ API 绑定代理 (fallback 可选)
│   └── dev/                          # 开发工具 (仅 Dev 模式)
│       └── hotreload.lua             # 脚本热重载
├── shaders/                          # bgfx 着色器
│   ├── vs_default.sc                 # 默认顶点着色器
│   ├── fs_default.sc                 # 默认片段着色器
│   ├── fs_blend/                     # 28 种混合模式着色器变体
│   ├── fs_palette.sc                 # 调色板 LUT 着色器
│   └── fs_transition/                # 转场着色器
├── assets/                           # 引擎内置资源
│   ├── fonts/                        # 内置字体
│   │   └── bitmap_font.bin           # 8x16 位图字体 (ASCII 32-126)
│   └── textures/
│       └── checkerboard.png          # 紫黑棋盘格占位纹理
├── tests/                            # 测试
│   ├── unit/                         # C++ 单元测试 (Catch2)
│   ├── lua/                          # Lua 单元测试 (busted)
│   └── integration/                  # 集成测试
├── tools/                            # 构建工具
│   └── carc_pack/                    # CARC 打包工具
│       └── main.cpp
├── examples/                         # 示例项目
│   ├── hello_world/                  # 最简示例
│   └── demo_game/                    # 完整功能演示
├── docs/                             # 文档
│   ├── Caesura_功能实现规格说明书_整合版.md
│   └── superpowers/
│       └── plans/
│           └── 2026-06-06-caesura-implementation.md  # 本文件
└── thirdparty/                       # 第三方依赖 (git submodule 或 fetch)
    ├── bgfx/
    ├── bx/
    ├── bimg/
    ├── soloud/
    ├── SDL/
    ├── lua/
    ├── lpeg/
    ├── freetype/
    ├── pl_mpeg/
    ├── zstd/
    └── openssl/ (或 mbedtls)
```

---
