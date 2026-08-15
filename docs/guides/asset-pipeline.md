# 资源管线 (Asset Pipeline)

Caesura (AmeKAG) 引擎的资源管线负责加载、解码和管理游戏所需的所有外部资源。

## 图片

引擎使用 stb_image 库解码图片，支持以下格式：

| 格式 | 支持 | 透明通道 | 推荐用途 | 备注 |
|------|------|---------|---------|------|
| PNG | ✅ | ✅ (RGBA) | 立绘、UI 元素 | 推荐格式，支持无损压缩 + alpha |
| JPG/JPEG | ✅ | ❌ (RGB) | 背景图片 | 有损压缩，文件小，适合大尺寸背景 |
| TGA | ✅ | ✅ | 纹理资源 | 支持 RLE 压缩 |
| BMP | ✅ | ❌ | — | 无压缩，不推荐 |
| PSD | ✅ | ✅ | — | Photoshop 原生格式，开发期可用 |
| GIF | ✅ | ✅ | — | 静态 GIF 支持，不支持动画 |
| HDR | ✅ | N/A | HDR 光照贴图 | 高动态范围 |
| PIC | ✅ | — | — | Softimage PIC 格式 |
| PNM | ✅ | — | — | Netpbm 格式 (PBM/PGM/PPM) |

### 纹理限制

| 属性 | 限制 |
|------|------|
| 最大分辨率 | TextureBudget 分级控制 (128MB–4GB) |
| 最大单纹理尺寸 | 硬件相关 (通常 4096×4096 或 8192×8192) |
| 推荐立绘尺寸 | 1024×2048 (全身) / 1024×1024 (半身) |
| 推荐背景尺寸 | 1920×1080 |
| 推荐 UI 元素 | 按实际显示尺寸，不超过 512×512 |

## 音频

引擎使用 SoLoud 音频引擎，支持以下格式：

| 格式 | 支持 | 推荐用途 | 备注 |
|------|------|---------|------|
| WAV | ✅ | 音效 (SE)、语音 (Voice)、BGM | 无损，推荐用于音效和语音 |
| FLAC | ✅ | 高质量 BGM | 无损压缩，文件比 WAV 小 |
| MP3 | ✅ | 压缩 BGM | 有损压缩，文件最小 |
| OGG (Vorbis) | ✅ | 压缩 BGM | 开源有损格式 |

### 音频参数建议

| 用途 | 采样率 | 位深 | 声道 | 推荐格式 |
|------|--------|------|------|---------|
| BGM | 44100 Hz | 16-bit | 立体声 | MP3 192kbps / OGG q5 |
| 语音 | 22050 Hz | 16-bit | 单声道 | WAV |
| 音效 | 44100 Hz | 16-bit | 单声道/立体声 | WAV |

### 音频总线

引擎有三条独立的音频总线：

| 总线 | Lua API | 用途 | 同时播放数 |
|------|---------|------|-----------|
| BGM | `KAG.play_bgm` / `KAG.stop_bgm` | 背景音乐 | 1 |
| Voice | `KAG.play_voice` / `KAG.stop_voice` | 角色语音 | 1 |
| SE | `KAG.play_se` / `KAG.stop_se` | 音效 | 多个 |

## 视频

视频解码默认启用 FFmpeg（CMake 选项 `CAESURA_ENABLE_FFMPEG`，默认 `ON`）。**当编译时找到 FFmpeg（`external/ffmpeg` / vcpkg / pkg-config）时，引擎通过 FFmpeg 支持其可探测的全部容器与编码格式**（MP4/H.264、HEVC、VP9、WebM、MKV、MPEG-1/2 等），并启用硬件解码与 SIMD 加速、含音频轨。`pl_mpeg` 仅作零依赖回退：只在未编译 FFmpeg、或 FFmpeg 打开失败时，回退到 **MPEG-1 专用**解码。

| 解码器 | 支持范围 | 备注 |
|--------|---------|------|
| FFmpeg（首选，`CAESURA_ENABLE_FFMPEG=ON` 且找到 FFmpeg） | FFmpeg 支持的全格式（MP4/H.264、HEVC、VP9、WebM、MKV、MPEG-1/2 等） | 硬件解码 + SIMD；含音频轨（重采样为浮点立体声 PCM） |
| pl_mpeg（回退） | 仅 MPEG-1 | FFmpeg 未启用或打开失败时回退；帧经 `bgfx` 上传为纹理 |

> 若需固定 MPEG-1 兼容性（如目标平台不随附 FFmpeg），可 `-DCAESURA_ENABLE_FFMPEG=OFF` 强制走 pl_mpeg 回退，并预先用 FFmpeg 将素材转码为 MPEG-1。

## 推荐目录结构

```
assets/
├── images/
│   ├── bg/          # 背景图片
│   │   ├── classroom.png
│   │   └── hallway.jpg
│   ├── fg/          # 前景/立绘
│   │   ├── hero_normal.png
│   │   ├── hero_smile.png
│   │   └── heroine_default.png
│   └── ui/          # UI 元素
│       ├── dialog_box.png
│       └── choice_button.png
├── audio/
│   ├── bgm/         # 背景音乐
│   │   ├── title.mp3
│   │   └── peaceful_day.ogg
│   ├── se/          # 音效
│   │   ├── click.wav
│   │   └── door_open.wav
│   └── voice/       # 语音
│       ├── hero_001.wav
│       └── heroine_001.wav
└── video/           # 视频（FFmpeg 支持任意容器，如 .mp4/.mkv/.webm）
    ├── opening.mp4   # H.264
    └── ending.mkv
```

## 资源预加载

异步预加载通过 KAG 标签 `[preload]` 声明（见 `scripts/kag/commands/resource.lua`），底层由 C++ 侧 `AsyncLoader` + `JobSystem` 在工作线程执行 IO/解码，完成后回调通知主线程；Lua 侧管理缓存与占位纹理回退。

```ks
; 预加载图片到纹理缓存（异步，先返回占位纹理）
[preload type="texture" path="images/bg/school.png" wait="false"]

; 同步等待全部加载完成后再继续
[preload type="texture" path="images/bg/a.png,images/bg/b.png" wait="true"]

; 预加载音频 / 场景
[preload type="audio" path="audio/bgm_01.mp3"]
[preload type="scene" path="act2.ks"]
```

- `type` — `texture` / `audio` / `scene`
- `path`（或 `storage`）— 逗号分隔的资源路径（相对游戏根目录）
- `wait` — `"true"` 同步阻塞协程直至加载完成；`"false"` 后台加载，提前使用时显示占位纹理（开发紫色 / 发布灰色）

加载状态由 `kag.commands.resource` 的 `is_loaded` / `is_pending` / `flush_cache` 函数管理（供其他 KAG 命令内部调用，并非 `KAG.*` 直接绑定）。
