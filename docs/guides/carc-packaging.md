# CARC 打包指南

## 概述

CARC (Caesura ARChive) 是 Caesura (AmeKAG) 引擎的加密压缩归档格式。它用于将游戏资源打包为单个文件，支持：

- **AES-256-GCM 加密**: 可选的文件级加密，保护游戏资源
- **zstd 压缩**: 高效的字典式压缩，平衡速度与压缩率
- **Ed25519 签名**: 可选的数字签名，防止资源篡改

## 命令行工具

### 基本用法

```bash
# 打包（无加密）
tools/carc_pack ./mygame ./release/game.carc

# 打包 + 生成密钥对
tools/carc_pack ./mygame ./release/game.carc ./mygame/game.key.pub ./mygame/game.key

# 使用已有密钥打包
tools/carc_pack ./mygame ./release/game.carc
# (密钥文件 game.key 和 game.key.pub 需放在工作目录)
```

### 参数说明

```
carc_pack <input_dir> <output.carc> [public.key] [private.key]

  input_dir    — 要打包的目录
  output.carc  — 输出的 CARC 归档文件
  public.key   — (可选) 保存 Ed25519 公钥的文件路径
  private.key  — (可选) 保存 Ed25519 私钥的文件路径
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

打包后的资源通过 `carc://` 协议引用：

```kag
; 背景图片
@bg "carc://scenes/bg_classroom.png"

; 背景音乐
@bgm "carc://audio/bgm_01.mp3"

; 角色立绘
@fg "carc://characters/hero_smile.png"

; 音效
@se "carc://audio/se_click.wav"
```

## 运行时加载

引擎通过 `CarcAssetProvider` 处理 `carc://` 协议的资源请求：

```cpp
// 引擎内部 (自动处理，无需手动编码)
auto* provider = new CarcAssetProvider("game.carc");
provider->loadKey("game.key.pub");
AssetManager::instance().addProvider(provider);
```

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
A: 当前不支持增量更新。使用 `DeltaCARC`（开发中）可在后续版本中支持差异更新。
