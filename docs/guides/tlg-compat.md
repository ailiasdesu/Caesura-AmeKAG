# TLG 图像格式兼容 (KiriKiri .tlg → PNG)

> 工具：`tools/tlg2png.py` — Caesura (AmeKAG) 引擎的 **独立 CLI 原型**。
> 状态：**v1 原型**。将 KiriKiri 吉里吉里引擎专有图像格式 TLG5/TLG6 解码为 PNG。

## 1. TLG5 / TLG6 格式概述

TLG 是 KiriKiri（吉里吉里）游戏引擎的无损图像格式，最初用于 tjs2，后由
krkrz 沿用。文件魔数均为 **11 字节**：

```
TLG5.0\x00raw\x1a     → TLG5
TLG6.0\x00raw\x1a     → TLG6
```

（更旧的变体在 15 字节 `TLG0.0\x00sds\x1a` 前缀之后才出现上述签名；
另有 `XXXYYY` / `XXXZZZ` / `JKMXE8` 等被 XOR 混淆的魔数变体。）

### TLG5（较简单，无分层）

| 段 | 字节 | 含义 |
|----|------|------|
| 签名 | 11 | `TLG5.0\x00raw\x1a` |
| colors | 1 | 3=RGB, 4=RGBA |
| width / height | 4+4 | 像素尺寸（小端） |
| blockheight | 4 | 纵向块高 |
| block 尺寸表 | 4×blockcount | 每块字节数（解码时跳过） |

按块（每 `blockheight` 行一块）逐块、逐颜色平面压缩；每个平面一个数据段：
- **mark=0** → 修正 LZSS 压缩；
- **mark≠0** → 原始 bytes。

LZSS 使用 **4096 字节环形窗口** + 8 位控制标志（LSB 先行），匹配令牌为
【12 位反向偏移 + 4 位长度】，长度 3..18，`18` 时额外读 1 字节扩展。
窗口**跨块、跨颜色平面连续**（每块每平面的输出会顺序写入窗口）。

像素重组：每块平面缓冲累加水平差分，再叠加上一像素行的垂直差分（mod 256）。
颜色平面内嵌 G 通道（B+G / R+G 的差分），alpha 各自独立累加。

### TLG6（分层 + 分块 + Golomb 熵编码）

| 段 | 字节 | 含义 |
|----|------|------|
| 签名 | 11 | `TLG6.0\x00raw\x1a` |
| colors | 1 | 1=灰度, 3=RGB, 4=RGBA |
| flags | 3 | data flag / color type / external golomb table（必须为 0） |
| width / height | 4+4 | 尺寸 |
| max_bit_length | 4 | 全图像比特流总长（辅助） |
| 滤波类型表 | u32 长度 + LZSS 压缩载荷 | 每 8×8 块一个滤波器类型（0~31） |

滤波类型表用**预置 4096 字节窗口**的修正 LZSS 压缩（窗口初始化为 32×16 的
0x01010101 递增值字节）。

图像按 **8 行一组**处理；每组每个颜色通道一个 Golomb 比特流：

- 比特流第一个 bit 是 **首段是否全零** 标志；
- 段（run）以 **unary 计数 + 二进制后缀** 编码：`zeros` 个 0 → 一个 1 →
  `zeros` 位（LSB-first）写入 `count - (1<<zeros)`；
- 非零段内每个值用 **4 状态自适应 Golomb 表**（表 `k` 随误差绝对值和 `a`
  每 4 值更新一次），符号-幅度编码（`m = 2|e|-1` / `-2|e|-2`）；
- 当 unary 的 0 数导致写入超过 4 字节（`GOLOMB_GIVE_UP_BYTES`）时，改为把
  `m>>k` 作为**单个 8 位字面字节**写出（解码器 `idx+=5` 读取）。

行组装：按 8×8 块对 32 位 ARGB 像素套用 **32 种滤波预测**（MED / AVG 及
其通道线性组合），支持锯齿扫描（偶行左→右，奇行右→左）。预测误差 `diff`
经 `decoder(p, up, p_prev, diff)` 重建，参考 krkrz `tvp_med2` / `tvp_avg`。

## 2. 工具用法

```sh
# 解码为 PNG（输出名缺省 = 输入名换 .png）
python tools/tlg2png.py <image.tlg>

# 指定输出 / 仅看信息
python tools/tlg2png.py <image.tlg> <out.png>
python tools/tlg2png.py <image.tlg> --info

# 用 Pillow 写 PNG（缺省用手写 zlib+struct 编码，零依赖更稳）
python tools/tlg2png.py <image.tlg> --via-pillow
```

输出按内容自动选择 PNG 色彩模式：
- TLG6 colors=1 → 8 位灰度 `L`
- colors=3 → `RGB`
- colors=4 → `RGBA`

**PNG 编码不依赖 Pillow**：默认用 `zlib` + `struct` 手写（IHDR/IDAT/IEND，
filter type 0），因此即使无第三方库也能工作；`--via-pillow` 在 Pillow 可用时
提供更通用的输出路径，不可用时自动回退。

## 3. 支持 / 不支持范围

**已支持**
- TLG5：RGB(3) / RGBA(4)，任意 blockheight，LZSS 与 raw 两种平面编码，
  跨块跨平面窗口延续，RGB/RGBA 差分重组。
- TLG6：灰度(1) / RGB(3) / RGBA(4)，滤波类型表（预置窗口 LZSS），
  每通道 Golomb（含全零段、非零段、自适应 k、转义长码路径），
  8×8 块 MED/AVG 32 种滤波器与锯齿扫描，非 8 倍数尺寸（fraction 块）。
- 旧式 15 字节前缀头的 TLG5/TLG6（签名位于 offset 15）。
- 冗余检测：畸形颜色分量数、TLG6 非零标志位、被混淆的变体魔数、
  数据截断、尺寸异常，全部抛出可读错误。

**明确不支持（报错）**
- TLG6 熵编码方法非 00（Golomb）的变体：01 Gamma / 10 修正 LZSS / 11 raw。
- `XXXYYY` / `XXXZZZ` / `JKMXE8` 等 XOR 混淆魔数变体。
- TLG6 非零 `data flag / color type / external golomb table`。
- 引擎侧的多图合成（TLG 尾部 `tags` 元数据引用基底图做混合/覆盖）——
  本工具只给出单帧解码像素。
- TLG5 的 universal transition rule（glm 非 normal 模式）加载路径。

## 4. 设计实现要点

- **零第三方依赖**：仅标准库 `struct` / `zlib` / `argparse`；Pillow 仅作为
  可选输出后端。
- 解码算法逐行对齐 krkrz（`visual/LoadTLG.cpp`）与 GARbro
  （`ArcFormats/KiriKiri/ImageTLG.cs`）参考实现，含 32 位 ARGB 槽位逐字节
  SIMD 等价逻辑（MED/AVG 用字节掩码位运算，与 C 版 `tvp_med2` 一致）。
- 纯 Python 实现，性能用于原型/工具场景足够；生产引擎接入宜移植为 C++。

## 5. 测试

合成样本自洽测试：`python tools/tests/test_tlg2png.py`

因缺少真实 .tlg 样本，测试采用 **已知像素模式 + 最小实现** 自洽验证：

1. 测试内置的对称**编码器**（enc_tlg5 / enc_tlg6、enc_lzss、golomb_encode）
   从已知像素构造合法二进制流（与解码器逐位互逆）；
2. 解码回像素，断言与原始像素**逐字节相等**；
3. 手写 PNG 编码器写出 .png，再用独立的最小 PNG 读取器（zlib+反滤波）
   读回断言像素一致；
4. Pillow 可用时交叉验证一次。

覆盖用例：LZSS 单元（match / 扩展长度 / 跨调用窗口延续）、TLG5 RGB/RGBA/
RAW/multi-block、TLG6 RGBA/RGB/灰度/窄图/1x1/转义长码、错误路径与不支持
变体报错。**38 项断言全部通过。**

（备注：TLG6 自适应 Golomb 的 `a` 累计按格式可越过 1024 表边界，参考实现
依赖输入自然图像保证误差幅度有界；本工具在 `a≥1024` 处截断到 1023 防止越界，
测试已覆盖。）

## 6. 参考

- [krkrz 官方解码器 visual/LoadTLG.cpp + SaveTLG6.cpp](https://github.com/krkrz/krkrz)
- [GARbro C# 移植 ArcFormats/KiriKiri/ImageTLG.cs](https://github.com/morkt/GARbro)
- [tlg-rs / libtlg-rs（Rust 实现）](https://lib.rs/crates/tlg-rs)
- [jisho 格式页：TLG (KiriKiri)](http://fileformats.archiveteam.org/index.php?title=TLG_(KiriKiri))
