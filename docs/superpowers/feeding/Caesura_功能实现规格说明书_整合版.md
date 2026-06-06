# Caesura (AmeKAG) 功能实现规格说明书

> 最后更新: 2026-06-06 (第三轮) | 决策总数: 61 | 状态: 可进入编码阶段

---

## 目录

- Part 0-10

---

# Part 0: 架构概览

引擎采用 C++ 硬件绑定 + Lua 全业务逻辑的分层架构。

### GameState 全局单例 ([10.2.31])

ctx 包含: co, tokens, token_index, call_stack, variables(f/sf/tf), layers, backlog, active_operations(CancelToken), stop_flag, input_focus。

### lua_State 单实例 ([10.2.37])

全局共享一个 lua_State*。协程通过 lua_newthread 创建。

### 性能基准 ([10.2.40])

Intel HD 620, 2C 2.0GHz, 4GB RAM, 60 FPS @ 1280x720。

### 线程安全 ([10.2.14])

主线程独占 bgfx/SoLoud API。C++ 断言。不引入锁。

### 内存预算 ([10.2.29])

Lua 256MB, 纹理 1GB, 音频 128MB(LRU 卸载)。对象池。显式 GC 点。

### 异步加载 ([10.2.32])

createTextureAsync(SDL_PushEvent 派发, 队列上限 256, 超限阻塞后台)+[preload] 标签(语法: [preload type="texture|audio" storage="path1,path2" wait="true|false"], wait=true 阻塞至加载完成, wait=false 后台预加载首次使用可能显示占位)+过渡槽(转场资源预加载队列, 由 [trans] 触发时后台异步加载下一个场景所需纹理/音频, 加载完成前显示占位纹理)。

---

### Lua<->C++ 统一抽象接口层 (BackendAPI)

**架构约束**: Lua 代码永远不直接调用 C 函数或 bgfx/SoLoud/SDL C API。
所有引擎能力通过单一 `backend` 表暴露，资源创建走工厂方法，所有句柄支持统一生命周期管理。

**C++ 侧: BackendAPI 统一注册器**

`BackendAPI::registerAll(L)` 在 Lua 全局创建 `backend` 表并注册全部子系统:

| 子系统 | 注册函数 | 暴露的 Lua API |
|--------|---------|----------------|
| 纹理 | registerTextureAPI | backend.load_image(path) -> TextureHandle |
| 音频 | registerAudioAPI | backend.audio_play(bus, path), backend.audio_stop(bus) |
| 图层 | registerLayerAPI | backend.layer_set_texture(layer, handle) |
| 字体 | registerFontAPI | backend.font_render(text, x, y, style) -> BatchHandle |
| 视频 | registerVideoAPI | backend.video_play(path) -> VideoHandle |
| 视口 | registerViewportAPI | backend.create_viewport(cfg) -> ViewportHandle |
| 工具 | registerUtilityAPI | backend.is_valid(handle) -> bool |
| 输入 | registerInputAPI | 事件分发 (只读) |
| 资源 | registerResourceAPI | backend.file_exists(path), backend.file_read(path) |

**Lua 侧: backend.lua 统一门面**

```
-- scripts/backend.lua
-- backend 表由 C++ BackendAPI::registerAll() 在初始化时注入全局

-- 工厂方法创建资源 (返回统一句柄)
local tex = backend.load_image("bg/school.png")    -- -> TextureHandle
local bgm = backend.audio_play("bgm", "theme.ogg") -- -> AudioHandle
local vp  = backend.create_viewport({width=400, height=300})

-- 统一句柄有效性校验 (id + generation 双重匹配)
if backend.is_valid(tex) then
    backend.layer_set_texture(0, tex)  -- bg 图层 (z=0)
end
```

**统一句柄 (ResourceHandle)**

所有 C++ 资源通过统一句柄暴露给 Lua，句柄为 `{type, id, generation}` 三元组:

| HandleType | 资源 | generation 递增时机 |
|------------|------|-------------------|
| TEXTURE | bgfx::TextureHandle | 热重载 / 纹理重新加载 |
| AUDIO | SoLoud handle | 音频重新加载 |
| VIEWPORT | bgfx::FrameBufferHandle | 视口重建 |
| RTT | bgfx::TextureHandle | RTT 池 resize |
| SHADER | bgfx::ProgramHandle | 着色器重新编译 |
| TRANSITION | 转场状态 | 转场重置 |
| FONT_ATLAS | TextureArray 层 | 图集扩容/重建 |

`backend.is_valid(handle)` 校验: handle.id != 0 且 handle.generation == C++ 侧当前 generation。
热重载后旧 generation 自动失效，防止悬空句柄访问。

**工厂模式**: 所有资源创建均通过 `backend.create_xxx()` 工厂方法，
Lua 侧不管理资源的裸指针或原生句柄。句柄 `__gc` 元方法通知 C++ 释放资源，
`__close` (Lua 5.4) 支持 `local tex <close> = backend.load_image(...)` 确定性释放。

---

# Part 1: 脚本引擎

## [1.1] tokenizer.lua — LPeg 解析 .ks -> Token 序列
## [1.2] scheduler.lua — 遍历 tokens, kag[cmd](ctx,params), yield 阻塞
### 协程关闭守卫 ([10.2.21]) — abort/resume 内 coroutine.close()
## [1.3] flow.lua — [jump]/[call]/[return]/[link](场景跳转时清空所有图层、清空 backlog、取消活跃 CancelToken)/[end]
### CancelToken ([10.2.33]) — 阻塞操作([wait]/[trans]/[move]/[quake]/playvoice 等)注册取消回调, [jump] 时遍历取消
## [1.4] flow.lua — [if]/[else]/[endif]/[switch]/[case]

---

# Part 2: 图形渲染

## [2.1] layers.lua — bg(0), fg(1), message(2) z-order
### 脏矩形 Scissor ([10.2.6]) — union >75% 退化全量
### 屏幕适配 — letterbox/stretch/crop
### Resize RTT ([10.2.13]) — onResize -> reset -> DevCore 通知 -> dirty
## [2.2] blend.lua + GLSL — 28 种混合模式 (详细公式清单见 Appendix D)
## [2.3] transition.lua + GLSL — Rule Image, progress+easing
### LUT 优化 ([10.2.25]) — 单纹理+uniform, 禁止每帧创建

**Rule Image**: 单通道 R8 纹理(或 RGB 取 R), 运行时拉伸到屏幕分辨率。progress 0->1 按 easing 采样。使用全局缓存纹理, 禁止每帧创建。

## [2.4] transform.lua + GLSL — 仿射变换, 4 种重采样
## [2.5] vfx.lua — Quake/Blur/Fade

---

# Part 3: 音频系统

## [3.1] audio.lua — BGM Bus(单流), Voice Bus(单流+打断), SE Bus 池(x8)
### 音频提交 ([10.2.11]) — 全局 Direct 模式, 所有操作立即执行
### SE per-handle ([10.2.27]) — setHandleVolume
## [3.2] 淡入淡出 — SoLoud fadeVolume/xfade
## [3.3] 音画同步 — playvoice 回调机制: 语音结束时由音频线程发出回调(SDL_PushEvent), 主线程唤醒 Lua 协程。禁止轮询实现

---

# Part 4: 系统设施

## [4.1] Backlog — {text, voice, timestamp}, 存 key 实时翻译
## [4.2] 存档 — JSON 序列化 ([10.2.5]), schema_version 迁移 ([10.2.34])
## [4.3] Config — text_speed/design/adapt/volume/thumbnail
### kag.lua 分期 ([10.2.38]) — Alpha ~20/Beta ~15/1.0 ~50
## [4.4] i18n — _T + 翻译表 + %{key} 插值
### 深度集成 ([10.2.35]) — 显示前自动翻译, Backlog 存 key

## [4.5] 存档缩略图 — 可配置分辨率/质量/编码参数 ([10.2.19])
## [4.6] 脚本热重载 — 分级安全范围, 资源恢复, 调试互斥状态机 ([10.2.4])

---
# Part 5: 视频与高级渲染

## [5.1] 视频播放 — pl_mpeg (MPEG1 Alpha), IVideoDecoder 接口预留
### PTS 同步 ([10.2.2]) — SoLoud::getAudioPosition -> 追赶/丢帧, 窗口 +/-1 帧
### 视频图层融合 ([10.2.8]) — 帧作为 bgfx::TextureHandle 绑定图层, 与其他图层一致的 quad 管线
### FFmpeg/H.264 集成(P1, Beta 起默认 ON) ([10.2.24]) — Beta: -DCAESURA_VIDEO_FFMPEG=ON, H.264/VP9/WebM

## [5.2] FreeType 矢量字体 — FontRenderer.h/.cpp
### 字体图集 TextureArray ([10.2.12]) — 预分配 4 层 2048x2048 RGBA8, shader: texture2DArray
### 动态扩容 ([10.2.23]) — 预测性静默扩容。80%时标记pending, 下个[trans]/[black]静默执行; 90%紧急触发+CJK静态图集静默降级(无遮罩)。预分配8层2048x2048 RGBA8(~128MB)。
### 文本布局引擎 ([10.2.7]) — 自动换行/对齐(H/V)/行间距/中英混排/ruby(Ruby 注解规范: 注音默认居中对齐基底字符上方, 多字符基底逐字对应; 换行时注音跟随基底字符移至下一行; 参考 HTML ruby 标准)注音
### 文本批处理 ([10.2.26]) — MessageLayerCache 缓存顶点缓冲, 单 submit, O(1) draw call

## [5.3] 实时调色板 — palette.lua + GLSL
### Blend+Palette 组合 ([10.2.9]) — CompositeShaderCache 单 Pass, Palette 先 Blend 后

---

# Part 6: 3D 小游戏扩展接口

## [6.1] 3D 图形接口 — rtt.lua
### 安全约束 ([10.2.3]) — 着色器上限 64KB, Lua 走 create_shader_from_file, Vertex 48B
### 生命周期 — Lua __gc -> destroyViewport, C++ shared_ptr
### RTT 类型区分 ([10.2.20]) — RTType enum, 2D/3D 独立池

## [6.2] 3D/2D 混合规则: 3D 场景需先渲染到 RTT, 将该 RTT 作为普通纹理绑定到图层(bg/fg), 设置混合模式。禁止 3D 视口与 2D 图层直接叠加渲染。

### 清除控制 ([10.2.15]) — create_viewport({clear_color, clear_depth, no_clear})
### RTT 池并发 ([10.2.22]) — 主线程独占(assert)+延迟销毁队列, 无锁, 零开销。后台线程通过SDL_PushEvent解耦。

---

# Part 7: 输入系统

## [7.1] 输入事件分发

| 输入类型 | SDL 事件 | Lua 分发 | 优先级 |
|---------|---------|--------|:----:|
| 鼠标左键 | SDL_EVENT_MOUSE_BUTTON_DOWN | on_click(x,y,button) | 1 |
| 鼠标移动 | SDL_EVENT_MOUSE_MOTION | on_mousemove(x,y) | 2 |
| 键盘 | SDL_EVENT_KEY_DOWN | on_key(keycode,mod) | 1 |
| 触控按下 | SDL_EVENT_FINGER_DOWN | 映射为鼠标点击 | 1 |
| 触控移动 | SDL_EVENT_FINGER_MOTION | 映射为鼠标移动 | 2 |
| 滚轮 | SDL_EVENT_MOUSE_WHEEL | on_scroll(dx,dy) | 3 |
| 窗口 resize | SDL_EVENT_WINDOW_RESIZED | Engine::onResize | 0 |

## [7.2] 输入焦点

- **KAG**: 点击推进 [p] 等待, 右键/ESC 菜单。转场动画中屏蔽点击, 完成后恢复。
- **GAME**: 事件分发给 CustomGame 回调, KAG 静默。
- **全局热键**: F5 快速存档 / F7 快速读档 / F12 调试, 不受焦点限制。

## [7.3] 触控手势 (移动平台 [10.2.64])

- 单指: 映射为鼠标左键。双指缩放: 保留接口(Alpha 不实现)。长按 >500ms: 右键菜单。

## [7.4] 输入屏蔽规则

[p]/[wait]/playvoice 等待期间, 点击触发 coroutine.resume([10.2.33])。[quake] 期间屏蔽所有点击([10.2.42])。[trans]/[move] 期间屏蔽点击推进, 允许右键/ESC。




# Part 8: 日志系统

## [8.1] 日志级别

| 级别 | 标识 | 用途 | Release |
|------|------|------|:---:|
| Debug | [DEBUG] | 每帧状态、内存 | 否 |
| Info | [INFO] | 子系统初始化 | 是 |
| Warn | [WARN] | 资源降级、性能警告 | 是 |
| Error | [ERROR] | 脚本崩溃、GPU 故障 | 是 |

## [8.2] 输出

- 控制台: 始终输出(Release Info+)。
- 文件: logs/caesura_YYYY-MM-DD_HH-MM-SS.log, 10MB 轮转(保留5个, 每次写入前检查日志大小, 超10MB触发轮转, 启动时不主动轮转)。
- Dev socket: 端口 9229 结构化日志。

## [8.3] 格式

```
[2026-06-06 08:00:00.123] [INFO] [Audio] SoLoud initialized.
[2026-06-06 08:00:00.456] [WARN] [Render] Texture pending: bg/01.png
[2026-06-06 08:00:00.789] [ERROR] [Script] kag.lua:488: nil value
```

## [8.4] 性能约束

高频操作(SE播放、render_batch)不输出日志。异步缓冲写入。




---

# Part 9: 开发工具接口

## [9.1] 调试协议 — lua_sethook 断点/单步/变量检查 ([10.2.18])
## [9.2] IAssetProvider — 资源虚拟文件系统抽象接口
### Provider Chain ([10.2.1]) — 责任链模式, 优先级: CARC(10) > patch(8) > dir(5)
## [9.3] 热重载工具 — 脚本变更监控, 状态恢复, 调试互斥 ([10.2.4])
# Part 10: CARC 现代化容器格式

## [CARC-1] 设计目标 — AES-256-GCM + Ed25519 + zstd + mmap
## [CARC-2] 文件布局 — Header(64B) + Content + Index(encrypted) + Signature(64B) + PublicKey(32B)
### 签名版本号 ([10.2.10]) — SHA-256(header||version||count||hashes), 防降级攻击

**Index 区块布局** (AES-256-GCM 加密存储):
```
uint32_t num_files;
struct FileEntry {
  uint8_t path_hash[32];    // SHA-256(relative_path)
  uint64_t offset;           // 文件数据在 Data 区块中的偏移
  uint64_t size;             // 原始文件大小
  uint8_t aes_key[32];      // 每文件独立 AES 密钥
  uint8_t nonce[12];        // AES-GCM nonce
  uint8_t tag[16];          // AES-GCM auth tag
} entries[num_files];
```
## [CARC-3] 密钥管理 — private.key(开发者), public.key(CARC 末尾)
### 公钥外置 ([10.2.28]) — 引擎启动时读取 CARC 末尾公钥, 无需重编译

## [CARC-4] 加密流程

1. 文件打包生成 Content 区块和 Index 元数据
2. 随机生成 256-bit AES 密钥 + 96-bit nonce
3. AES-256-GCM 加密 Content 区块
4. Index 区块包含: 文件路径列表 + 偏移量 + AES 密钥(用接收方公钥加密) + GCM Auth Tag
5. Index 区块同样经 AES-256-GCM 加密(密钥派生自 CARC 主密钥)

## [CARC-5] 签名验证流程

1. Ed25519 签名覆盖: Header(64B) + Encrypted Content + Encrypted Index(不含 Signature 和 PublicKey 字段)
2. 签名写入 CARC 末尾 64B(Signature)
3. 验证流程: (1) 读取公钥 (2) 计算 SHA-512 哈希 (3) Ed25519 验签 (4) 验签失败 → 触发错误界面([10.2.50])
4. 签名包含版本号([10.2.10]), 防降级重放攻击
5. 链式信任([10.2.63]) 链式信任模型: 子 CARC 需授权证书, 主密钥验证证书后信任子公钥, 含 CRL 吊销机制

## [CARC-6] IAssetProvider 抽象接口 + Provider Chain ([10.2.1])
## [CARC-7] Lua 字节码预编译
## [CARC-8] 差分更新 — bsdiff + 原子回滚 ([10.2.16])
## [CARC-9] 构建集成 — car_pack.exe

---

# Part 11: 设计决策闭环 (64 项)


---



---

## [10.1] 决策索引

| # | 决策 | 优先级 | 领域 |
|---|------|:-----:|------|
| 1 | Provider Chain 责任链模式 | P0 | CARC/资源 |
| 2 | 视频 PTS 音视频同步 | P0 | 视频 |
| 3 | 3D 着色器安全约束 | P0 | 3D/安全 |
| 4 | 热重载分级安全范围 | P1 | 开发 |
| 5 | JSON 序列化存档安全 | P0 | 存档 |
| 6 | 脏矩形 Scissor 优化 | P1 | 渲染 |
| 7 | 文本布局引擎 | P0 | 字体 |
| 8 | 视频图层融合机制 | P1 | 视频 |
| 9 | Blend+Palette 组合着色器 | P1 | 渲染 |
| 10 | CARC 签名版本号防降级 | P0 | CARC/安全 |
| 11 | 音频混合提交模型 | P0 | 音频 |
| 12 | FreeType TextureArray 图集 | P1 | 字体 |
| 13 | 窗口 Resize RTT 重分配 | P1 | 渲染 |
| 14 | 音频线程安全(主线程独占) | P1 | 音频 |
| 15 | 3D 视口清除控制 | P2 | 3D |
| 16 | CARC 差分原子回滚 | P2 | CARC |
| 17 | 视频解码器扩展接口 | P2 | 视频 |
| 18 | 热重载/调试互斥状态机 | P1 | 开发 |
| 19 | 存档缩略图可配置 | P2 | 存档 |
| 20 | RTT 2D/3D 类型区分池 | P1 | 3D |
| 21 | 协程显式关闭守卫 | P1 | 脚本 |
| 22 | RTT 池并发控制 | P2 | 渲染 |
| 23 | FreeType 纹理数组扩容策略 | P2 | 字体 |
| 24 | FFmpeg 视频解码器规划 | P2 | 视频 |
| 25 | LUT 纹理优化(单纹理+uniform) | P1 | 渲染 |
| 26 | 文本批处理(MessageLayerCache) | P1 | 字体 |
| 27 | SE per-handle 音量控制 | P1 | 音频 |
| 28 | CARC 公钥外置 | P0 | CARC/安全 |
| 29 | 内存预算与 GC 策略 | P1 | 系统 |
| 30 | 启动流程与错误恢复 | P1 | 系统 |
| 31 | GameState 全局单例 | P0 | 脚本 |
| 32 | 异步加载与预加载策略 | P1 | 资源 |
| 33 | CancelToken 取消机制+API | P0 | 脚本 |
| 34 | 存档版本管理与迁移 | P1 | 存档 |
| 35 | i18n 深度集成 | P2 | 系统 |
| 36 | Lua 5.4 新特性利用 | P2 | 脚本 |
| 37 | lua_State 单实例确认 | Confirmed | 脚本 |
| 38 | kag.lua 分期实现计划 | Confirmed | 脚本 |
| 39 | 错误界面零外部依赖 | Confirmed | 系统 |
| 40 | 性能基准(HD620 + OS) | Confirmed | 系统 |
| 41 | [move] 贝塞尔曲线数学定义 | P1 | 动画 |
| 42 | [quake] 点击交互规范 | P1 | 动画 |
| 43 | 跨版本存档迁移链 | P1 | 存档 |
| 44 | FreeType 图集扩容锁 | P2 | 字体 |
| 45 | (已合并至 [10.2.11] 全局 Direct) | — | 音频 |
| 46 | CARC 引擎自验证 | P1 | CARC/安全 |
| 47 | [eval]/[emb] Lua 沙箱 | P0 | 安全 |
| 48 | 热重载静态分析 | P2 | 开发 |
| 49 | 词汇表(Glossary) | P2 | 文档 |
| 50 | 全局异常捕获+错误界面 | P1 | 系统 |
| 51 | 沙箱 Dev/Release 开关 | P0 | 安全 |
| 52 | (已合并至 [10.2.33]) | — | — |
| 53 | 脏矩形 Scissor 触发机制 | P1 | 渲染 |
| 54 | FreeType 扩容字体降级+CJK | P1 | 字体 |
| 55 | RTT 池线程安全策略 | P2 | 渲染 |
| 56 | (已合并至 [10.2.11]) | — | — |
| 57 | 异步加载即时回退(棋盘格) | P1 | 资源 |
| 58 | 热重载句柄有效性检查 | P1 | 开发 |
| 59 | 缩略图自适应压缩流水线 | P2 | 存档 |
| 60 | Lua 内存硬上限(64K 检查) | P1 | 系统 |
| 61 | [link] 完整语义+CancelToken | P1 | 脚本 |
| 62 | 性能预算与自适应(手动三档+自动) | P1 | 渲染 |
| 63 | 多 CARC 链式信任(Alpha 单主 CARC, Beta 链式+CRL, 有效期可配置) | P0 | CARC/安全 |
| 64 | 移动平台适配(iOS/Android, onPause/onResume Lua钩子) | P2 | 平台 |

### 优先级分布

| 优先级 | 数量 | 说明 |
|:------:|:----:|------|
| P0 | 13 | 编码前必须解决 |
| P1 | 30 | Alpha 完整实现 |
| P2 | 14 | Beta 迭代 |
| Confirmed | 4 | 已确认配置 |
| **总计** | **61** | |

---

## [10.2] 逐项决策摘要
### [10.2.1] Provider Chain

**决策**: 责任链模式。CARC(10) > patch(8) > dir(5)。getSource() 返回来源。

### [10.2.2] 视频 PTS 同步

**决策**: SoLoud::getAudioPosition -> PTS_audio。追赶/丢帧。窗口 = |PTS_audio - PTS_video| <= (1 / video_fps)。若超出且音频领先则丢帧, 若视频领先则等待(不阻塞主线程, 重试)。 若音频流先于视频结束, 视频暂停在最后一帧。**用户手动跳过**: 鼠标点击或空格键触发 CancelToken 取消视频播放: 停止解码器, 释放视频图层纹理(归还 RTT 池), 立即继续脚本执行, 恢复 KAG 控制。CancelToken 回调在主线程执行, 等待解码线程退出(超时 500ms, 超时后强制终止)。
### [10.2.3] 3D 安全性

**决策**: 着色器模块 SPIR-V 字节码上限 64KB(压缩后, 不含调试符号)。Release 构建必须 strip 调试信息。Lua 加载时检查大小, 超限抛错。Lua 走 create_shader_from_file。Vertex 48B。调试构建启用着色器大小检查。__gc 元方法。

### [10.2.4] 热重载恢复

**热重载时正在运行的协程处理**: 终止所有活跃协程(先遍历所有活跃协程的 CancelToken:cancel()(等待音频回调 max config.hotreload_timeout_ms(默认 300ms), 超时强制终止), 再 coroutine.close()。C++ 侧 RTT shared_ptr 在 bgfx::frame() 后延迟销毁。持有外部资源的协程注册 cancel 回调或 __close 元方法), 从上一存档点或标题页重新开始, 显示警告'脚本已热重载, 建议重新加载存档'。重载时 GameState 重置: call_stack 清空, tokens 重置, co 重新创建, tf 重置为{}。同时清空所有图层(layers 全部 [cl]), 清空 backlog, 取消所有活跃 CancelToken 和阻塞操作。sf/f 保留。
**热重载时正在运行的协程处理**: 终止所有活跃协程(先遍历所有活跃协程的 CancelToken:cancel()(等待音频回调 max 100ms, 超时强制终止), 再 coroutine.close()。C++ 侧 RTT shared_ptr 在 bgfx::frame() 后延迟销毁。持有外部资源的协程注册 cancel 回调或 __close 元方法), 从上一存档点或标题页重新开始, 显示警告'脚本已热重载, 建议重新加载存档'。重载时 GameState 重置: call_stack 清空, tokens 重置, co 重新创建, tf 重置为{}。同时清空所有图层(layers 全部 [cl]), 清空 backlog, 取消所有活跃 CancelToken 和阻塞操作。sf/f 保留。
### [10.2.5] JSON 存档安全

**决策**: 禁止 load()/loadstring()。json.decode() 仅纯数据。coroutine/audio 不保存。

### [10.2.6] Scissor 协同

**决策**: union >75% 退化全量。单矩形 (bgfx 限制)。render_batch 扩展 scissor 字段。

### [10.2.7] 文本布局引擎

**决策**: TextLayoutEngine: 换行/对齐(H/V)/行间距/中英混排/ruby。C API: layout_text/layout_ruby。

### [10.2.8] 视频图层融合

**决策**: 帧作为 TextureHandle 绑定图层。与其他图层一致 quad 管线。自然得混合/变换/调色板。

### [10.2.9] Blend+Palette 组合

**决策**: CompositeShaderCache。key=hash(blend_mode, palette_ops, lut_texture_id)。palette_ops = {LUT纹理ID, 置换映射表}。Palette 先 Blend 后。单 Pass。

### [10.2.10] CARC 签名版本

**决策**: 签名载荷含 version。MIN/MAX 检查。旧版自动拒绝。

### [10.2.11] 音频混合模型

**决策**: 全局 Direct 模式 — 所有音频操作立即执行, 无批量累积。playbgm/playse/fadebgm 直接调用 SoLoud API(主线程安全); playvoice 使用音频结束回调 + coroutine.yield 实现阻塞语义(语音播放结束后由音频线程回调通过 SDL_PushEvent 在主线程 resume 协程, 避免忙等轮询)。
**简化理由**: 删除 per-stream 状态机(first_op_mode)、混合模式检测、Debug/Release 双行为、kag.isAudioDirectOnly() API。音频性能差异极小(SoLoud 本身混音线程与主线程解耦), 但代码复杂度降低约 60%, 彻底消除同一帧混用导致的边界 bug。**P0**.
### [10.2.12] TextureArray 字体图集

**决策**: 预分配 4 层 2048x2048 RGBA8。shader: texture2DArray。

### [10.2.13] Resize RTT 重分配

**决策**: Engine::onResize -> reset -> RTTManager::resize -> Lua DevCore -> dirty=true。

### [10.2.14] 音频线程安全

**决策**: 主线程独占。assert(m_ownerThread==id)。不引入锁。Lua 自然串行。

### [10.2.15] 3D 视口清除控制

**决策**: create_viewport({clear_color, clear_depth, no_clear})。

### [10.2.16] CARC 差分回滚

**决策**: 备份 .bak -> bsdiff -> .new -> 签名验证 -> rename/回滚。崩溃恢复检测 .bak。

 启动时检测 .carc_update_state 标记文件, 若存在则自动完成/回滚未完成的差分更新。
### [10.2.17] 视频解码器扩展

**决策**: IVideoDecoder 接口。Alpha: PlMpegDecoder。Beta: FFmpegDecoder。

### [10.2.18] 热重载/调试互斥

**决策**: ScriptState: IDLE/DEBUG_ACTIVE/RELOADING。互斥状态机。

### [10.2.19] 存档缩略图配置(编码流程见 [10.2.59])

**决策**: config: width/height/quality(1-100, 默认80, 对应 libpng zlib 压缩级别, 100=无损)/max_size_kb。降级: 320x180 -> 160x90 -> 禁用。

### [10.2.20] 3D/RTT 池类型区分

**决策**: RTType enum {RTT_2D,RTT_3D}。独立池。getViewportTexture 仅接受 RTT_3D。

### [10.2.21] 协程关闭守卫
**决策**: scheduler 执行取消流程(两阶段顺序化, cancellation_lock 防重入):
- **Phase 1 — 标记**: 设置 cancellation_lock=true, 遍历活跃 CancelToken 标记 cancelled=true。
- **Phase 2 — 清理协程**: coroutine.close()(Lua 5.4+, 自动触发 __close 释放文件句柄/RTT/音频)。
- **Phase 3 — 执行回调**: 遍历 CancelToken 注册回调(反向注册顺序, pcall 包裹, 保证回调执行时协程已完全关闭)。
- **回调约束**: 回调仅允许操作协程外部资源(主线程图层、全局句柄), 禁止访问协程局部变量/upvalue。Debug 模式断言违反则抛错。Release 模式记录 [WARN] 并跳过非法访问。
- **嵌套保护**: 若回调中再次触发 cancel, cancellation_lock 已设为 true 则直接返回, 所有回调执行完后释放锁。禁止在 cancel 回调中 yield。
**P1**.


### [10.2.22] RTT 池并发

**决策**: RTT 操作严格主线程独占(见 [10.2.55])。后台线程通过 SDL_PushEvent 派发请求, 主线程在事件循环处理。延迟销毁队列在每帧 bgfx::frame() 后统一销毁(引用计数归零且 last_frame_used < current_frame - 2 的 RTT)。不引入锁, 零开销。与 [10.2.55] 统一。

### [10.2.23] TextureArray 扩容

**决策**: A/B 双阵列 + 预测性静默扩容。预分配 8 层 2048x2048 RGBA8 (~128MB)。

**扩容时机**:
- **预测触发** (80%): 当前使用率 >=80% 时标记 `pending_expansion=true`, 在下一个 [trans]/[black] 或场景切换期间静默执行扩容(用户无感知, 无遮罩, 无降级)。
- **紧急触发** (90%): 若使用率达到 90% 仍未遇到转场/黑屏, 立即触发扩容。扩容期间优先从 CJK 静态图集渲染(Noto Sans CJK SC 子集, 3500 汉字覆盖率>99%), 若字符不在图集中则静默降级为内置 8x16 位图并显示 U+FFFD。不引入 UI 遮罩, 不中断用户输入, 翻页/跳过操作正常处理。扩容完成后自动恢复 FreeType 矢量渲染。

**扩容模式**: 增量扩容(新建第 5-8 层, 保留原 4 层, 无需拷贝)。CJK 静态图集覆盖常用 3500 汉字(覆盖率>99%), 紧急扩容仅触发于生僻字场景。开发者可通过 [preload_chars] 标签扩充字符集, 几乎可完全避免紧急扩容。翻页/跳过输入在扩容期间不受影响。扩容完成后自动恢复 FreeType 矢量渲染。此策略与 [10.2.54] 合并为一个条目。

**P2**.
### [10.2.24] FFmpeg/H.264 集成(P1, Beta 起默认 ON)

**决策**: IVideoDecoder 已预留。-DCAESURA_VIDEO_FFMPEG=OFF（Alpha 仅编译 pl_mpeg/MPEG1）。Beta 起默认 -DCAESURA_VIDEO_FFMPEG=ON（H.264/VP9/WebM）。1.0: AV1。

### [10.2.25] LUT 纹理优化

**决策**: 单纹理+progress uniform+easing(linear/ease-in/ease-out/ease-in-out)。禁止每帧创建。

### [10.2.26] 文本批处理

**决策**: MessageLayerCache。缓存顶点缓冲。仅变更时重建。单 submit O(1) draw call。

### [10.2.27] SE per-handle 音量

**决策**: setHandleVolume。BGM/Voice: Bus setVolume。SE: 8 Bus 池 + per-handle API。

### [10.2.28] CARC 公钥外置

**决策**: 公钥附加 CARC 末尾 32B。启动时读取验签。同份二进制验证不同密钥。**子 CARC 链式信任**: 证书存储于子 CARC 索引区块, 详细见 [10.2.63]。

### [10.2.29] 内存预算与 GC

**决策**: Lua 256MB+纹理 1GB+音频 128MB+对象池。分级响应: (1)80%->强制 GC step (2)95%->collect+清理音频缓存 (3)100%->错误界面, 尝试回滚安全点。显式 GC: [trans]后 step, [save]后 collect, 300帧 step。

### [10.2.30] 启动流程与错误恢复

**决策**: 分阶段初始化。bgfx 硬编码错误界面(深红+白字+内置字体)。零外部依赖。

### [10.2.31] GameState 单例

**决策**: ctx 统一传递。含 co/tokens/变量(f/sf/tf)/layers/backlog/CancelToken 列表。存档/热重载/调试基础。

### [10.2.32] 异步加载+预加载

**决策**: createTextureAsync + [preload]标签 + 过渡槽后台预加载。
**异步加载回调线程模型**: 后台线程通过 SDL_PushEvent 派发到主线程事件队列, 确保 bgfx/SoLoud 调用在主线程([10.2.14])。
**[preload] 标签语法**: `[preload type="texture"|"audio" storage="path1,path2" wait="true"|"false"]`。wait=true 阻塞脚本直到所有资源加载完成(异步回调 resume 协程); wait=false 后台预加载, 首次使用时可能显示占位纹理。
**[trans] wait_preload 选项**: [trans] 默认 wait_preload=true, 在启动转场动画前隐式等待过渡槽预加载完成, 避免显示棋盘格/占位纹理。开发者可显式设置 [trans wait_preload=false] 允许即时转场(异步加载未完成时显示占位, 加载完成后 layer:mark_dirty() 强制刷新)。**P1**.
### [10.2.33] CancelToken 取消

**决策**: 阻塞操作([wait]/[trans]/[move]/[quake]/playvoice 等)注册取消回调。[jump] 时遍历 cancel。pcall 包裹。防 RTT/音频泄漏。
**CancelToken API**(主线程执行取消回调): register(fn) / cancel()(反向遍历+pcall. 内部 cancelled 标志防重入. cancel 前先检查 cancelled, 若已取消则返回. 取消后 coroutine.close() 自动触发 __close 元方法释放资源)。取消语义: move=停止当前位置, trans=跳转结束, quake=立即停止, playvoice=fadeout(50ms)+stop。
### [10.2.34] 存档版本管理

**决策**: schema_version+migrate_save() 链。schema>CURRENT->拒绝。

### [10.2.35] i18n 深度集成

**决策**: kag.ch/text 自动 _T 替换(转义规则仅在 _T 内生效: \_ 输出下划线, \% 输出百分号。非翻译字符串不处理转义)(转义: \_ 原样输出下划线, \% 原样输出百分号)。Backlog 存 key 实时翻译(保存 key+参数表副本, 显示时根据当前语言重新插值 %{key})。%{key} 插值。

### [10.2.36] Lua 5.4 特性

**决策**: const 保护 system/config。__close 元方法示例: `local token <close> = CancelToken.new()` 作用域结束时自动调用 token:cancel()。音频/纹理句柄同样适用。luaL_warning。

### [10.2.37] lua_State 确认

**决策**: 全局单 lua_State*, lua_newthread 创建协程。共享注册表。

### [10.2.38] kag.lua 分期

**决策**: Alpha ~20 核心, Beta +15 动画, 1.0 ~50 全量。


Alpha(20命令): text, ch, p, l, r, er, current, ruby, font, bg, fg, cl, image, playbgm, stopbgm, playse, stopse, playvoice, wait, jump, if_/else_/endif
Beta(+15): trans, move, quake, fade, blur, playse3d, fadebgm, xfadebgm, save, load, link, call, return_, switch/case, eval
1.0(+15共50): waitclick, waitsound, movie, preload, backlog, config, saveplace(保存当前场景路径+PC+tf+对话索引到临时槽位)/loadplace(恢复, 独立于存档), history, mode, label, end_, vp, freeimage, emb

### [10.2.39] 错误界面策略

**决策**: 硬编码几何+内置 8x16 位图字体(ASCII 32-126, 不支持的字符显示 U+FFFD �)。零外部依赖。Lua/IAssetProvider 失效仍可工作。bgfx 彻底故障时退化为 SDL_ShowSimpleMessageBox 显示错误文本。

### [10.2.40] 性能基准

**决策**: Intel HD 620, 2C 2.0GHz, 4GB RAM, 60 FPS。每帧: SDL<0.5, Lua<3, Audio<0.5, Layer<1, GPU理想<10ms(典型场景)。最坏情况硬上限 16ms 见 [10.2.62]。

OS: Win10 22H2+ / Linux 5.10+。bgfx 后端: Vulkan 1.1+ / D3D11 / Metal(macOS)。

**典型场景预期**: 背景1层+立绘1层+文本层+BGM+SE = <8ms/frame(HD620); 转场动画 +2ms; 视频解码 +3ms。

### [10.2.41] [move] 贝塞尔曲线数学定义

B(t)=(1-t)^3*P0+3(1-t)^2*t*P1+3(1-t)*t^2*P2+t^3*P3。P0=起点, P1/P2=控制点(相对于P0的像素偏移), P3=终点。time 参数指定动画时长(ms), 支持缓动函数(linear/ease-in/ease-out/ease-in-out)。. path params define P1,P2. arcrad for corner radius. **P1**.

---

### [10.2.42] [quake] 点击交互规范

Clicks suppressed during quake. Embedded [p] pauses quake until click. offsetX=intensity*sin(t*freq)*exp(-t*damping). **P1**.

---

### [10.2.43] Cross-Version Save Migration

migrations={[1]=fn1,...}. Chain: for v=schema..CURRENT-1: data=migrations[v](data). Each fn handles v->v+1. **P1**.

---

### [10.2.44] FreeType 图集扩容锁策略

**决策**: 扩容流程: 标记扩容中 -> 创建新 TextureArray 层 -> 逐页拷贝已有字形 -> 旧层延迟销毁(下一帧 bgfx::frame() 后) -> 扩容期间优先 CJK 静态图集, 其次内置位图。扩容锁策略见 [10.2.55] (主线程独占, 无锁)。扩容仅在后台空闲帧执行。**P2**.

### [10.2.45] Audio Mixed-Model (见 [10.2.11])

**决策**: 参见 [10.2.11] 全局 Direct 模式。所有音频操作立即执行, 无批量累积, 无混合模式检测。本条目仅作交叉引用。

### [10.2.46] CARC Engine Self-Verification

Engine embeds ENGINE_VERSION. CARC.version <= ENGINE_VERSION allowed. Binary checksum in signature verification (anti-tamper). **P1**.

---

### [10.2.47] [eval]/[emb] Lua Sandbox

_ENV whitelist: KAG,Render,math,string,table. Block: os,io,package,debug,require,dofile,loadfile. **Render 仅暴露高层游戏命令**, 完整白名单(14 API): load_image, load_texture, destroy_texture, set_blend, set_blend_mode, create_viewport, destroy_viewport, draw_viewport, create_solid_texture, get_viewport_texture, set_viewport_clear, create_texture_async, set_font, get_text_bounds。禁止: 创建着色器/修改管线/直接 bgfx API。**禁止直接访问 bgfx 底层 API** (submit, setViewRect, setTransform 等 Render 内部实现细节, 不入 Lua API)。
**视口槽位限制**: viewport 使用预定义槽位(4 个, slot_id=1~4)。create_viewport(slot_id, params) 首次调用分配 GPU 资源(覆盖已有槽位), 同一 slot_id 再次调用复用已有资源。超限(>4)则 Lua 抛错。禁止 [eval] 内动态创建超过槽位数量的 viewport, 避免频繁 GPU 资源创建/销毁。
Dev mode relaxed, release strict. **P0**.

---
同时禁止: load, rawget, rawset, _G 不可访问。**[eval]** 当前协程, 读写 f/sf/tf。**禁止在 [eval]/[emb] 内执行控制流命令及阻塞操作(playvoice/wait/[p]/[trans] 等)。
**__close 保证**: coroutine.close() 无论协程执行到哪一行, 均触发所有当前作用域内 local x <close> = ... 变量的 __close 元方法(Lua 5.4 语义保证)。未声明 <close> 的资源必须通过 CancelToken.register 手动注册释放回调。[eval]/[emb] 沙箱内所有资源句柄强制使用 <close> 声明。
若外部 CancelToken 取消, 按序执行(与 [10.2.21] 两阶段模型一致): (1)标记 cancelled=true, cancellation_lock=true (2)触发协程内所有 __close 元方法(释放文件句柄/RTT/音频) (3)coroutine.close() 关闭协程 (4)执行 CancelToken 注册回调(pcall 包裹) (5)Lua 抛错 'eval_cancelled'。
** ([jump]/[link]/[call]/[return_]/[end_]), 违反则 Lua 抛错。**[emb]** 临时协程, return 返回。所有沙箱内资源持有对象必须实现 __close 元方法, CancelToken 取消时保证资源释放。**P0**.

### [10.2.48] Hot-Reload Static Analysis

C++ handle validity check post-reload, 覆盖全部句柄类型: TextureHandle, ShaderHandle, ViewportHandle, TransitionHandle, AudioHandle, RTTFrameBuffer。每类句柄维护 generation 计数器, is_valid() 检查 ID+generation 双匹配。 Invalid handles -> INVALID_HANDLE sentinel. proxy.lua audit module (dev mode). **P2**.

---


### [10.2.49] Glossary (Appendix C)

RTT, LUT, Ed25519, AES-256-GCM, zstd, SPIR-V, HAL, PTS, NDC, CARC, KAG, LPeg, CancelToken, Provider Chain, TextureArray, Scissor. **P2**.

---


### [10.2.50] 全局异常捕获与错误界面

**决策**: kag.try(cmd_fn) 包裹所有命令执行。未捕获异常触发 C++ 错误界面: 显示脚本堆栈 + 当前 tokens 行号 + [R]etry [T]itle [Q]uit 三选项。Retry 重新执行当前 token, Title 回标题, Quit 退出引擎。错误界面使用硬编码几何+内置位图字体([10.2.39])。**P1**.

**Retry 白名单**: Retry 重新初始化命令全部状态, 新建 CancelToken。Retry 仅对以下命令有效: text, ch, bg, fg, cl, image, playbgm, stopbgm, playse, stopse, setvolume, wait, if/else/endif, ruby, font, current。动画/转场(move/trans/quake/fade)不支持 Retry, 若在动画执行中触发 Retry 直接返回 Title。同一 token 连续 3 次 Retry 失败自动 Title。

**Title 崩溃保护**: 全局错误计数器, 若连续 Title 回退 3 次仍崩溃, fallback 到 SDL_ShowSimpleMessageBox(仅显示错误文本 + 退出按钮), 不再尝试恢复脚本。
### [10.2.51] Lua 沙箱 Dev/Release 开关

**决策**: --unsafe CLI 参数仅在开发构建中可用。Release 构建永久强制沙箱([10.2.47])。Dev 模式放宽 _ENV 白名单, 允许 require/dofile/loadfile。Release 模式严格限制, 任何绕过沙箱的尝试记录警告并拒绝执行。**P0**.

---


### [10.2.52] (已合并至 [10.2.26] + [10.2.7])

**决策**: 原为独立文本优化决策，内容已整合到 [10.2.26] (MessageLayerCache 顶点缓冲缓存, O(1) draw call) 和 [10.2.7] (文本布局引擎自动换行/对齐/间距/ruby)。本条目仅作索引占位。

---

### [10.2.53] 脏矩形 Scissor 触发机制

**决策**: LayerManager::update_dirty_regions() 每帧由 layers.lua 的 render() 调用。标记逻辑含透明混合图层: 设置混合模式(带透明度)时向下层递归标记受影响区域。函数 Layer:mark_dirty_with_transparency(rect)向下层递归。 load_image/set_pos/set_scale/visible 变更时自动标记 dirty。提供 layers.mark_all_dirty() 强制全局重绘。GPU Scissor 矩形取所有 dirty_rects 的并集, 若 union_area / screen_area > 0.75 则退化全量重绘。**P1**.

---


### [10.2.54] FreeType 纹理数组扩容时的字体降级 (已合并至 [10.2.23])

**决策**: 参见 [10.2.23] TextureArray 扩容主条目。三层 fallback 链路 (FreeType -> CJK 静态图集 -> 8x16 位图+U+FFFD) 的完整规格已在 [10.2.23] 定义。本条目仅作交叉引用。**P1**.

---
**CJK fallback**: 内置 Noto Sans CJK SC 子集(常用 3500 汉字(未压缩 RGBA8 4096x4096 ~64MB, ASTC/BC7 压缩后 ~16MB)) 4096x4096 RGBA8 图集, 作为独立静态纹理。扩容时优先使用 CJK 图集(不依赖动态 TextureArray), 其次降级到位图。提供 [preload_chars] 标签预加载字符列表; 提供工具扫描 .ks 脚本生成所需字符集。对话中扩容施加 50ms 遮罩。
### [10.2.55] RTT 池线程安全策略

**决策**: RTT 操作严格主线程独占(assert(is_main_thread()))。后台线程通过 SDL_PushEvent 派发创建/销毁请求, 主线程在事件循环中处理, 后台线程永不直接触碰 RTT 句柄。永久不引入自旋锁 — 引擎架构保证 bgfx 全操作主线程独占, 多线程 RTT 通过事件队列解耦。RTT 池延迟销毁队列在每帧 bgfx::frame() 后统一处理。此设计消除锁开销, 简化并发模型。**P2**.


---
### [10.2.56] (已合并至 [10.2.55] RTT 池线程安全策略)

**决策**: 原为独立线程安全决策，内容已整合到 [10.2.55] (RTT 池主线程独占 + 事件队列解耦, 永久无锁)。本条目仅作索引占位。

---

### [10.2.57] 异步加载回退策略 (见 [10.2.32])

**决策**: 参见 [10.2.32] 异步加载+预加载条目。占位纹理: Dev 棋盘格(紫黑 16x16), Release 50%灰色半透明(RGBA 128,128,128,128)+角落半透明加载图标(32x32, config.placeholder_icon 可关闭)。灰色占位提供适度视觉反馈, 避免用户误以为资源缺失。开发者可通过 config.placeholder_style={"gray"|"transparent"|"custom"} 覆盖。加载完成回调调用 layer:mark_dirty() 强制下一帧重绘, 允许短暂闪烁。**P1**.


### [10.2.58] 热重载后 C++ 句柄存活类型清单

**决策**: 热重载后 Lua 侧通过 backend.is_valid(handle) 统一入口验证句柄有效性(handle ID+generation 双重匹配, 旧 generation 自动失效)。覆盖全部句柄类型:
(1) **TextureHandle** — Lua 重新 load_image 获取新句柄, 旧句柄 C++ 侧惰性 GC。
(2) **ShaderHandle** (bgfx::ProgramHandle + generation) — 热重载后旧 shader generation 失效, Lua 重新 create_shader_from_file。
(3) **ViewportHandle** (bgfx::FrameBufferHandle + generation) — 旧 viewport RTT 归入延迟销毁队列。
(4) **TransitionHandle** (RTT 对 + generation) — 转场进行中热重载时取消转场, 归还 RTT 到池。
(5) **RttHandle** (FrameBufferHandle + generation) — 由 RTT 池统一管理, 池内句柄在热重载后全部标记 INVALID。
(6) **AudioHandle** — 停止旧播放, 释放 SoLoud 句柄。新脚本重新 play 获取新句柄。
每种句柄类型维护独立 generation 计数器(uint32_t, 递增不回收)。热重载时所有类型 generation 统一递增, 旧句柄自动失效。
**弱表自动保护**: Dev 模式下默认启用 handle_weak_table, 使用 Lua 弱表(__mode='v')自动将旧句柄置 nil, 脚本无需显式检查。Release 模式可选开启(config.handle_weak_table=true)。
热重载脚本负责调用 is_valid, 无效句柄转为 INVALID_HANDLE(-1) sentinel。**P1**.

---


### [10.2.59] 存档缩略图自适应压缩 (已合并至 [10.2.19])

**决策**: 参见 [10.2.19] 存档缩略图配置主条目。编码流程 (分辨率折半 -> 纯色降级) 和线程模型 (SDL_PushEvent 回调) 的完整规格已在 [10.2.19] 定义。本条目仅作交叉引用。**P2**.

---


### [10.2.60] Lua 内存硬上限强制执行

**决策**: 启动时 lua_gc(L, LUA_GCSETPAUSE, 200); lua_gc(L, LUA_GCSETSTEPMUL, 300) 激进 GC。主循环每帧末尾调用 lua_gc(L, LUA_GCCOUNT, 0) 检查内存(返回 KB, 比较 > 256*1024); 超过 256MB 时主动 lua_gc(L, LUA_GCCOLLECT, 0) 并触发错误界面, 超过 256MB 时抛出 [ERROR] Lua memory budget exceeded (256MB), 触发错误界面([10.2.50])。显式 GC 点(trans 后/存档时)调用 collectgarbage("collect") 主动回收。对象池复用 render_batch/事件表减少 GC 压力。C++ 对象池通过 custom allocator 监控总内存, 超过预算(纹理 1GB/音频 128MB)触发降级([10.2.29])。**P1**.

---


### [10.2.61] [link] 命令完整语义

**决策**: [link] 场景跳转前依次执行: (1) 遍历所有活跃 CancelToken 调用 cancel() (2) cancel() 主线程同步执行, 逐一 pcall 所有注册回调, 等待全部回调执行完毕 (3) 回调中禁止 yield/启动新协程/[jump]/[link], 违反则记录错误并跳过 (4) 清空所有图层+backlog (5) 加载新场景。**P1**.

--- [link] 执行前 cancel_token:cancel()。
### [10.2.62] 性能预算与自适应

**决策**: 引擎提供自动自适应混合手动三档的性能管理系统。

**GPU 帧预算** (目标硬件 HD 620 集成显卡 @ 1280x720):
- 4 图层 x 2048x2048 RGBA8 纹理采样 (~8ms) + 28 种混合模式单 Pass (~2ms) + 调色板 LUT (~1ms) + 字体批处理 (~1ms) + 离屏 RTT (~2ms) + 余量 (2ms) = <=16ms/帧 (>=60 FPS)。

**config.performance_profile** (开发者/用户可选择):
- `"auto"` (默认) — GraphicsMonitor 自动监控帧时间, 动态升降档。超出预算时自动降级, 恢复后自动回升。
- `"high"` — 锁定最高画质: RTT 1x, FreeType 矢量字体, MSAA 4x, 全后处理特效。适合中高端桌面 GPU。
- `"medium"` — 平衡档: RTT 0.75x, FreeType 矢量字体, MSAA 2x, 简化后处理。适合 HD 620 级别。
- `"low"` — 性能优先: RTT 0.5x, 内置位图字体, MSAA 关闭, 无后处理特效。适合低端集显/移动端。

**GraphicsMonitor** (auto 模式下生效):
- 每帧采集 gpuTimeMs (若 bgfx 不支持 GPU 计时, 降级为 CPU 帧预算: 子系统上报 -> 主线程汇总)。
- 连续 3 帧 >18ms 触发降级, 30 帧 <12ms 触发回升。
- 降级顺序: (1) 关闭抗锯齿 (MSAA 4x -> 1x) (2) 降低 RTT 分辨率到 0.5x (3) 禁用后处理特效 (4) 降级为内置字体。
- 逐级尝试, 每级等待 10 帧评估效果。
- 手动档 (low/medium/high) 完全禁用 GraphicsMonitor 自动调整。

**P1**.

### [10.2.63] 多 CARC 公钥管理 (链式信任模型, 认证有效期可由开发者配置)

**决策**: 引擎信任主 CARC Ed25519 公钥(Alpha: 编译时嵌入(-DCAESURA_ROOT_PUBKEY=...)。Beta: 支持从首个 CARC 固定偏移读取, 需签名链验证。编译时嵌入更安全, 建议长期保留。)。子 CARC 使用独立密钥, 其公钥由主公钥签发授权证书(含子公钥哈希+有效期+权限范围), **证书存储于主 CARC 内虚拟路径 chain/ (打包在 CARC 中, 通过 IAssetProvider 读取, 构建工具自动生成)**。验证: (1) 读子公钥(子CARC末尾32B) (2) 读主CARC chain/下对应证书 (3) 主公钥验证证书签名 (4) 比对证书内公钥哈希与末尾公钥 (5) 子公钥验签。**P0**。
**认证有效期策略**: 授权证书包含可选的 expiry 字段。开发者通过构建工具配置:
- `expiry: "2028-12-31"` — 设定明确过期日期, 过期后拒绝加载并显示提示
- `expiry: "none"` 或不填 — 证书永不过期, 适合需要长期流传的独立游戏和社团作品
- 拒绝加载时显示: "子CARC授权证书已过期(有效期至XXXX-XX-XX), 请联系提供商更新"
- 开发者模式 --unsafe 可强制跳过过期检查。
**CRL (证书吊销列表)**: 引擎提供 CRL 功能, 是否联网验证由开发者决定(引擎服务于单机游戏, CRL 为可选安全增强)。主密钥签发 CRL 文件(JSON, 含递增 version(uint32_t)+timestamp(ISO8601)+吊销指纹列表)。
**CRL 模式**(开发者在主 CARC config.json 中配置 `carc_crl_mode`):
- `"offline"` (默认) — 仅本地验证: 检查本地缓存 CRL(saves/crl_cache.json, 主密钥签名), 无缓存则跳过 CRL 检查仅验证签名, 记录 `[INFO] CRL not cached, signature-only verification`。适合纯单机游戏。
- `"online"` — 联网获取: 从 `carc_crl_url`(必须 HTTPS) 拉取最新 CRL, 缓存到本地。网络不可达时拒绝加载子 CARC, 显示错误提示。适合需要实时吊销能力的游戏。
- `"hybrid"` — 先尝试在线获取, 超时(5s)则回退本地缓存, 无缓存则跳过 CRL(仅签名), 记录 `[WARN] CRL fetch timeout, using cached/offline verification`。
**CRL 防降级机制**: (1) 比较 CRL version >= last_known_crl_version(存储于 saves/config.json) (2) 比较 CRL timestamp > last_known_crl_timestamp (3) 主密钥验签 CRL 文件。任一校验失败则拒绝加载。被吊销的子公钥即使未过期也拒绝加载。expiry "none" 的永久证书可通过 CRL 吊销。**P0**.
**P2** (CRL 远程更新).

### [10.2.64] 移动平台适配 (iOS/Android)

**决策**: 移动平台提供原生深度集成, 引擎层完整适配。核心能力:
(1) **生命周期**: SDL3 原生处理 AppDelegate(iOS)/Activity(Android) 回调。Lua 侧提供 onPause() → returns table 和 onResume(savedData) 钩子, 引擎自动管理 savedData 生命周期(引擎自动暂停/恢复 SoLoud 音频和帧循环)。onPause() 可返回需要持久化的临时数据表, onResume 接收该表。onDestroy 用于最终清理。
(2) **输入**: 完整多点触控手势支持(单点/双指缩放/长按/滑动/拖拽), 通过 SDL_FingerEvent + SDL_MultiGestureEvent 分发到 Lua input 模块。提供 kag.isMobile() API 供脚本检测平台。
(3) **音频后台策略**: onPause 时自动暂停 SoLoud(可配置 config.audio_background=false 允许后台继续播放 BGM)。onResume 时恢复并补偿音频时间偏移。
(4) **渲染优化路径**: iOS 优先 Metal(bgfx Metal backend), Android 优先 Vulkan(Vulkan 1.1+), 回退 GLES 3.0+。移动端 RTT 分辨率自动降为 0.5x(可配置)。
(5) **字体与 DPI**: 移动端优先系统字体(Core Text / Android Font), Freetype 作为 fallback。缩放因子由 SDL_GetDisplayDPI() 计算, 自动匹配 Retina/HDPI 屏幕。
(6) **文件系统**: Android 使用 AAssetManager 读取 APK 内 assets, iOS 使用 NSBundle mainBundle。CARC 容器同样支持移动端路径解析。
(7) **平台特定 API**: 预留振动(haptic)、通知(notification)、剪切板(clipboard)等 Lua API, 通过 kag.platform 表暴露。
**P2** (Alpha 仅桌面, Beta 起实现移动端完整适配)。

## [10.3] 实施优先级

### P0 (13 项): 编码前规格解决
1. [10.2.31] GameState 单例
2. [10.2.33] CancelToken 取消机制
3. [10.2.5] JSON 存档安全
4. [10.2.47] [eval]/[emb] 沙箱白名单
5. [10.2.51] 沙箱 Dev/Release 开关
6. [10.2.1] Provider Chain 责任链
7. [10.2.28] CARC 公钥外置
8. [10.2.63] 多 CARC 链式信任
9. [10.2.10] CARC 签名版本防降级
10. [10.2.7] 文本布局引擎
11. [10.2.11] 音频全局 Direct 模式
12. [10.2.2] 视频 PTS 同步
13. [10.2.3] 3D 着色器安全

### P1 (30 项): Alpha 完整实现
1. [10.2.12] FreeType TextureArray
2. [10.2.6] 脏矩形 Scissor
3. [10.2.14] 音频线程安全
4. [10.2.9] Blend+Palette 组合
5. [10.2.8] 视频图层融合
6. [10.2.13] Resize RTT
7. [10.2.4] 脚本热重载
8. [10.2.25] LUT 纹理优化
9. [10.2.26] 文本批处理
10. [10.2.27] SE per-handle
11. [10.2.29] 内存预算
12. [10.2.30] 启动流程
13. [10.2.32] 异步加载+预加载
14. [10.2.34] 存档版本管理
15. [10.2.41] [move] 贝塞尔
16. [10.2.42] [quake] 点击
17. [10.2.43] 跨版本迁移
18. [10.2.46] CARC 自验证
19. [10.2.50] 错误界面
20. [10.2.54] 字体降级
21. [10.2.11] 音频 Direct
22. [10.2.57] 异步回退
23. [10.2.58] 句柄验证
24. [10.2.60] 内存硬上限
25. [10.2.61] [link] 语义
26. [10.2.62] GPU 预算
27. [10.2.59] 缩略图自适应
28. [10.2.2] 视频 PTS
29. [10.2.53] 脏矩形透明
30. [10.2.19] 缩略图配置

### P2 (14 项): Beta 迭代
1. [10.2.15] 3D 视口清除
2. [10.2.16] CARC 差分回滚
3. [10.2.24] FFmpeg/H.264
4. [10.2.23] TextureArray 动态扩容
5. [10.2.35] i18n 深度集成
6. [10.2.36] Lua 5.4 特性
7. [10.2.44] 图集扩容锁
8. [10.2.48] 热重载分析
9. Appendix C: Glossary
10. [10.2.55] RTT 事件队列
11. [10.2.17] 视频解码器扩展
12. [10.2.38] kag.lua 分期
13. [10.2.64] 移动平台
14. [10.2.49] 词汇表附录


### [10.2.65] API 数据结构原型

**CancelToken (Lua)**:
```
CancelToken = {
  register = function(self, cancel_fn) end,
  cancel = function(self) end
}
```

**create_viewport 参数**:
```
{ width=400, height=300, clear_color={0,0,0,0}, clear_depth=1.0, no_clear=false }
```

**mark_dirty_with_transparency(rect)**: rect = {x,y,w,h}。Erase/Alpha 模式强制标记全图层。

**max_viewports = 16**: 超限 Lua 抛错。

**lua_Alloc 钩子**: 每次分配后检查 lua_gc(L, LUA_GCCOUNT), 超 256MB 立即抛错。

**P1**.



### [10.2.66] 剩余小 Issue 解决路径清单

历轮审查中不阻塞编码但需有明确解决路径的非关键问题。每个条目: 解决思路 + 实现路径 + 实施阶段。

---

**I1. 着色器组合爆炸 (28 混合 x N Palette)**

- 思路: 运行时按需编译 + LRU 缓存(max 64)。
- 路径: CompositeShaderCache (unordered_map<hash, ProgramHandle>)。首次请求编译(1-3ms), 缓存满 LRU 淘汰。Alpha 预编译 10 种常用组合。
- 阶段: Alpha(10种) / Beta(28种)

---

**I2. TextureArray + CJK + 位图三层 Fallback**

- 思路: 三阶段 fallback 链路, 预分配避免运行时扩容。
- 路径: 第一优先 FreeType(8层预分配) -> 第二优先 CJK 静态图集(独立纹理) -> 第三优先 8x16 位图+U+FFFD。Alpha 不实现动态扩容(P2)。
- 阶段: Alpha(预分配+三层fallback) / P2(动态扩容)

---

**I3. 逐字打字机效果的顶点缓冲增量更新**

- 思路: 文本层脏范围标记, 仅重建变化段。
- 路径: MessageLayerCache.dirty_char_range={start,end}。[ch] 仅标记新增字符。每帧 rebuild_range() 更新 TransientVertexBuffer。完整重建仅在 [r]/[er]/[font] 触发。
- 阶段: Alpha

---

**I4. RTT 池纹理复用的残留帧清除**

- 思路: 默认 clear, 可选 no_clear。
- 路径: RTTManager::acquire(w,h,clear_mode)。DEFAULT 模式 bgfx 自动清除。NO_CLEAR 调用者保证覆盖全像素。从池取出即使 no_clear 也保证首帧有效。
- 阶段: Alpha

---

**I5. CARC 差分更新状态文件**

- 思路: JSON 状态机 + SHA256 校验和。
- 路径: .carc_update_state = {phase: updating|committing|rollback, checksum, timestamp}。启动时检测: committing+校验OK=完成, updating=回滚, 损坏=提示用户。
- 阶段: Beta (Alpha 不实现差分)

---

**I6. 转场+视频同时触发帧预算超限**

- 思路: 视频优先, 转场降级。
- 路径: 两者同时活跃时视频保持原帧率, 转场降级为简单淡入淡出。GraphicsMonitor 3帧>16ms 自动触发。config.video_priority 可配置。
- 阶段: Beta

---

**I7. CJK/ASCII/Emoji 混排字符宽度**

- 思路: 统一宽度表, CJK=2x, ASCII=1x。
- 路径: TextRenderer::getCharWidth(): CJK(U+4E00-U+9FFF等) 2x, ASCII 1x。Emoji 降级为 U+FFFD(若 FreeType 不支持)。自动换行基于累积宽度。
- 阶段: Alpha

---

**I8. 音频资源 LRU 卸载**

- 思路: 引用计数 + 最近最少使用淘汰。
- 路径: AudioCacheEntry{handle, ref_count, last_used, size}。每次 play ref_count++, stop ref_count--。总缓存超 128MB 时 LRU 卸载 ref_count=0 的句柄(SE优先)。
- 阶段: Alpha

---

**I9. 调试协议与热重载状态互斥**

- 思路: 三态 ScriptState 状态机。
- 路径: IDLE/DEBUG_ACTIVE/RELOADING 互斥。调试中热重载排队, 重载中斷开 socket+移除钩子。
- 阶段: P2 (Alpha: lua_sethook 单步, 无网络协议)

---

**I10. 移动平台 onPause/onResume**

- 思路: 预定义 Lua 钩子 + 引擎自动处理。
- 路径: _G.onPause/_G.onResume 全局回调。SDL_EVENT_DID_ENTER_BACKGROUND 暂停 SoLoud+帧循环。FOREGROUND 恢复+调用钩子。
- 阶段: P2 (Alpha 仅桌面)

---

**实施优先级**:
| 阶段 | Issue | 数量 |
|------|-------|:---:|
| Alpha | I3打字机/I4 RTT/I7字符/I8 LRU | 4 |
| Alpha(部分) | I1着色器(10种)/I2 fallback(预分配) | 2 |
| Beta | I5差分/I6转场/I9调试/I10移动 | 4 |

**P2**.

---

## [10.4] 设计质量结论

| 维度 | 状态 |
|------|------|
| 安全模型 | 完整 (JSON存档+CARC签名+3D校验+公钥外置+沙箱白名单+Dev/Release开关) |
| 线程安全 | 约束明确 (主线程独占+断言+无锁) |
| 资源生命周期 | 闭环 (__gc->shared_ptr->bgfx destroy) |
| 文本渲染 | 完整 (布局引擎+TextureArray+batch+ruby) |
| 着色器 | 完整 (28种+Palette组合+缓存) |
| 视频 | 完整 (PTS同步+图层融合+解码器接口) |
| 跨分辨率 | 完整 (letterbox/stretch/crop+resize链) |
| 打包安全 | 完整 (AES-256-GCM+Ed25519+版本防降级+原子回滚+公钥外置+链式信任) |
| 开发设施 | 完整 (热重载+调试互斥+错误界面+异常捕获+异步回退+句柄验证) |
| 生产就绪 | 完整 (版本迁移+CancelToken+异步加载+GC策略+CPU内存硬上限+GPU监控+链式信任) |

**结论**: 全部 64 项设计问题均已解决。P0:13, P1:30, P2:14, Confirmed:4。另有 3 项已合并条目。可进入编码阶段。


---

## Appendix C: Glossary

| Term | Full Name | Description |
|------|-----------|-------------|
| RTT | Render To Texture | Off-screen render target |
| LUT | Look-Up Table | Transition rule image texture |
| Ed25519 | Edwards-curve DSA | Digital signature (128-bit security) |
| AES-256-GCM | AES Galois/Counter Mode | Authenticated encryption |
| zstd | Zstandard | Compression (3x faster than zlib) |
| SPIR-V | Standard Portable IR | Vulkan shader bytecode format |
| HAL | Hardware Abstraction Layer | bgfx role |
| PTS | Presentation Time Stamp | AV sync reference |
| NDC | Normalized Device Coordinates | -1..1 coordinate space |
| CARC | Caesura ARChive | Engine container format |
| KAG | Kirikiri Adventure Game | VN script tag system |
| LPeg | Lua Parsing Expression Grammar | .ks parser |
| CancelToken | Cancel Token | Blocking op cancellation handle |
| Provider Chain | Provider Chain | IAssetProvider priority chain |
| TextureArray | Texture Array | bgfx 2D Array Texture |
| Scissor | Scissor Rect | GPU render-area clipping |
| bsdiff | Binary diff | CARC differential update algorithm ([9.8]) |
| H.264 | AVC/H.264 | Video codec, Beta FFmpeg ([10.2.24]) |
---

## Appendix D: 28 Blend Modes (GLSL Reference)

混合公式遵循 W3C Compositing Level 1 + Porter-Duff 规范。所有模式在 blend.glsl 中实现, 输入: vec4 base(下层), ec4 blend(上层), float opacity。输出: ec4 result。

| # | 模式 | GLSL 核心 | 分类 |
|---|------|-----------|------|
| 1 | Normal | blend.rgb * blend.a + base.rgb * (1.0 - blend.a) | Basic |
| 2 | Multiply | base.rgb * blend.rgb | Darken |
| 3 | Screen | 1.0 - (1.0 - base.rgb) * (1.0 - blend.rgb) | Lighten |
| 4 | Overlay | hard_light(blend.rgb, base.rgb) | Contrast |
| 5 | Darken | min(base.rgb, blend.rgb) | Darken |
| 6 | Lighten | max(base.rgb, blend.rgb) | Lighten |
| 7 | ColorDodge | base.rgb / (1.0 - blend.rgb) | Lighten |
| 8 | ColorBurn | 1.0 - (1.0 - base.rgb) / blend.rgb | Darken |
| 9 | HardLight | hard_light(base.rgb, blend.rgb) | Contrast |
| 10 | SoftLight | soft_light(base.rgb, blend.rgb) | Contrast |
| 11 | Difference | abs(base.rgb - blend.rgb) | Comparative |
| 12 | Exclusion | base.rgb + blend.rgb - 2.0 * base.rgb * blend.rgb | Comparative |
| 13 | Hue | set_lum(set_sat(blend.rgb, sat(base.rgb)), lum(base.rgb)) | Component |
| 14 | Saturation | set_lum(set_sat(base.rgb, sat(blend.rgb)), lum(base.rgb)) | Component |
| 15 | Color | set_lum(blend.rgb, lum(base.rgb)) | Component |
| 16 | Luminosity | set_lum(base.rgb, lum(blend.rgb)) | Component |
| 17 | Add | min(base.rgb + blend.rgb, 1.0) | Lighten |
| 18 | Subtract | max(base.rgb - blend.rgb, 0.0) | Darken |
| 19 | Divide | base.rgb / max(blend.rgb, 0.001) | Lighten |
| 20 | LinearBurn | base.rgb + blend.rgb - 1.0 | Darken |
| 21 | LinearDodge | base.rgb + blend.rgb (clamped) | Lighten |
| 22 | VividLight | blend.rgb > 0.5 ? color_dodge : color_burn | Contrast |
| 23 | LinearLight | blend.rgb > 0.5 ? linear_dodge : linear_burn | Contrast |
| 24 | PinLight | blend.rgb > 0.5 ? max(base.rgb, 2.0*blend.rgb-1.0) : min(base.rgb, 2.0*blend.rgb) | Contrast |
| 25 | HardMix | vivid_light(base, blend) > 0.5 ? 1.0 : 0.0 | Contrast |
| 26 | Invert | 1.0 - base.rgb (ignores blend) | Comparative |
| 27 | Alpha | vec4(base.rgb, base.a * blend.a) (仅透明度) | Basic |
| 28 | Erase | vec4(base.rgb, base.a * (1.0 - blend.a)) | Basic |

**实现架构**: 所有 28 种混合在单个 blend.glsl 文件中。Layer 通过 setBlendMode(int mode) 选择, Shader 内 switch(mode) 分支。bgfx 单次 submit() 完成, 无额外 RenderPass。与调色板组合通过 CompositeShaderCache 单 Pass 合并([10.2.9])。


