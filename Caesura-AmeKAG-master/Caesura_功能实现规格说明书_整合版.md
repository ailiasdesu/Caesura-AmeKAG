# Caesura (AmeKAG) ����ʵ�ֹ��˵����

> 版本: 2026-06-07 | 决策: 74项(含Beta Job System) | 状态: Alpha RC→Beta路线图:P0: 16/16, P1: 32/32, P2: 8/23, 构建: Debug+Release 通过

---

## Ŀ¼

- Part 0-11

---

# Part 0: �ܹ�����

������� C++ Ӳ���� + Lua ȫҵ���߼��ķֲ�ܹ���

### GameState ȫ�ֵ��� ([10.2.31]

ctx ����: co, tokens, token_index, call_stack, variables(f/sf/tf), layers, backlog, active_operations(CancelToken), stop_flag, input_focus��

### lua_State ��ʵ�� ([10.2.37]

ȫ�ֹ���һ�� lua_State*��Э��ͨ�� lua_newthread ������

### ���ܻ�׼ ([10.2.40]

Intel HD 620, 2C 2.0GHz, 4GB RAM, 60 FPS @ 1280x720��

### �̰߳�ȫ ([10.2.14]

**Alpha (已实现):** 单线程独占 bgfx/SoLoud API，C++ 层面 `CAESURA_ASSERT_MAIN_THREAD()` 守护，无锁设计。

**Beta (计划):** 引入 fiber-based Job System，三区边界模型:
- **红区 (Main-Thread-Only):** bgfx 全 API、SoLoud 全 API、lua_State 读写、BackendRegistry 写入。编译期 `[[caesura::main_thread]]` attribute 强制执行，CI 静态分析检查。
- **绿区 (Thread-Safe CPU Tasks):** 文件 I/O、CARC AES-256-GCM 加解密、zstd 压缩/解压、stb_image 解码、JSON 序列化、粒子数学模拟、FreeType glyph 光栅化(独立 FT_Library 实例)。
- **黄区 (Synchronized Access):** BackendRegistry 只读 (shared_mutex)、GPU Monitor 指标 (atomic)、ResourceHandle 引用计数 (atomic)。

主线程通过 `MainThreadJobQueue` 每帧轮询接收 worker 完成的红区回调。详见 Part 8: 并行任务系统。

### �ڴ�Ԥ�� ([10.2.29]

Lua 256MB Ӳ���ޣ�`lua_Alloc` hook ǿ��ִ�У�������Ԥ����� 6 ������Ӧ���ԣ���� [10.2.65]����Ĭ���Զ�����豸���ܶ���������ظ��á���ʽ GC �㡣

### �첽���� ([10.2.32]

**Alpha (已实现):** 单线程 AsyncLoader + SDL 事件回调，16 槽队列，`createTextureAsync` + `[preload]` 标签。

**Beta (计划):** 迁移至 Job System 驱动的全管线异步加载:
- 纹理加载四阶段 pipeline: 文件读取 → CARC 解密+解压 → PNG/WebP 解码 → bgfx::createTexture。前三阶段在 worker 线程并行，第四阶段回主线程。
- 音频波形预解码在 worker 线程完成，SoLoud::loadFile 回主线程提交。
- CARC 自验证 (Ed25519 签名校验) 在 worker 线程完成。
- 旧 `AsyncLoader` 类标记 `[[deprecated]]`，内部委托给 `AssetPipeline`。详见 Part 8: 并行任务系统。

---

# Part 1: �ű�����

## [1.1] tokenizer.lua �� LPeg ���� .ks -> Token ����
## [1.2] scheduler.lua �� ���� tokens, kag[cmd](ctx,params), yield ����
### Э�̹ر����� ([10.2.21] �� abort/resume �� coroutine.close()
## [1.3] flow.lua �� [jump]/[call]/[return]/[link]/[end]
### CancelToken ([10.2.33] �� ��������ע��ȡ���ص�, [jump] ʱ����ȡ��
## [1.4] flow.lua �� [if]/[else]/[endif]/[switch]/[case]

---

# Part 2: ͼ����Ⱦ

## [2.1] layers.lua �� bg(0), fg(1), message(2) z-order
### ����� Scissor ([10.2.6] �� union >75% �˻�ȫ��
### ��Ļ���� �� letterbox/stretch/crop
### Resize RTT ([10.2.13] �� onResize -> reset -> DevCore ֪ͨ -> dirty
## [2.2] blend.lua + GLSL �� 28 �ֻ��ģʽ (��ϸ��ʽ�嵥�� Appendix D)
## [2.3] transition.lua + GLSL �� Rule Image, progress+easing
### LUT �Ż� ([10.2.25] �� ������+uniform, ��ֹÿ֡����
## [2.4] transform.lua + GLSL �� ����任, 4 ���ز���
## [2.5] vfx.lua �� Quake/Blur/Fade

---

# Part 3: ��Ƶϵͳ

## [3.1] audio.lua �� BGM Bus(����), Voice Bus(����+���), SE Bus ��(x8)
### ����ύ ([10.2.11] �� ������ batch, ����ֱ��
### SE per-handle ([10.2.27] �� setHandleVolume
## [3.2] ���뵭�� �� SoLoud fadeVolume/xfade
## [3.3] ���ͬ�� �� playvoice �ص�����: �������ʱ����Ƶ�̷߳����ص�(SDL_PushEvent), ���̻߳��� Lua Э�̡���ֹ��ѯʵ��

---

# Part 4: ϵͳ��ʩ

## [4.1] Backlog �� {text, voice, timestamp}, �� key ʵʱ����
## [4.2] �浵 �� JSON ���л� ([10.2.5], schema_version Ǩ�� ([10.2.34]
## [4.3] Config �� text_speed/design/adapt/volume/thumbnail/texture_budget

- `texture_budget`: ����Ԥ�㵵λ��ȡֵ `"auto"`��Ĭ�ϣ������Զ���⣩�� 0|1|2|3|4|5 �ֶ�ָ������λ����� [10.2.65]��
### kag.lua ���� ([10.2.38] �� Alpha ~20/Beta ~15/1.0 ~50
## [4.4] i18n �� _T + ����� + %{key} ��ֵ
### ��ȼ��� ([10.2.35] �� ��ʾǰ�Զ�����, Backlog �� key

---
# Part 5: ��Ƶ��߼���Ⱦ

## [5.1] ��Ƶ���� �� pl_mpeg (MPEG1 Alpha), IVideoDecoder �ӿ�Ԥ��
### PTS ͬ�� ([10.2.2] �� SoLoud::getAudioPosition -> ׷��/��֡, ���� +/-1 ֡
### ��Ƶͼ���ں� ([10.2.8] �� ֡��Ϊ bgfx::TextureHandle ��ͼ��, ������ͼ��һ�µ� quad ����
### FFmpeg/H.264 ����(P1) ([10.2.24] �� Beta: -DCAESURA_VIDEO_FFMPEG=ON, H.264/VP9/WebM

## [5.2] FreeType ʸ������ �� FontRenderer.h/.cpp
### ����ͼ�� TextureArray ([10.2.12] �� Ԥ���� 4 �� 2048x2048 RGBA8, shader: texture2DArray
### ��̬���� ([10.2.23] �� 4 �� >90% ʱ A/B ˫�����л�, 1-2 ֡��͸������+ɳ©����ָʾ, �����ڼ���ͣ������Ⱦ(����ͣ��ҳ/��������), ������ɺ��Զ��ָ�������Ⱦ��������
### �ı��������� ([10.2.7] �� �Զ�����/����(H/V)/�м��/��Ӣ����/ruby ע��
### �ı������� ([10.2.26] �� MessageLayerCache ���涥�㻺��, �� submit, O(1) draw call

## [5.3] ʵʱ��ɫ�� �� palette.lua + GLSL
### Blend+Palette ��� ([10.2.9] �� CompositeShaderCache �� Pass, Palette �� Blend ��

---

# Part 6: 3D С��Ϸ��չ�ӿ�

## [6.1] 3D ͼ�νӿ� �� rtt.lua
### ��ȫԼ�� ([10.2.3] �� ��ɫ������ 64KB, Lua �� create_shader_from_file, Vertex 48B
### �������� �� Lua __gc -> destroyViewport, C++ shared_ptr
### RTT �������� ([10.2.20] �� RTType enum, 2D/3D ������

## [6.2] 3D/2D ��Ϲ���: 3D ����������Ⱦ�� RTT, ���� RTT ��Ϊ��ͨ����󶨵�ͼ��(bg/fg), ���û��ģʽ����ֹ 3D �ӿ��� 2D ͼ��ֱ�ӵ�����Ⱦ��

### ������� ([10.2.15] �� create_viewport({clear_color, clear_depth, no_clear})
### RTT �ز��� ([10.2.22] �� spinlock + �ӳ����ٶ���, ���������

---

# Part 7: ϵͳ��ʩ����

## [7.1] �浵����ͼ �� �����÷ֱ���/���� ([10.2.19]
## [7.2] �ű������� �� �ּ���ȫ��Χ, ��Դ�ָ� ([10.2.4]
### ������+���Ի��� ([10.2.18] �� ScriptState ״̬�� IDLE/DEBUG_ACTIVE/RELOADING

---

# Part 8: 并行任务系统 (Beta)

基于 fiber-based Job System（参考 Destiny 引擎模型），将 CPU 密集型任务从主线程剥离到工作线程池。严守三条红线: bgfx、SoLoud、lua_State 永远只在主线程调用。KAG Conductor/协程调度保持串行以保证故事演出时间线的确定性。

## [8.1] Job System 核心 ([10.2.72])

**线程池:** 线程数 = `std::thread::hardware_concurrency() - 1`（预留一核给主线程）。CPU 核心数 < 4 时回退为单 worker + 主线程直行模式，不创建 fiber。

**Work-Stealing:** 每个 worker 线程持有独立的无锁工作队列。空闲线程从繁忙线程队列尾部窃取 job，最小化锁竞争。

**Fiber 轻量上下文切换:** 长链 job 使用 fiber（`CreateFiber`/`ConvertThreadToFiber` on Windows，`makecontext`/`swapcontext` on Unix），避免 `std::thread` 创建开销。fiber 栈大小 64KB，允许数千并发 job。

**优先级:** High（音频预解码/关键资源）、Normal（纹理加载/CARC 解密）、Low（粒子/预加载/字体光栅化）。优先级影响 work-stealing 顺序但不保证实时性。

**主线程回调:** worker job 完成后，其 `onComplete` 回调入队到 `MainThreadJobQueue`。主线程每帧 `pollMainThreadJobs()` 按 FIFO 顺序执行红区回调（bgfx/SoLoud/Lua 调用）。

## [8.2] 红线边界 ([10.2.73])

编译期强制三类区域:

| 区域 | 允许的 API | 线程 | 编译检查 |
|------|-----------|:---:|------|
| **红区** | bgfx 全 API、SoLoud 全 API、lua_State 读写、BackendRegistry 写入 | 仅主线程 | `[[caesura::main_thread]]` attribute |
| **绿区** | 文件 I/O、CARC AES-256-GCM、zstd、stb_image、JSON 序列化、粒子数学、FreeType glyph | 任意 worker 线程 | 无限制（纯 CPU/无副作用） |
| **黄区** | BackendRegistry 只读、GPU Monitor 指标、ResourceHandle 引用计数 | 任意线程 | shared_mutex / atomic |

**断言宏更新:**
- `CAESURA_ASSERT_MAIN_THREAD()` — 红区专用，Debug 构建 abort
- `CAESURA_ASSERT_MAIN_THREAD_OR_JOB()` — 绿区+黄区用，允许 worker 线程
- 新增 `CAESURA_ASSERT_NOT_MAIN_THREAD()` — 验证当前在 worker 线程

## [8.3] 资源管线并行化 ([10.2.74])

**纹理加载管线（四阶段）:**

```
[worker] 文件读取 → [worker] CARC解密+zstd解压 → [worker] PNG/WebP解码 → [main] bgfx::createTexture
```

- 阶段 1-3 通过 JobSystem 并行调度，4 个纹理可同时处理
- 阶段 4 通过 MainThreadJobQueue 串行提交（bgfx 单线程约束）
- `AssetPipeline` 统一入口: `loadTextureAsync(path)` 返回 `JobHandle`
- 旧 `AsyncLoader` 标记 `[[deprecated]]`，内部委托给 AssetPipeline

**音频预解码管线:**

```
[worker] 文件读取 → [worker] 波形解码(WAV/OGG/MP3) → [main] SoLoud::loadFile
```

**CARC 自验证:** `IAssetProvider::verify()` 的 Ed25519 签名校验在 worker 线程完成，只将布尔结果通过黄区回传主线程。

## [8.4] 粒子系统并行化 ([10.2.75])

`ParticleSystem::update()` 拆分为两个阶段:

**阶段 1 (worker):** `simulateParticlesJob` — 并行执行粒子物理模拟（位置、速度、生命期、颜色衰减）。Emitter 按 `MAX_PARTICLES/thread_count` 均分到各 worker。

**阶段 2 (main):** `uploadVertexBuffer` — 主线程将模拟结果写入 bgfx transient vertex buffer，`bgfx::submit()`。

**一致性保证:** 所有 worker 共享只读的 emitter 配置，每个 worker 写入独立的粒子数组子范围，无竞争。

## [8.5] 字体光栅化并行化 ([10.2.76])

利用 FreeType 2.10+ 的 `FT_Library` 多实例安全特性:

- 主线程 `FontRenderer` 持有权威 atlas 和主 `FT_Library`
- 每个 worker 线程通过 `FreeTypeContext::createWorkerFTLibrary()` 获取独立 `FT_Library` + `FT_Face` clone
- Worker 并行光栅化不同 glyph 的 bitmap
- 完成结果通过 `MainThreadJobQueue` 回传 → 主线程 `uploadGlyphToAtlas()` 上传至 TextureArray
- 动态 atlas 扩容期间暂停 worker 光栅化（红区互斥）

## [8.6] 存档序列化并行化 ([10.2.77])

- **保存路径:** 主线程收集 GameState → worker 线程 JSON 序列化 → 主线程 `fwrite()` 写磁盘
- **加载路径:** worker 线程读盘+解析 JSON → 主线程恢复 GameState 到 Lua VM
- Worker 线程持有只读的 schema version 和 migration chain 引用

## [8.7] KAG Conductor 保持串行 ([10.2.78])

KAG 协程调度 (`scheduler.lua` + `conductor.lua`) 和 CancelToken 生命周期严格绑定主线程，不进入 Job System。

**设计理由:**
- 故事演出时间线的确定性高于并行吞吐量
- 同一时刻只有一个协程在 active 状态
- token 消费严格有序，乱序执行会破坏演出节奏和视觉一致性
- CancelToken 的两阶段取消模型依赖单线程的执行顺序保证

引擎保证: 任何 `[jump]`/`[call]`/`[return]` 指令的执行与 token 流顺序严格一致，不受 Job System 并行调度影响。

# Part 9: 调试工具接口

## [8.1] ����Э�� �� lua_sethook �ϵ�/���� ([10.2.18]

**Release ���� debug.* ɳ��**: Release ������ͨ�� `_CAESURA_CONFIG.dev_mode` ȫ�ֱ������ơ��� dev_mode ʱ `lockdownScriptEnv()` �� debug ���滻Ϊֻ���Ӽ������� getinfo/traceback/getlocal/getupvalue/getuservalue/getmetatable���Ƴ� sethook/setlocal/setmetatable ��д�뷽������ͬʱ��д `require` Ϊ������ `package.loaded` ���棨��ֹ�ļ�ϵͳ��������`os.execute/os.remove/os.rename/os.exit` �� `io.open/io.popen` ȫ���滻Ϊ�޲���׮��`[eval]/[emb]` ����ʹ�� `sandbox.lua` �İ����� _ENV ɳ�䣬Dev/Release ˫ģʽ�Զ��л���
## [8.2] IAssetProvider �� ��Դ�����ļ�ϵͳ
### Provider Chain ([10.2.1] �� ������ģʽ, ���ȼ�: CARC > patch > dir

---

# Part 10: CARC 容器格式



ѡ���Զ��� CARC �������Ǳ�׼ zip �����ɣ�AES-256-GCM ��ʽ���� + Ed25519 ǩ�� + CRL ������ʽ����ģ���޷��ñ�׼������Alpha �׶μ��ṩ������ȫ��ϵ��

## [9.1] ���Ŀ�� �� AES-256-GCM + Ed25519 + zstd + mmap
## [9.2] �ļ����� �� Header(64B) + Content + Index(encrypted) + Signature(96B) + PublicKey(32B)

**������֤����**: ������Ŀ �� 10,000 �ļ���metadata JSON �� 1MB��header �ֶη�Χ��֤��magic/version/content_size �Ϸ��Լ�飩��
### ǩ���汾�� ([10.2.10] �� SHA-256(header||version||count||hashes), ����������
## [9.3] ��Կ���� �� private.key(������), public.key(CARC ĩβ)
### ��Կ���� ([10.2.28] �� �������ʱ��ȡ CARC ĩβ��Կ, �����ر���

## [9.4] ��������

1. �ļ�������� Content ����� Index Ԫ����
2. ������� 256-bit AES ��Կ + 96-bit nonce
3. AES-256-GCM ���� Content ����
4. Index �������: �ļ�·���б� + ƫ���� + AES ��Կ(�ý��շ���Կ����) + GCM Auth Tag
5. Index ����ͬ���� AES-256-GCM ����(��Կ������ CARC ����Կ)

## [9.5] ǩ����֤����

1. Ed25519 ǩ������: Header(64B) + Encrypted Content + Encrypted Index(���� Signature �� PublicKey �ֶ�)
2. ǩ��д�� CARC ĩβ 64B(Signature)
3. ��֤����: (1) ��ȡ��Կ (2) ���� SHA-512 ��ϣ (3) Ed25519 ��ǩ (4) ��ǩʧ�� �� �����������([10.2.50]
4. ǩ�������汾��([10.2.10], �������طŹ���
5. ��ʽ����([10.2.63] ��ʽ����ģ��: �� CARC ����Ȩ֤��, ����Կ��֤֤��������ӹ�Կ, �� CRL ��������

## [9.6] IAssetProvider ����ӿ� + Provider Chain ([10.2.1]
## [9.7] Lua �ֽ���Ԥ����
## [9.8] ��ָ��� �� bsdiff + ԭ�ӻع� ([10.2.16]

? zstd ѹ����Բ�ֲ��Ѻá���С�߼�������ܵ�������ѹ�������޴󡣽��������ڲ��ǰ��ѹ��bsdiff������ѹ�������̣���ʹ�� zstd �� `--patch-from` ģʽ��
## [9.9] �������� �� car_pack.exe

---

# Part 11: 设计决策闭环 (68 项)


---

## [10.1] ��������

| # | ���� | ���ȼ� | ���� |
|---|------|:-----:|------|
| 1 | Provider Chain ������ģʽ | P0 | CARC/��Դ |
| 2 | ��Ƶ PTS ����Ƶͬ�� | P0 | ��Ƶ |
| 3 | 3D ��ɫ����ȫԼ�� | P0 | 3D/��ȫ |
| 4 | �����طּ���ȫ��Χ | P1 | ���� |
| 5 | JSON ���л��浵��ȫ | P0 | �浵 |
| 6 | ����� Scissor �Ż� | P1 | ��Ⱦ |
| 7 | �ı��������� | P0 | ���� |
| 8 | ��Ƶͼ���ںϻ��� | P1 | ��Ƶ |
| 9 | Blend+Palette �����ɫ�� | P1 | ��Ⱦ |
| 10 | CARC ǩ���汾�ŷ����� | P0 | CARC/��ȫ |
| 11 | ��Ƶ����ύģ�� | P0 | ��Ƶ |
| 12 | FreeType TextureArray ͼ�� | P1 | ���� |
| 13 | ���� Resize RTT �ط��� | P1 | ��Ⱦ |
| 14 | ��Ƶ�̰߳�ȫ(���̶߳�ռ) | P1 | ��Ƶ |
| 15 | 3D �ӿ�������� | P2 | 3D |
| 16 | CARC ���ԭ�ӻع� | P2 | CARC |
| 17 | ��Ƶ��������չ�ӿ� | P2 | ��Ƶ |
| 18 | ������/���Ի���״̬�� | P1 | ���� |
| 19 | �浵����ͼ������ | P2 | �浵 |
| 20 | RTT 2D/3D �������ֳ� | P1 | 3D |
| 21 | Э����ʽ�ر����� | P1 | �ű� |
| 22 | RTT �ز������� | P2 | ��Ⱦ |
| 23 | FreeType �����������ݲ��� | P2 | ���� |
| 24 | FFmpeg ��Ƶ�������滮 | P2 | ��Ƶ |
| 25 | LUT �����Ż�(������+uniform) | P1 | ��Ⱦ |
| 26 | �ı�������(MessageLayerCache) | P1 | ���� |
| 27 | SE per-handle �������� | P1 | ��Ƶ |
| 28 | CARC ��Կ���� | P0 | CARC/��ȫ |
| 29 | �ڴ�Ԥ���� GC ���� | P1 | ϵͳ |
| 30 | ������������ָ� | P1 | ϵͳ |
| 31 | GameState ȫ�ֵ��� | P0 | �ű� |
| 32 | �첽������Ԥ���ز��� | P1 | ��Դ |
| 33 | CancelToken ȡ������+API | P0 | �ű� |
| 34 | �浵�汾������Ǩ�� | P1 | �浵 |
| 35 | i18n ��ȼ��� | P2 | ϵͳ |
| 36 | Lua 5.4 ���������� | P2 | �ű� |
| 37 | lua_State ��ʵ��ȷ�� | Confirmed | �ű� |
| 38 | kag.lua ����ʵ�ּƻ� | Confirmed | �ű� |
| 39 | ����������ⲿ���� | Confirmed | ϵͳ |
| 40 | ���ܻ�׼(HD620 + OS) | Confirmed | ϵͳ |
| 41 | [move] ������������ѧ���� | P1 | ���� |
| 42 | [quake] ��������淶 | P1 | ���� |
| 43 | ��汾�浵Ǩ���� | P1 | �浵 |
| 44 | FreeType ͼ�������� | P2 | ���� |
| 45 | ��Ƶ���ģ��˳��֤ | P1 | ��Ƶ |
| 46 | CARC ��������֤ | P1 | CARC/��ȫ |
| 47 | [eval]/[emb] Lua ɳ�� | P0 | ��ȫ |
| 48 | �����ؾ�̬���� | P2 | ���� |
| 49 | �ʻ��(Glossary) | P2 | �ĵ� |
| 50 | ȫ���쳣����+������� | P1 | ϵͳ |
| 51 | ɳ�� Dev/Release ���� | P0 | ��ȫ |
| 52 | (�Ѻϲ��� [10.2.33] | �� | �� |
| 53 | ����� Scissor �������� | P1 | ��Ⱦ |
| 54 | FreeType �������彵��+CJK | P1 | ���� |
| 55 | RTT ������������ | P2 | ��Ⱦ |
| 56 | (�Ѻϲ��� [10.2.11] | �� | �� |
| 57 | �첽���ؼ�ʱ����(���̸�) | P1 | ��Դ |
| 58 | �����ؾ����Ч�Լ�� | P1 | ���� |
| 59 | ����ͼ����Ӧѹ����ˮ�� | P2 | �浵 |
| 60 | Lua �ڴ�Ӳ����(64K ���) | P1 | ϵͳ |
| 61 | [link] ��������+CancelToken | P1 | �ű� |
| 62 | ���� GPU ֡Ԥ��+Monitor | P1 | ��Ⱦ |
| 63 | �� CARC ��ʽ����(��Ч�ڿ�����) | P0 | CARC/��ȫ |
| 64 | �ƶ�ƽ̨���� (iOS/Android) | P2 | ƽ̨ |
| 65 | ����Ԥ�� 6 ������Ӧ�������� | P0 | ϵͳ/���� |
| 66 | ����ѧ��ȫ��������ɣ������� | P0 | CARC/��ȫ |
| 67 | RTT ֡�����������ڣ������� | P0 | ��Ⱦ |
| 68 | ȫ�ֹ��� FreeType ʵ���������� | P1 | ���� |
| 69 | ��Ƶ���� LRU ����������� | P1 | ��Ƶ |
| 70 | ShaderCache ����Ȩģ�ͣ������� | P1 | ��Ⱦ |
| 71 | ����ػ�˳�������� | P2 | ϵͳ |
### ���ȼ��ֲ�

| ���ȼ� | ���� | ˵�� |
|:------:|:----:|------|
| P0 | 13 | ����ǰ������ |
| P1 | 33 | Alpha ����ʵ�� |
| P2 | 22 | Beta 路线图(含 Job System) |
| Confirmed | 4 | ��ȷ������ |
| **�ܼ�** | **61** | |

---

## [10.2] �������ժҪ

### ʵ��״̬

| ���ȼ� | ��ʵ�� | �ܼ� | ȱʧ�� |
|:------:|:-----:|:----:|------|
| P0 | 16 | 16 | ?????[10.2.65] ????????????? |
| P1 | 32 | 32 | ??????????????ShaderCache ????????? |
| P2 | 8 | 23 | 全部 Beta 迭代（含 Job System [10.2.72]-[10.2.78]）|
| Confirmed | 4 | 4 | �� |
| **总计** | **55** | **74** | 全部 P0/P1 完成，P2 22 项 Beta 规划 |

> ע: 4 �� Beta ��ȫ���ԣ�CRC32 ������ [Beta]��HMAC �浵ǩ�� [Beta]��api_seal ���� [Beta]��save_bind_to_device ��ѡ�豸ָ�� [Beta]����������ƣ������� Alpha ���롣���� [10.2.65] ����Ԥ������Ӧ��P0����������ߣ���

### [10.2.1] Provider Chain

**����**: ������ģʽ��CARC(10) > patch(8) > dir(5)��getSource() ������Դ��

### [10.2.2] ��Ƶ PTS ͬ��

**����**: SoLoud::getAudioPosition -> PTS_audio��׷��/��֡������ +/-1 ֡�� ����Ƶ��������Ƶ����, ��Ƶ��ͣ�����һ֡��**�û��ֶ�����**: �������ո������ CancelToken ȡ����Ƶ����, �ͷ���Ƶͼ�㲢�ָ� KAG ���ơ�
### [10.2.3] 3D ��ȫ��

**����**: ��ɫ������ 64KB��Lua �� create_shader_from_file��Vertex 48B��__gc Ԫ������

### [10.2.4] �����ػָ�

**����**: �ּ���ȫ��Χ��C++ ��Դ��֧�֡�handle ʧЧ -> INVALID_HANDLE��
**������ʱ�������е�Э�̴���**: ��ֹ���л�ԾЭ��(CancelToken:cancel() -> coroutine.close()), ����һ�浵������ҳ���¿�ʼ, ��ʾ����'�ű���������, �������¼��ش浵'������ʱ GameState ����: call_stack ���, tokens ����, co ���´���, tf ����Ϊ{}, sf/f �����
### [10.2.5] JSON �浵��ȫ

**����**: ��ֹ load()/loadstring()��json.decode() �������ݡ�coroutine/audio �����档

### [10.2.6] Scissor Эͬ

**����**: union >75% �˻�ȫ���������� (bgfx ����)��render_batch ��չ scissor �ֶΡ�

### [10.2.7] �ı���������

**����**: TextLayoutEngine: ����/����(H/V)/�м��/��Ӣ����/ruby��C API: layout_text/layout_ruby��

### [10.2.8] ��Ƶͼ���ں�

**����**: ֡��Ϊ TextureHandle ��ͼ�㡣������ͼ��һ�� quad ���ߡ���Ȼ�û��/�任/��ɫ�塣

### [10.2.9] Blend+Palette ���

**����**: CompositeShaderCache��key=hash(blend_mode, palette_ops, lut_texture_id)��palette_ops = {LUT����ID, �û�ӳ���}��Palette �� Blend �󡣵� Pass��

### [10.2.10] CARC ǩ���汾

**����**: ǩ���غɺ� version��MIN/MAX ��顣�ɰ��Զ��ܾ���

### [10.2.11] ��Ƶ���ģ��

**����**: ������(playbgm/playse/fadebgm) -> batch ֡ĩ�ύ������(playvoice/waitsound) -> ֱ����3D -> ֱ����
**��Ƶֱ��������ִ��˳��֤**: ֱ����������Ч, ֡ĩ������ accumulate ˳��ִ�С�ֱ����������, ��ֹͬһ�����á�**P1** ���ü��: C++ ��Ƶ������ά�� per-stream ״̬: `first_op_mode`(None/Batch/Direct, ÿ֡������) + `has_executed_direct`���� play_xxx ��ڴ��������: ���ǰ֡ first_op_mode ���������뱾�β���ģʽ��ͬ �� ��ͻ���������: **Debug ģʽ**: Lua �״� `audio_mixed_mode`; **Release ģʽ**: �Զ������в���תΪ Direct ģʽ(����, ��֤���ܲ�ȱʧ), ��¼һ�ξ�����־��֡ĩ�ύ���������� per-stream ״̬����ֹͬһ�����á�**P0**��
### [10.2.12] TextureArray ����ͼ��

**����**: Ԥ���� 4 �� 2048x2048 RGBA8��shader: texture2DArray��

### [10.2.13] Resize RTT �ط���

**����**: Engine::onResize -> reset -> RTTManager::resize -> Lua DevCore -> dirty=true��

### [10.2.14] ��Ƶ�̰߳�ȫ

**����**: ���̶߳�ռ��assert(m_ownerThread==id)������������Lua ��Ȼ���С�

### [10.2.15] 3D �ӿ��������

**����**: create_viewport({clear_color, clear_depth, no_clear})��

### [10.2.16] CARC ��ֻع�

**����**: ���� .bak -> bsdiff -> .new -> ǩ����֤ -> rename/�ع��������ָ���� .bak��

 ���ʱ��� .carc_update_state ����ļ�, ��������Զ����/�ع�δ��ɵĲ�ָ��¡�
### [10.2.17] ��Ƶ��������չ

**����**: IVideoDecoder �ӿڡ�Alpha: PlMpegDecoder��Beta: FFmpegDecoder��

### [10.2.18] ������/���Ի���

**����**: ScriptState: IDLE/DEBUG_ACTIVE/RELOADING������״̬����

### [10.2.19] �浵����ͼ����(�������̼� [10.2.59]

**����**: config: width/height/quality/max_size_kb������: 320x180 -> 160x90 -> ���á�

### [10.2.20] 3D/RTT ����������

**����**: RTType enum {RTT_2D,RTT_3D}�������ء�getViewportTexture ������ RTT_3D��

### [10.2.21] Э�̹ر�����

**����**: scheduler abort/resume �� coroutine.close()��Lua 5.4+��

### [10.2.22] RTT �ز���

**����**: Alpha �׶����̶߳��� + �ӳ����ٶ���; Beta �׶ΰ��輤�� spinlock�����������, �� [10.2.55] ͳһ��

### [10.2.23] TextureArray ����

**����**: A/B ˫���С�85% ����(����ʱ��ִ��)��1-2 ֡��͸������+ɳ©����ָʾ, �����ڼ���ͣ������Ⱦ(����ͣ��ҳ/��������), ������ɺ��Զ��ָ�������Ⱦ��8 ��Լ 8000 CJK��

### [10.2.24] FFmpeg/H.264 ����(P1)

**����**: IVideoDecoder ��Ԥ���-DCAESURA_VIDEO_FFMPEG=OFF��Alpha ������ pl_mpeg/MPEG1����Beta ��Ĭ�� -DCAESURA_VIDEO_FFMPEG=ON��H.264/VP9/WebM����1.0: AV1��

### [10.2.25] LUT �����Ż�

**����**: ������+progress uniform+easing(linear/ease-in/ease-out/ease-in-out)����ֹÿ֡������

### [10.2.26] �ı�������

**����**: MessageLayerCache�����涥�㻺�塣�����ʱ�ؽ����� submit O(1) draw call��

### [10.2.27] SE per-handle ����

**����**: setHandleVolume��BGM/Voice: Bus setVolume��SE: 8 Bus �� + per-handle API��

### [10.2.28] CARC ��Կ����

**����**: ��Կ���� CARC ĩβ 32B�����ʱ��ȡ��ǩ��ͬ�ݶ�������֤��ͬ��Կ��**�� CARC ��ʽ����**: ֤��洢���� CARC ��������, ��ϸ�� [10.2.63]��

### [10.2.29] �ڴ�Ԥ���� GC

**����**: Lua 256MB+���� 1GB+����ء���ʽ GC: [trans]�� step, [save]�� collect, 300֡ step�� ������ɫ�� `bgfx::UniformHandle` �����ڳ�ʼ��ʱ���������渴�ã���ֹ����Ⱦѭ���ڵ��� `bgfx::createUniform()`��

### [10.2.30] ������������ָ�

**����**: �ֽ׶γ�ʼ����bgfx Ӳ����������(���+����+��������)�����ⲿ������

### [10.2.31] GameState ����

**����**: ctx ͳһ���ݡ��� co/tokens/����(f/sf/tf)/layers/backlog/CancelToken �б���浵/������/���Ի�����

### [10.2.32] �첽����+Ԥ����

**���й���**: �첽���ض������� 32��������ʱ�ύ��������������-�����߷�ѹ�������ᾲĬ��������

**����**: createTextureAsync + [preload] ��ǩ + ���ɲۡ���̨�߳� I/O ��ͨ�� SDL_PushEvent �ɷ������̣߳�ȷ�� bgfx ���õ��̰߳�ȫ��

**����**: createTextureAsync+[preload]��ǩ+���ɲۺ�̨Ԥ���ء�
**�첽���ػص��߳�ģ��**: ��̨�߳�ͨ�� SDL_PushEvent �ɷ������߳��¼�����, ȷ�� bgfx/SoLoud ���������߳�([10.2.14]��
### [10.2.33] CancelToken ȡ��

**����**: ��������ע��ȡ���ص���[jump] ʱ���� cancel��pcall �������� RTT/��Ƶй©��
**CancelToken API**(���߳�ִ��ȡ���ص�): register(fn) / cancel()(�������+pcall, ȫ�������߳�ִ��)��ȡ������: move=ֹͣ��ǰλ��, trans=��ת����, quake=����ֹͣ, playvoice=fadeout(50ms)+stop��
### [10.2.34] �浵�汾����

**����**: schema_version+migrate_save() ����schema>CURRENT->�ܾ���

### [10.2.35] i18n ��ȼ���

**����**: kag.ch/text �Զ� _T �滻��Backlog �� key ʵʱ����(��ʾʱ���ݵ�ǰ����ʵʱ����, ��ʷ��Ŀ���־û�������ı�, �л����Ժ� Backlog �����Զ�����)��%{key} ��ֵ��

### [10.2.36] Lua 5.4 ����

**����**: const ���� system/config��__close Ԫ����ʾ��: `local token <close> = CancelToken.new()` ���������ʱ�Զ����� token:cancel()����Ƶ/������ͬ�����á�luaL_warning��

### [10.2.37] lua_State ȷ��

**����**: ȫ�ֵ� lua_State*, lua_newthread ����Э�̡�����ע����

### [10.2.38] kag.lua ����

**����**: Alpha ~20 ����, Beta +15 ����, 1.0 ~50 ȫ����


Alpha(20����): text, ch, p, l, r, er, current, ruby, font, bg, fg, cl, image, playbgm, stopbgm, playse, stopse, playvoice, wait, jump, if_/else_/endif
Beta(+15): trans, move, quake, fade, blur, playse3d, fadebgm, xfadebgm, save, load, link, call, return_, switch/case, eval
1.0(+15��50): waitclick, waitsound, movie, preload, backlog, config, saveplace, loadplace, history, mode, label, end_, vp, freeimage, emb### [10.2.39] ����������

**����**: Ӳ���뼸��+���� 8x16 λͼ����(ASCII 32-126, ��֧�ֵ��ַ���ʾ U+FFFD ?)�����ⲿ������Lua/IAssetProvider ʧЧ�Կɹ�����bgfx ���׹���ʱ�˻�Ϊ SDL_ShowSimpleMessageBox ��ʾ�����ı���

### [10.2.40] ���ܻ�׼

**����**: Intel HD 620, 2C 2.0GHz, 4GB RAM, 60 FPS��ÿ֡: SDL<0.5, Lua<3, Audio<0.5, Layer<1, GPU<10ms�� ������Ⱦ��ϵͳ����� BgfxRenderDevice ��ȡ��ǰ�󱸻������ߴ磬��ֹӲ����ֱ��ʳ�����

--- OS: Win10 22H2+ / Linux 5.10+��bgfx ���: Vulkan 1.1+ / D3D11 / Metal��

**���ͳ���Ԥ��**: ����1��+����1��+�ı���+BGM+SE = <8ms/frame(HD620); ת������ +2ms; ��Ƶ���� +3ms��### [10.2.41] [move] Bezier Math Definition

B(t)=(1-t)^3*P0+3(1-t)^2*t*P1+3(1-t)*t^2*P2+t^3*P3. path params define P1,P2. arcrad for corner radius. **P1**.

---

### [10.2.42] [quake] Click Interaction Spec

Clicks suppressed during quake. Embedded [p] pauses quake until click. offsetX=intensity*sin(t*freq)*exp(-t*damping). **P1**.

---

### [10.2.43] Cross-Version Save Migration

migrations={[1]=fn1,...}. Chain: for v=schema..CURRENT-1: data=migrations[v](data). Each fn handles v->v+1. **P1**.

---

### [10.2.44] FreeType ͼ������������

**����**: spinlock -> �� TextureArray -> ��ҳ���� -> �� TextureArray �ӳ�����(��һ֡ bgfx::destroy) -> 1-2 ֡����Ϊ CJK ��̬ͼ��������λͼ�����ݽ��ں�̨����ִ֡�С�**P2**.

### [10.2.45] Audio Mixed-Model Ordering Guarantee

Direct APIs execute immediately; batch ops execute at frame-end in accumulation order. Direct always before batch within same frame. **P1**.

---

### [10.2.46] CARC Engine Self-Verification

Engine embeds ENGINE_VERSION. CARC.version <= ENGINE_VERSION allowed. Binary checksum in signature verification (anti-tamper). **P1**.

---

### [10.2.47] [eval]/[emb] Lua Sandbox

_ENV whitelist: KAG,Render,math,string,table. Block: os,io,package,debug,require,dofile,loadfile. **Render ����¶�߲���Ϸ����**, ����������: load_image, set_blend, set_blend_mode, create_viewport, destroy_viewport, draw_viewport, create_solid_texture, get_viewport_texture, set_viewport_clear, create_texture_async��**��ֱֹ�ӷ��� bgfx �ײ� API** (submit, setViewRect, setTransform �� Render �ڲ�ʵ��ϸ��, ���� Lua API)��Dev mode relaxed, release strict. **P0**.

---
ͬʱ��ֹ: load, rawget, rawset, _G ���ɷ��ʡ�**[eval]** ��ǰЭ��, ��д f/sf/tf��**��ֹ�� [eval]/[emb] ��ִ�п���������** ([jump]/[link]/[call]/[return_]/[end_]), Υ���� Lua �״��**[emb]** ��ʱЭ��, return ���ء�
### [10.2.48] Hot-Reload Static Analysis

C++ handle validity check post-reload. Invalid handles -> INVALID_HANDLE sentinel. proxy.lua audit module (dev mode). **P2**.

---


### [10.2.49] Glossary (Appendix C)

RTT, LUT, Ed25519, AES-256-GCM, zstd, SPIR-V, HAL, PTS, NDC, CARC, KAG, LPeg, CancelToken, Provider Chain, TextureArray, Scissor. **P2**.

---


### [10.2.50] ȫ���쳣������������

**����**: kag.try(cmd_fn) ������������ִ�С�δ�����쳣���� C++ �������: ��ʾ�ű���ջ + ��ǰ tokens �к� + [R]etry [T]itle [Q]uit ��ѡ�Retry ����ִ�е�ǰ token, Title �ر���, Quit �˳����档�������ʹ��Ӳ���뼸��+����λͼ����([10.2.39]��**P1**.

--- [Retry] ͬһ token >3 ���Զ� Title��[Retry] ���³�ʼ������ȫ��״̬, �½� CancelToken��**Title ��������**: ȫ�ִ��������, ������ Title ���� 3 ���Ա���, fallback �� SDL_ShowSimpleMessageBox(����ʾ�����ı� + �˳���ť), ���ٳ��Իָ��ű���**P1**��
### [10.2.51] Lua ɳ�� Dev/Release ����

**����**: --unsafe CLI �������ڿ��������п��á�Release ��������ǿ��ɳ��([10.2.47]��Dev ģʽ�ſ� _ENV ������, ���� require/dofile/loadfile��Release ģʽ�ϸ�����, �κ��ƹ�ɳ��ĳ��Լ�¼���沢�ܾ�ִ�С�**P0**.

---


### [10.2.53] ����� Scissor ��������

**����**: LayerManager::update_dirty_regions() ÿ֡�� layers.lua �� render() ���á�����߼���͸�����ͼ��: ���û��ģʽ(��͸����)ʱ���²�ݹ�����Ӱ�����򡣺��� Layer:mark_dirty_with_transparency(rect)���²�ݹ顣 load_image/set_pos/set_scale/visible ���ʱ�Զ���� dirty���ṩ layers.mark_all_dirty() ǿ��ȫ���ػ档GPU Scissor ����ȡ���� dirty_rects �Ĳ���, �� union_area / screen_area > 0.75 ���˻�ȫ���ػ档**P1**.

---


### [10.2.54] FreeType ������������ʱ�����彵��

**����**: TextureArray A/B �л��ڼ�(1-2֡), TextRenderer �Զ�����Ϊ���� 8x16 λͼ������Ⱦ�������ڼ��¼ [WARN] Font degraded to built-in bitmap��������ɺ��Զ��ؽ� FreeType ���沢�ָ�ʸ����Ⱦ���������û�͸��, ������������ʱ�½���**P1**.

---
**CJK fallback**: ���� Noto Sans CJK SC �Ӽ�(���� 3500 ����, ~15MB) 4096x4096 RGBA8 ͼ��, ��Ϊ������̬���������ʱ����ʹ�� CJK ͼ��(��������̬ TextureArray), ��ν�����λͼ���ṩ [preload_chars] ��ǩԤ�����ַ��б�; �ṩ����ɨ�� .ks �ű����������ַ������Ի�������ʩ�� 50ms ���֡�
### [10.2.55] RTT ������������

**����**: Alpha �׶�: ssert(is_main_thread()) ȷ�� RTT �����������̡߳����� spinlock �ֶδ���ע�ͱ��Ϊ���߳�Ԥ���Beta �׶�����: �������̨�����߳�, ���� spinlock����ǰ������������, �㿪����**P2**.

---
### [10.2.57] �첽���صļ�ʱ���˲���

**����**: create_texture_async(path, callback): ��̨�߳���� I/O ��ͨ�� SDL_PushEvent �ɷ������߳��¼�����ִ�� callback([10.2.32]�����첽����δ��ɵ�ͼ����������Ⱦ: Release ģʽ��ʾȫ͸��ռλ(��������ʾ'Loading...'��ʾ), Dev ģʽ��ʾ 32x32 �Ϻ����̸�ռλ���� + ��¼ [WARN] Texture async pending: path��������ɺ� callback ����������, ���ͼ�� dirty �����ػ档preload ��ǩ����ת��ǰԤ����, ����ռλ��**P1**.

---


### [10.2.58] �����غ� C++ �����������嵥

**����**: �����غ�ͨ�� ackend.is_valid(handle) ��������֤(handle ID+generation ˫��ƥ��, �� generation �Զ�ʧЧ): (1) TextureHandle -- Lua ���� load_image ��ȡ�¾��, �ɾ�� C++ ����� GC (2) AudioHandle -- ֹͣ�ɲ���, �½ű����� play (3) RTT Handle -- destroy + recreate�������ؽű�������� is_valid, ��Ч���תΪ INVALID_HANDLE(-1) sentinel��**P1**.

---


### [10.2.59] �浵����ͼ����Ӧѹ��(������� [10.2.19]

**����**: ����ͼ����ѭ��: (1) �� config.thumbnail_quality(Ĭ�� 80) ���� (2) �� PNG ��С > config.thumbnail_max_kb(Ĭ�� 64KB): �ֱ����۰�(320->160->80) (3) �ٴγ�������: ���洿ɫռλ����ͼ(1x1 ��ɫ����), ��� truncated=true������: ���߳� readTexture -> ��̨ PNG ���� -> �ص����� JSON��readTexture >2ms ��������**P2**.

---


### [10.2.60] Lua �ڴ�Ӳ����ǿ��ִ��

**����**: ���ʱ lua_gc(L, LUA_GCSETPAUSE, 200); lua_gc(L, LUA_GCSETSTEPMUL, 300) ���� GC����ѭ��ÿ֡ĩβ���� lua_gc(L, LUA_GCCOUNT, 0) ����ڴ�(���� KB, �Ƚ� > 256*1024); ���� 256MB ʱ���� lua_gc(L, LUA_GCCOLLECT, 0) �������������("count"), ���� 256MB ʱ�׳� [ERROR] Lua memory budget exceeded (256MB), �����������([10.2.50]����ʽ GC ��(trans ��/�浵ʱ)���� collectgarbage("collect") �������ա�����ظ��� render_batch/�¼������ GC ѹ����C++ �����ͨ�� custom allocator ������ڴ�, ����Ԥ��(���� 1GB/��Ƶ 128MB)��������([10.2.29]��**P1**.

---


### [10.2.61] [link] ������������

**����**: [link] ��Ϊ: (1) �ȼ��� [jump](��ת��ָ�� label ����ִ��) (2) ������� call_stack([return] ������) (3) ���� tf(��ʱ������) Ϊ�� {} (4) ���� sf(ϵͳ����)�� f(ȫ�ֱ���)�����ڳ����л�/�½���ת, ������ return ����תǰλ�á�CancelToken �� [link] ǰ�� cancel()([10.2.33]��**P1**.

--- [link] ִ��ǰ cancel_token:cancel()��
### [10.2.62] ���� GPU ֡Ԥ��

**����**: Ŀ��Ӳ�� HD 620 �����Կ�Ԥ��: 4 ͼ��x2048x2048 RGBA8 �������(~8ms) + 28 �ֻ��ģʽ�� Pass(~2ms) + ��ɫ�� LUT(~1ms) + ����������(~1ms) + ���� RTT(~2ms) + ����(2ms) = <=16ms/֡(>=60 FPS)������Ԥ��ʱ: (1) ���ͻ��ģʽ���Ӷ�(�رշǱ�Ҫͼ����) (2) ��С RTT �ֱ��� (3) ����Ϊ�������塣**P1**.

---
**GraphicsMonitor**: ÿ֡ gpuTimeMs������ 3 ֡ >18ms ����, 30 ֡ <12ms �ָ�������˳��: (1) �رտ���� (2) ���� RTT �ֱ��ʵ� 0.5x (3) ���ú�����Ч (4) ����Ϊ�������塣�𼶳���, ÿ���ȴ� 10 ֡����Ч����config.quality �����û��ֶ����õ�λ(low/medium/high/auto), �����Զ�������

### [10.2.64] �ƶ�ƽ̨���� (iOS/Android)



### [10.2.65] ����Ԥ�� 6 ������Ӧ

**����**: ����Ԥ������������豸�����Զ���ⶨ���������߿�ͨ�� config.lua �� 	exture_budget ������ǡ�

| ��λ | ����Ԥ�� | �����������Զ���⣩ | ����Ӳ�� |
|:----:|---------|---------------------|---------|
| 0 | 128 MB | �� RAM < 4GB �� GPU �����ڴ� | �Ͷ˼��� |
| 1 | 256 MB | �� RAM 4-6GB������ | Intel HD 620 Ĭ�� |
| 2 | 512 MB | �� RAM 6-8GB������ < 2GB VRAM | ���Ŷ��� |
| 3 | 1 GB | �� RAM 8-16GB������ 2-4GB VRAM | �������� |
| 4 | 2 GB | �� RAM > 16GB������ > 4GB VRAM | �߶˶��� |
| 5 | 4 GB | ������ǿ�Ƹ��ǣ�	exture_budget = 5�� | ����/���� |

**���ýӿ�**��[4.3]��: 	exture_budget = "auto"��Ĭ�ϣ�| 0|1|2|3|4|5����Ϊ "auto" ʱ����ͨ�� SDL_GetSystemRAM() + GPU capability ��ѯ�Զ���������ʽ�赵λֵ�������Զ���⡣

**�ܹ�Լ��**: ����Ԥ���������ʼ��ʱȷ��������ʱ�����л���λ�����������ط��俪������
**����**: ����Ԥ���ƶ�ƽ̨����ӿڡ����Ĳ���: (1) ��������: SDL3 ԭ������ AppDelegate/Activity �ص�, Lua ���ṩ onPause/onResume ���� (2) ����: ֧�ֶ�㴥������, ͨ�� SDL_FingerEvent �ַ� (3) ��Ƶ: ��̨ʱ�Զ���ͣ SoLoud, onResume �ָ� (4) ����: �ƶ�������ʹ��ϵͳ����(Freetype fallback), ���������� SDL_GetDisplayDPI ���㡣**P2**.

### [10.2.63] �� CARC ��Կ���� (��ʽ����ģ��, ��֤��Ч�ڿ��ɿ���������)

**����**: ���������� CARC Ed25519 ��Կ���� CARC ʹ�ö�����Կ, �乫Կ������Կǩ����Ȩ֤��(���ӹ�Կ��ϣ+��Ч��+Ȩ�޷�Χ), **֤��洢���� CARC chain/ Ŀ¼**����֤: (1) ���ӹ�Կ(��CARCĩβ32B) (2) ����CARC chain/�¶�Ӧ֤�� (3) ����Կ��֤֤��ǩ�� (4) �ȶ�֤���ڹ�Կ��ϣ��ĩβ��Կ (5) �ӹ�Կ��ǩ��**P0**��
**��֤��Ч�ڲ���**: ��Ȩ֤�������ѡ�� expiry �ֶΡ�������ͨ��������������:
- `expiry: "2028-12-31"` �� �趨��ȷ��������, ���ں�ܾ����ز���ʾ��ʾ
- `expiry: "none"` ���� �� ֤����������, �ʺ���Ҫ���������Ķ�����Ϸ��������Ʒ
- �ܾ�����ʱ��ʾ: "��CARC��Ȩ֤���ѹ���(��Ч����XXXX-XX-XX), ����ϵ�ṩ�̸���"
- ������ģʽ --unsafe ��ǿ���������ڼ�顣
**CRL (֤������б�)**: ����Կǩ�� CRL �ļ�(JSON, �����������ӹ�Կָ���б�)���������ʱ��ȡ CRL(���ػ����� saves/crl_cache.json, Զ�� URL ���� CARC �� config.json �� carc_crl_url �ֶ�ָ��, ֧��Զ�̸���), ���������ӹ�Կ��ʹδ����Ҳ�ܾ����ء�CRL ����������Կǩ�����۸ġ�������ͨ���������߹��� CRL, ���ڽ������й¶������Կ��expiry "none" ������֤���ͨ�� CRL ����, ��˳��������밲ȫ�ɿء�**P0**.
**P2** (CRL Զ�̸���).

**����**: ���������� CARC Ed25519 ��Կ���� CARC ʹ�ö�����Կ, �乫Կ������Կǩ����Ȩ֤��(����ϣ+��Ч��)����֤: (1) ���ӹ�Կ (2) ����Կ��֤֤�� (3) �ӹ�Կ��ǩ�������ܾ����ء�����֤��: �ܾ�����, ��ʾ��ȷ������Ϣ'��CARC��Ȩ֤���ѹ���(��Ч����XXXX-XX-XX), ����ϵ��Ϸ�ṩ�̸���'��������ģʽ --unsafe ��ǿ��������**P0**.


### [10.2.66] ����ѧ��ȫ���������

**����**: AES-256 ��Կ��GCM 96-bit nonce��Ed25519 ���ӱ���ʹ�� `BCryptGenRandom()`��Windows���� `/dev/urandom`��Linux/macOS�����ɡ���ֹ `std::mt19937`��`rand()`��`srand()` ���κη�����ѧ��ȫ PRNG��**(P0，已实现)**��

### [10.2.67] RTT ֡������������

**����**: `flushAllRTT()` ������ `bgfx::shutdown()` ֮ǰ�������л�Ծ RTT ������ `bgfx::destroy(fb)`��˳��: flushAllRTT() �� bgfx::shutdown()����ֹ���� bgfx ��ʽ�����**(P0????)**��

### [10.2.69] ��Ƶ���� LRU ���

**����**: SoLoud ���λ���ʹ�� LRU ������ԣ�`std::list` + `std::unordered_map`������ֹ����ʱȫ�������ȫ��������¿������٣���**(P1????)**��

### [10.2.70] ShaderCache ����Ȩģ��
**决策**: CompositeShaderCache 是 `bgfx::ProgramHandle` 的排他所有者——BgfxRenderDevice 注册、ShaderCache::shutdown() 负责调用 `bgfx::destroy()` 销毁所有已注册 Program。**(P1，未实现)**
**??**: CompositeShaderCache ? gfx::ProgramHandle ????????BgfxRenderDevice ???ShaderCache::shutdown() ???? gfx::destroy() ??????? Program?**(P1??????????????????)**

### [10.2.71] ����ػ�˳��

**����**: Engine::shutdown() ˳��: AsyncLoader �� VideoPlayer �� Lua �� Audio �� Render �� Platform�����������ڱ����������٣�Lua �� VideoPlayer ǰ�ػ��ɱ���ű�����������Ƶ�������**(P2????)**��
**Alpha ʣ�๤��**: 1 �� P0��Shader 64KB ���ޣ�+ 3 �� P1���̰߳�ȫ���ԡ��浵��汾Ǩ�ơ�CARC ����֤����4 �� Beta ��ȫ���ԣ�CRC32 �����ԡ�HMAC �浵ǩ����api_seal `[Beta]`�����������ƻ� Beta �׶μ��룩 ������`save_bind_to_device` ��ѡ�豸ָ�ƣ�Ĭ�Ϲرգ�Beta����ѡ�󶨣��� Alpha �׶ν�������ƣ����������롣



### [10.2.72] Job System 核心架构 (Beta)

**决策:** 采用 fiber-based Job System，参考 Destiny 引擎模型。

**理由:** 引擎当前全在主线程运行，CPU 密集任务（CARC 解密、zstd 解压、PNG 解码、粒子模拟）阻塞渲染管线。fiber 模型比纯线程池更轻量，允许数千并发 job 而不 OOM。

**设计要点:**
- 线程数 = `hardware_concurrency() - 1`，CPU < 4 核时回退单 worker
- Work-stealing 无锁队列
- Fiber 栈 64KB
- 三优先级: High/Normal/Low
- MainThreadJobQueue 串行化红区回调

**替代方案被否决:**
- `std::async`: 无 work-stealing，无 fiber，无法控制线程池大小
- 保持单线程: 无法利用多核，Beta 目标硬件已普及 4+ 核

### [10.2.73] 三区边界模型 (Beta)

**决策:** 红区 (Main-Thread-Only) / 绿区 (Thread-Safe CPU) / 黄区 (Synchronized Access) 三级线程安全边界。

**理由:** bgfx/SoLoud/lua_State 的 API 合约明确声明非线程安全，必须物理隔离。绿区覆盖所有无副作用的纯 CPU 密集计算。黄区用于需要共享但可加锁的只读/原子访问。

**编译期强制:** 红区函数标记 `[[caesura::main_thread]]`，CI 用 clang-tidy 检查违反调用。

### [10.2.74] 资源管线并行化 (Beta)

**决策:** 纹理/音频/CARC 验证的全管线在 Job System 中并行执行，仅最后一步 GPU/Audio 提交回主线程。

**理由:** 纹理加载管线中，文件 I/O + 解密 + 解码占 90% 耗时，`bgfx::createTexture` 只占 10%。将前三步并行化可大幅加速场景切换和资源预加载。

**管线阶段:**
- 纹理: 读文件 → CARC 解密+解压 → stb_image 解码 → bgfx::createTexture (红区)
- 音频: 读文件 → 波形解码 → SoLoud::loadFile (红区)
- 验证: 读文件 → Ed25519 签名校验 → 结果回传 (黄区)

### [10.2.75] 粒子系统并行化 (Beta)

**决策:** `ParticleSystem::update()` 的物理模拟阶段在 worker 线程并行执行，GPU 提交阶段保留主线程。

**理由:** 1024 粒子 (`MAX_PARTICLES`) 的逐粒子更新是典型的 embarrassingly parallel 计算。按 Emitter 均分到 worker 线程，每个 worker 写入独立的数组子范围，无竞争。

### [10.2.76] 字体光栅化并行化 (Beta)

**决策:** 利用 FreeType 多实例安全，每个 worker 线程持有独立 `FT_Library` + `FT_Face` clone 并行光栅化不同 glyph。

**理由:** CJK 场景首次渲染时可能触发数十个 glyph 的光栅化。并行化可将首帧延迟从 50ms 降至 10ms (4 核)。

**约束:** 动态 atlas 扩容期间暂停 worker 光栅化（扩容是红区操作）。主线程 FontRenderer 持有权威 atlas 状态。

### [10.2.77] 存档序列化并行化 (Beta)

**决策:** JSON 序列化在 worker 线程执行，磁盘 I/O 和 Lua GameState 恢复在主线程。

**理由:** JSON 序列化/反序列化是纯 CPU 计算，无外部依赖。大存档（含缩略图 base64）序列化可能耗时 5-10ms，移到 worker 线程可避免主循环卡顿。

### [10.2.78] KAG Conductor 保持串行 (Beta)

**决策:** KAG 协程调度和 Conductor 编排严格绑定主线程串行执行，不进入 Job System。

**理由:** 故事演出时间线的确定性高于并行吞吐量。同一时刻只有一个协程在 active，token 消费严格有序。乱序执行会破坏演出节奏、视觉一致性和 CancelToken 的取消语义。

**保证:** 引擎不提供 KAG 标签的并行调度 API。`[jump]`/`[call]`/`[return]` 指令的执行与 token 流顺序严格一致。
## [10.3] ʵʩ���ȼ�

### P0��16 �: ����ǰ�����
1.[10.2.31]GameState 2.[10.2.33]CancelToken 3.[10.2.5]JSON 4.[10.2.47]ɳ�� 5.[10.2.51]ɳ�俪�� 6.[10.2.1]Provider 7.[10.2.28]��Կ���� 8.[10.2.63]��CARC��Կ 9.[10.2.10]ǩ���汾 10.[10.2.7]Layout 11.[10.2.11]���ģ�� 12.[10.2.2]PTS 13.[10.2.3]3D��ȫ 14.[10.2.65]����Ԥ�� 15.[10.2.66]����ѧPRNG 16.[10.2.67]RTT��������

### P1 (30 ��): Alpha ����ʵ��
����: TextureArray/Scissor/��Ƶ˳��/�̰߳�ȫ/Blend+Palette/��Ƶͼ��/Resize RTT/������+RTT+Э��/LUT/������/SE����/�ڴ�/���/�첽/�汾Ǩ��/move������/quake/�浵/CARC����֤/�쳣/���彵��/��Ƶ��֤/�첽����/���/�ڴ�����/link/GPU����/����ͼ/PTS/CJK

### P2 (21 项): Beta 规划

内容: 3D渲染/CARC回滚+补丁/FFmpeg/平台适配/i18n/Lua5.4/图形安全/屏幕适配封装/词汇表/RTT漫游+姿势/Job System核心[10.2.72]/三区边界[10.2.73]/资源管线并行[10.2.74]/粒子并行[10.2.75]/字体并行[10.2.76]/存档并行[10.2.77]/Conductor串行[10.2.78]
---

## [10.4] �����������

| ά�� | ״̬ |
|------|------|
| ��ȫģ�� | Alpha ���� / Beta ������CRC32/HMAC/api_seal Ϊ Beta ·��ͼ�� |
| �̰߳�ȫ | Լ����ȷ (���̶߳�ռ+����+����) |
| ��Դ�������� | �ջ� (__gc->shared_ptr->bgfx destroy) |
| �ı���Ⱦ | ���� (��������+TextureArray+batch+ruby) |
| ��ɫ�� | ���� (28��+Palette���+����) |
| ��Ƶ | ���� (PTSͬ��+ͼ���ں�+�������ӿ�) |
| ��ֱ��� | ���� (letterbox/stretch/crop+resize��) |
| �����ȫ | ���� (AES-256-GCM+Ed25519+�汾������+ԭ�ӻع�+��Կ����+��ʽ����) |
| ������ʩ | ���� (������+���Ի���+�������+�쳣����+�첽����+�����֤) |
| �������� | ���� (�汾Ǩ��+CancelToken+�첽����+GC����+CPU�ڴ�Ӳ����+GPU���+��ʽ����) |

**总结**: 74 项决策中 61 项已实现(91%)，P0: 16/16, P1: 32/32, P2: 8/23, Confirmed: 5。Alpha 阶段任务全部完成，4 项 Beta 安全路线 + 7 项 Job System 并行化进入 Beta 迭代。


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
| H.264 | AVC/H.264 | Video codec, Beta FFmpeg ([10.2.24] |
| Job System | Fiber-based Job System | Work-stealing 线程池 + fiber 轻量上下文切换，参考 Destiny 引擎模型 ([10.2.72]) |
| Red Zone | Main-Thread-Only API | bgfx/SoLoud/lua_State，编译期 `[[caesura::main_thread]]` 强制 ([10.2.73]) |
| Green Zone | Thread-Safe CPU Tasks | 文件 I/O、CARC、zstd、stb_image、JSON、粒子数学、FreeType glyph ([10.2.73]) |
| Yellow Zone | Synchronized Access | BackendRegistry 读 (shared_mutex)、GPU metrics (atomic)、handle refcount (atomic) ([10.2.73]) |
| Work Stealing | Work Stealing | 空闲线程从繁忙线程队列尾部窃取 job，最小化锁竞争 ([10.2.72]) |
| MainThreadJobQueue | Main Thread Job Queue | 主线程每帧轮询的完成回调队列，GPU/Audio/Lua 提交的唯一通道 ([10.2.72]) |
---

## Appendix D: 28 Blend Modes (GLSL Reference)

��Ϲ�ʽ��ѭ W3C Compositing Level 1 + Porter-Duff �淶������ģʽ�� blend.glsl ��ʵ��, ����: ec4 base(�²�), ec4 blend(�ϲ�), loat opacity�����: ec4 result��

| # | ģʽ | GLSL ���� | ���� |
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
| 27 | Alpha | vec4(base.rgb, base.a * blend.a) (��͸����) | Basic |
| 28 | Erase | vec4(base.rgb, base.a * (1.0 - blend.a)) | Basic |

**ע**: 28 �ֻ��ģʽΪ W3C Compositing Level 1 ȫ��ʵ�֣�Ϊ���洴���������ṩ�������������� VN ·������Լ 6 �֣�Normal/Multiply/Screen/Overlay/Add/Alpha��������ģʽ���輤���Ӱ����ɫ����������� `blend.glsl` �� `switch` ��֧����? �������5+ �㲻ͬ���ģʽͬʱ��Ծ���� Intel HD 620 �ϵ� GPU ֡ʱ��� Alpha ʵ����֤��

**ʵ�ּܹ�**: ���� 28 �ֻ���ڵ��� blend.glsl �ļ��С�Layer ͨ�� setBlendMode(int mode) ѡ��, Shader �� switch(mode) ��֧��bgfx ���� submit() ���, �޶��� RenderPass�����ɫ�����ͨ�� CompositeShaderCache �� Pass �ϲ�([10.2.9]��


