# XP3 兼容文档（v1）

> 主题：KiriKiri（吉里吉里/TVP）`.xp3` 归档读取器最小原型
> 位置：`tools/xp3_tool.py`（独立 CLI，只读，不接入引擎）
> 配套测试：`tests/scripts/test_xp3_tool.py`
> 状态：v1 最小原型 —— 未加密索引 + zlib/raw 数据段

---

## 1. 格式研究摘要

本摘要对照**权威实现**编写：KiriKiri 官方源码
[`krkrz/base/XP3Archive.cpp`](https://github.com/krkrz/krkrz/blob/master/base/XP3Archive.cpp)
（及 `XP3Archive.h` 常量）与 GARbro
[`ArcFormats/KiriKiri/ArcXP3.cs`](https://github.com/morkt/GARbro/blob/master/ArcFormats/KiriKiri/ArcXP3.cs)。
两套实现布局一致，相互印证。

### 1.1 文件头（11 字节魔数 + 索引偏移）

| 偏移 | 长度 | 内容 |
|------|------|------|
| 0x00 | 11   | 魔数：`0x58 0x50 0x33 0x0D 0x0A 0x20 0x0A 0x1A 0x8B 0x67 0x01`，即 `"XP3\r\n \n\x1a\x8bg\x01"` |
| 0x0B | 8    | 索引块偏移（u64 LE，绝对偏移，通常按 0x200 对齐） |

**魔数勘误（重要）**：任务简报/网络上常见写法
`"XP3\r\n\x1a\x8a\r\n\x00\x00"` 是**误传变体**。
KiriKiri 源码（`XP3Mark1[]` + `XP3Mark2[]` 拼接）与 GARbro
（`s_xp3_header`）实际都是
`"XP3\r\n \n\x1a\x8bg\x01"`（第 5 字节是 0x20 空格、第 8 字节是
0x8B，且 0x01 表示"文件结构版本 0 + 字符编码 1 = BMP UTF-16" 的高低位提示）。
本工具**两种魔数都接受**（将误传变体记为 `variant` 并在 list 尾部标注），
打包写入时使用规范魔数。

自解压变体：XP3 归档可嵌入 .exe（文件头 "MZ"），引擎会扫描魔数。
v1 **不支持** exe 内嵌，仅支持纯 .xp3。

### 1.2 索引块（含索引编码标记）

索引块起始 1 字节为 `index_flag`：

| 位 | 掩码/值 | 含义 |
|----|---------|------|
| 0-2 | `0x07` | 索引编码方法：`0`=RAW（未压缩）、`1`=ZLIB（zlib 压缩） |
| 7   | `0x80` | `INDEX_CONTINUE`：链式索引标记（继续读下一个索引块） |

ZLIB 索引块布局：`flag(1) + compressed_size(u64) + index_size(u64) + zlib 数据`。
RAW 索引块布局：`flag(1) + index_size(u64) + 原始数据`。

**加密索引标记**：GARbro 探测到索引位置处 u32 == `0x80` 时认为索引被
加密/重定向（随后在 +9 处读取真实目录偏移），用于部分商业作品的
防提取。本工具对该情况与"index_flag 编码方法非 0/1"一律报
`encrypted-index` 错误；`INDEX_CONTINUE` 链式索引报
`index-continue-unsupported`。

### 1.3 索引载荷（chunk 序列）

索引解压/读出后是一串 **chunk**：`4 字节标签 + u64 大小 + 载荷`，
顶层只关心 `"File"` 块。每个 `File` 块内部是子 chunk 序列：

| 子块标签 | 载荷内容 |
|----------|----------|
| `"info"` | `flags(u32) + org_size(u64) + arc_size(u64) + name_len(i16) + name(UTF-16LE, name_len*2 字节)`。`flags` bit31 = protected |
| `"segm"` | 每段 28 字节：`seg_flags(u32) + start(u64 绝对偏移) + org_size(u64) + arc_size(u64)`。seg_flags bits0-2：`0`=RAW，`1`=ZLIB |
| `"adlr"` | 4 字节 u32 **路径哈希**（引擎按名查找用的哈希，**不是数据校验和**；格式本身无内容 CRC） |

要点：
- 文件名是 **UTF-16LE**（BMP），`name_len` 是字符数 i16。
- 一个文件条目可有 **多个段**（比如 OggVorbis VQ 码本共享场景），
  依 `segm` 顺序首尾拼接还原；每段可独立选择压缩与否。
- `info.org_size / arc_size` 是全文件合计值，与段求和一致。
- 数据段压缩为标准 **zlib**（带 2 字节头，Python `zlib.decompress`
  直接可解）；个别工具也可能写 raw-deflate，本工具做了
  `wbits=-15` 兜底。

### 1.4 常量速查（krkrz XP3Archive.h）

```c
#define TVP_XP3_INDEX_ENCODE_METHOD_MASK 0x07
#define TVP_XP3_INDEX_ENCODE_RAW  0
#define TVP_XP3_INDEX_ENCODE_ZLIB 1
#define TVP_XP3_INDEX_CONTINUE    0x80
#define TVP_XP3_FILE_PROTECTED    (1<<31)
#define TVP_XP3_SEGM_ENCODE_METHOD_MASK 0x07
#define TVP_XP3_SEGM_ENCODE_RAW  0
#define TVP_XP3_SEGM_ENCODE_ZLIB 1
```

---

## 2. v1 支持范围

- 未加密索引：RAW 与 ZLIB 两种编码方法。
- 单索引块（无 `INDEX_CONTINUE` 链）。
- 数据段：RAW（不压缩）与 ZLIB（zlib 头 + raw-deflate 兜底）。
- 多段文件条目（按段序拼接）。
- `protected`（flags bit31）条目：可列出、可解压（保护位是"请勿提取"
  的礼节性标记，数据本身未必加密）。
- 条目选择：8 位十六进制哈希（支持 `0x` 前缀与 ≥4 位前缀匹配）、
  完整路径名、唯一基线名。
- 纯 .xp3 文件；两种魔数均接受。

## 3. v1 限制清单（明确不支持的，均报带类别错误）

| 场景 | 错误类别 | 行为 |
|------|----------|------|
| 魔数不匹配 / 截断 | `bad-magic` / `truncated-header` | 拒绝并给出期望魔数 |
| 加密/重定向索引（u32==0x80） | `encrypted-index` | 拒绝，提示 v1 仅未加密索引 |
| index_flag 编码方法非 0/1 | `encrypted-index` | 拒绝 |
| 链式索引 `INDEX_CONTINUE` | `index-continue-unsupported` | 拒绝 |
| 段编码非 RAW/ZLIB | `encrypted-segment` | 拒绝 |
| 段越界 / 解压失败 / 长度不符 | 对应 `segment-*` 错误 | 拒绝该条目 |
| exe 内嵌归档（MZ 开头） | —— | 解析失败（魔数不匹配），文档提示 |

不保证项/降级：
- 无任何 CRC 校验（XP3 格式本身无内容校验和；`adlr` 是路径哈希）。
- 名字缺省/无法解码时以 `hash:0x……` 展示，解压落盘 `0x%08x.bin`。
- 加密索引**不做猜测性破解**（见第 5 节路线图）。

## 4. 工具用法

```bash
# 依赖：Python 3.8+（标准库 only：argparse/struct/zlib/unittest）
python tools/xp3_tool.py list <archive.xp3> [--json]
python tools/xp3_tool.py extract <archive.xp3> <hash|name> [--out DIR] [--force]
python tools/xp3_tool.py extract-all <archive.xp3> [--out DIR] [--use-names] [--force]
python tools/xp3_tool.py verify <archive.xp3>
```

- **list**：逐行输出 `0xhahsh  路径名  org=.. arc=.. segs=.. [标记]`，
  尾部汇总条目数/索引编码/魔数变体；`--json` 输出机器可读 JSON。
- **extract**：目标可用哈希（`0x22222222` 或前缀 `2222`）、完整路径
  （`bg/op.bmp`）或唯一基线名（`op.bmp`）。按名字落盘到
  `--out`（默认当前目录），自动建目录；已存在时需 `--force`。
- **extract-all**：v1 默认以 8 位哈希名落盘（保留原扩展名），同时产出
  `manifest.txt`（名字/大小/路径映射）；`--use-names` 改为按真实路径
  建目录落盘。名字含 `../` 等危险成分会被拒绝（`unsafe-name`）。
- **verify**：解析索引 + 试解压全部数据段 + 校验长度，输出 OK 统计；
  任何损坏/不支持都会以非零退出码与 `error[类别]` 形式报错。

退出码：`0` 成功；`2` 解析/IO 错误。

## 5. 测试

```bash
python tests/scripts/test_xp3_tool.py
```

零外部依赖（unittest，亦兼容 pytest 收集）。覆盖：
- 手工打包（对照 krkrz 布局）的合成 .xp3 之 list/extract/extract-all/verify
  与原始数据**逐字节**往返；
- 多段文件拼接、RAW/ZLIB 索引双变体、UTF-16 日文名、protected 位；
- 损坏魔数 / 截断头 / 加密索引标记 / 未知索引编码 / 链式索引 /
  垃圾 zlib 索引，全部断言报出**指定错误类别**；
- CLI 子进程往返 + 负例退出码 2。

## 6. 后续：加密索引路线图（v2+）

1. **识别**：沿用 GARbro 的 0x80 标记 + index_flag 扩展位探测，分离
   "加密索引" 与 "仅名字加密"（部分作品只加密名字、数据仍 zlib）。
2. **方案表**：收集已知作品用加密方案（如 千恋*万花/SenrenCxCrypt 的
   `yuz:`/名字映射块、KrkrExtract 的已知方案表），做成可插拔
   `ICrypt` 风格的解码器注册表；引擎侧不内置任何密钥，密钥由调用方注入。
3. **哈希到名字映射**：`list` 输出偏哈希；若启用名字映射块
   （`hnfn`/`smil`/`Yuzu` 等顶层块）解析，则尽力还原路径名。
4. **防护**：涉版权提取功能默认关闭，须显式 `--allow-crypto` 且承担
   TOS 责任；本工具仅做互操作研究，不提供绕过保护位的能力。
5. **TLG 解码**：.tlg 图像（tlg5/tlg6）读取器独立拆分为
   `tools/tlg_tool.py` 子项目，与 xp3 抽取解耦。

