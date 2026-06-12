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

| 格式 | 支持 | 分辨率限制 | 备注 |
|------|------|-----------|------|
| MPEG1 | ✅ | 1920×1080 | 基本视频播放支持 |
| 其他格式 | ❌ | — | 可通过 FFmpeg 预处理转换为 MPEG1 |

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
└── video/           # 视频
    ├── opening.mpg
    └── ending.mpg
```

## 异步加载

引擎通过 `AsyncLoader` 支持资源的异步加载：

```lua
-- 预加载图片到缓存
KAG.preload("images/bg/school.png", "texture")

-- 查询加载状态
if KAG.is_loaded("images/bg/school.png") then
    -- 资源已就绪
end
```

异步加载通过 `JobSystem` 在工作线程上执行 IO 和解码，完成后通过回调通知主线程。
