# CARC 打包指南

## 概述

CARC (Caesura ARChive) 是 Caesura (AmeKAG) 引擎的加密压缩归档格式。它用于将游戏资源打包为单个文件，支持：

- **AES-256-GCM 加密**: 可选的文件级加密，保护游戏资源
- **zstd 压缩**: 高效的字典式压缩，平衡速度与压缩率
- **Ed25519 签名**: 可选的数字签名，防止资源篡改

## 命令行工具

### 基本用法

工具位于构建输出目录 `bin/Debug/carc_pack.exe`（Release 为 `bin/Release/carc_pack.exe`），支持 `pack`（默认）、`list`、`extract` 三种操作：

```bash
CARC_PACK=bin/Debug/carc_pack.exe

# 打包（无加密）
$CARC_PACK ./mygame ./release/game.carc

# 打包 + 生成密钥对（保存公钥/私钥文件）
$CARC_PACK ./mygame ./release/game.carc ./mygame/game.key.pub ./mygame/game.key

# 使用已有密钥打包
$CARC_PACK ./mygame ./release/game.carc
# (密钥文件 game.key 和 game.key.pub 需放在工作目录)

# 列出归档内文件（每行一个路径哈希，供脚本/导入器消费）
$CARC_PACK list ./release/game.carc [public.key]

# 提取单个文件到原始相对路径（需 --path 指定已知路径）
$CARC_PACK extract ./release/game.carc ./out --path audio/bgm_01.mp3 [public.key]

# 全量提取（归档只存路径哈希，全量提取按哈希名落盘）
$CARC_PACK extract ./release/game.carc ./outfull [public.key]
```

### 参数说明

```
carc_pack.exe <input_dir> <output.carc> [public.key] [private.key]
carc_pack.exe list <archive.carc> [public.key]
carc_pack.exe extract <archive.carc> <out_dir> [--path <rel>] [public.key]

  pack        — 默认操作。打包目录
  input_dir   — 要打包的目录
  output.carc — 输出的 CARC 归档文件
  public.key  — (可选) Ed25519 公钥路径（list/extract 用于验签）
  private.key — (可选) 保存 Ed25519 私钥的文件路径
  list        — 打印归档内文件的路径哈希（每行一个，machine-readable）
  extract     — 提取归档内容到 out_dir
  --path      — (extract) 仅提取指定相对路径到一个文件（保留原始文件名）
```

### 密钥管理

- **公钥** (32 字节): 嵌入到引擎中，用于运行时验证签名和解密
- **私钥** (64 字节): 仅用于打包时签名，**不应分发**
- 密钥文件为原始二进制格式

## CARC 文件格式

```
┌─────────────────────────────────────────┐
│  Header (128 bytes)                     │
│  ├─ Magic: "CARC" (4 bytes)             │
│  ├─ Version: uint32                     │
│  ├─ Compression: enum (zstd=1)          │
│  ├─ Flags: uint32 (encrypted, signed)   │
│  └─ Reserved                           │
├─────────────────────────────────────────┤
│  Index (加密，如启用)                    │
│  ├─ File count: uint32                  │
│  └─ For each file:                      │
│      ├─ Path hash: uint64               │
│      ├─ Offset: uint64                  │
│      ├─ Size: uint64                    │
│      └─ Original size: uint64           │
├─────────────────────────────────────────┤
│  Body (加密+压缩，如启用)                │
│  └─ zstd 压缩的数据块                    │
├─────────────────────────────────────────┤
│  Footer (128 bytes)                     │
│  ├─ Ed25519 Signature (64 bytes)        │
│  ├─ Public Key (32 bytes)               │
│  └─ Reserved                           │
└─────────────────────────────────────────┘
```

## 在 KAG 脚本中使用

Caesura 引擎**没有 `carc://` 协议**。资源请求统一走纯路径：打包时的相对路径即脚本中的资源路径，引擎按优先级依次在各资源提供者（磁盘目录、`data.carc`/`game.carc`/`patch.carc`）中查找同名路径。

```kag
; 背景图片
@bg "scenes/bg_classroom.png"

; 背景音乐
@bgm "audio/bgm_01.mp3"

; 角色立绘
@fg "characters/hero_smile.png"

; 音效
@se "audio/se_click.wav"
```

## 运行时加载

引擎在组合根的 `registerDefaultAssetProviders`（`src/entry/Engine_Assets.cpp`）中，按 `data.carc`、`game.carc`、`patch.carc` 的顺序自动探测并注册存在的归档作为 `CarcAssetProvider`（纯路径语义，见上节）：

```cpp
// src/entry/Engine_Assets.cpp（组合根，Engine 启动时自动处理，无需游戏代码手动注册）
void registerDefaultAssetProviders(AssetManager& assetManager) {
    const char* carcFiles[] = {"data.carc", "game.carc", "patch.carc"};
    for (const char* fname : carcFiles) {
        auto reader = std::make_unique<carc::CARCReader>();
        if (reader->open(fname)) {
            assetManager.addProvider(
                std::make_unique<carc::CarcAssetProvider>(std::move(reader)));
            printf("[Engine] Registered CARC: %s\n", fname);
        }
    }
}
```

`assetManager` 由 `Engine` 独占持有并注入组合根辅助函数，不通过全局单例访问。`ProviderChain` 按优先级依次在磁盘目录与已注册的 CARC 提供者中查找同名路径。

## 安全注意事项

1. **私钥绝不提交到版本控制** — 将 `*.key` 添加到 `.gitignore`
2. **公钥嵌入引擎** — 运行时验证签名使用
3. **AES-256-GCM 提供认证加密** — 同时保护机密性和完整性
4. **Ed25519 签名在加密之上** — 防止恶意资源替换

## 常见问题

**Q: 打包后文件比原始文件大？**
A: 检查是否包含了不应打包的文件（如 `.git/`, `.DS_Store`）。仅打包 `assets/` 和 `scripts/` 目录。

**Q: 运行时提示 "CARC signature verification failed"？**
A: 公钥不匹配或归档文件被修改。确认使用的公钥与打包时的私钥配对。

**Q: 能否增量更新 CARC？**
A: 当前不支持增量更新。引擎侧 `DeltaCARC`（`src/archive/DeltaCARC.*`）差异更新实现已完成（文件级 diff，AES-256-GCM 加密整条 delta，按源/目标 SHA 绑定验证），但**尚未接入组合根或 `carc_pack` 命令行工具**——需额外集成方可在实际流程中使用。
