# CARC 打包指南

## 概述

CARC (Caesura ARChive) 是 Caesura (AmeKAG) 引擎的加密压缩归档格式。它用于将游戏资源打包为单个文件，支持：

- **AES-256-GCM 加密**: 每个文件独立生成 32 字节 AES 密钥 + 12 字节 nonce
  加密，**始终启用**（归档整体即加密+压缩形态，无明文模式）
- **zstd 压缩**: 逐文件 zstd 压缩，平衡速度与压缩率（始终启用）
- **Ed25519 签名**: 头+内容+索引整体签名，公钥内嵌归档尾部（始终启用）

## 命令行工具

### 基本用法

工具位于构建输出目录 `bin/Debug/carc_pack.exe`（Release 为 `bin/Release/carc_pack.exe`），支持 `pack`（默认）、`list`、`extract` 三种操作：

```bash
CARC_PACK=bin/Debug/carc_pack.exe

# 打包（每次生成全新密钥对；密钥随归档尾部 + 可选落盘）
$CARC_PACK ./mygame ./release/game.carc

# 打包并额外把生成的公钥/私钥写到指定文件
$CARC_PACK ./mygame ./release/game.carc ./mygame/game.key.pub ./mygame/game.key
# (若省略密钥路径，公钥仅内嵌归档尾部、私钥不留盘)

# 注意：不支持复用已有密钥——每次 pack 都会生成新密钥对。

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
│  Header (64 bytes)                      │
│  ├─ Magic "CARC" (4) / Version u32 (1)  │
│  ├─ contentOffset u64 (= 64)            │
│  ├─ contentSize u64 / indexOffset u64   │
│  ├─ indexSize u64 / numFiles u32        │
│  └─ reserved[20]                        │
├─────────────────────────────────────────┤
│  Content block（逐文件 zstd 压缩 →        │
│  AES-256-GCM 加密，按条目串行拼接）       │
├─────────────────────────────────────────┤
│  Index（始终 AES-256-GCM 加密）           │
│  ├─ File count: uint32                  │
│  ├─ FileEntry × N（每条 116 bytes）      │
│  │    pathHash[32]  offset u64          │
│  │    compressedSize u64  originalSize  │
│  │    aesKey[32]  nonce[12]  tag[16]    │
│  └─ append 16-byte AES-GCM tag          │
│  索引密钥 = SHA-256(归档公钥 32B)；       │
│  nonce = CARC 版本号（4B 零填充）         │
├─────────────────────────────────────────┤
│  Trailer (96 bytes)                     │
│  ├─ Ed25519 Signature (64 bytes)        │
│  └─ Public Key (32 bytes)               │
└─────────────────────────────────────────┘
> 归档**始终**加密+压缩+签名（无明文/未签名模式）。每个文件由自己的
> aesKey/nonce/tag 加密；`list` 仅输出 `pathHash` 的 64 位十六进制
> （原始相对路径不落盘，`extract --path <rel>` 才能还原真实文件名）。
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
5. **nonce 复用检测（默认开启）** — CryptoEngine 按 (AES key, nonce) 键界维护一个**有界**（1024 条，约 45 KB）进程级复用注册表。encrypt() 以同一 (key, nonce) 加密两次会在第二次**拒绝产出密文**（返回空），fail-closed 地避免 GCM 重放 keystream 泄露。注册表按 (key, nonce) **成对**键界，因此同一 nonce 在不同 key 下使用（安全语义）不会被误判。generateNonce() 使用 CSPRNG（96-bit 碰撞概率 < 2^-48），正常打包路径不会触注册表；仅当调用方在固定 key 下以确定性/计数器方式生成 nonce 时，可调用 CryptoEngine::setNonceReuseDetection(false) 关闭检测（默认开启）。

## 常见问题

**Q: 打包后文件比原始文件大？**
A: 检查是否包含了不应打包的文件（如 `.git/`, `.DS_Store`）。仅打包 `assets/` 和 `scripts/` 目录。

**Q: 运行时提示 "CARC signature verification failed"？**
A: 公钥不匹配或归档文件被修改。确认使用的公钥与打包时的私钥配对。

**Q: 能否增量更新 CARC？**
A: 当前不支持增量更新。引擎侧 `DeltaCARC`（`src/archive/DeltaCARC.*`）差异更新实现已完成（文件级 diff，AES-256-GCM 加密整条 delta，按源/目标 SHA 绑定验证）——**截至 round-99 复核，仍仅作为库存在于 archive 模块（`src/entry/Engine_Assets.cpp` 组合根只注册 `CarcAssetProvider`，`carc_pack` 也未暴露 DeltaCARC 子命令）**，需额外集成方可在实际流程中使用。