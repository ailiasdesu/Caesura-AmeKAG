# web_stress_vn — Web 压力测试资产（Track W4）

本目录是 Caesura (AmeKAG) **Web 发布链路 Track W4（Web stress）** 的压力测试项目：
用大量小纹理 + 大体积音频驱动 Web 播放器做持续的加载/释放循环，压测资源管线、
内存与音频流表现。场景脚本为 `stress.ks`（12 页循环，从 120 张背景图池中取图，
交替播放两个大 WAV BGM，经 `[jump *start]` 无限循环，由 smoke 驱动器控制轮数）。

## 资产清单

| 路径 | 数量 | 规格 |
| --- | --- | --- |
| ``仓库共享池 assets/stress/bg/bg001.png`（由 scripts/package_game.sh --assets 默认仓库 assets 池打包）.. bg120.png` | 120 | 256x256、8-bit RGB 纯色 PNG；单张 ≤4KB（实测 564–570B）。`bg001`=红、`bg002`=绿、`bg003`=蓝，其余 117 张为固定种子生成的互不相同亮色 |
| `assets/stress/bgm/tone440.wav` | 1 | 正弦 440 Hz，30 秒，44.1kHz 单声道 16-bit PCM，2,646,044 字节（约 2.6MB） |
| `assets/stress/bgm/tone550.wav` | 1 | 同上，550 Hz |

合计 122 个生成资产，约 5.24MB（其中 PNG 共约 67KB）。文件名全部 ASCII。

## 生成方式

仅使用 Python 标准库（无 PIL/numpy），由仓库内脚本一键再生：

```bash
cd tests/projects/web_stress_vn
python tools/gen_stress_assets.py
```

- **PNG**：手写编码（`zlib`+`struct`）——签名 `89 50 4E 47 0D 0A 1A 0A`，
  IHDR（256x256 / 位深 8 / 颜色类型 2=RGB / 不隔行），IDAT 为 zlib 压缩的原始
  扫描行（每行前置 filter byte 0），IEND 收尾；每个 chunk 按
  length(BE32)+type+payload+crc32 组帧。整图为单一纯色，压缩后单张不足 600 字节。
- **WAV**：`wave`+`math` 合成纯正弦（振幅 0.8 满量程），标准 PCM 头。
- **确定性**：调色板随机数种子固定（20260823），正弦采样为纯数学计算；
  重复运行输出与既有文件字节一致（幂等覆盖），已验证 122 个文件重跑前后 md5 全同。

## 与本项目的关系

- 本目录属于 `tests/projects/` 下的测试项目资产（对应 Web Track W4「Web stress」场景），
  供 Web 打包与浏览器冒烟链路使用（参考：`scripts/package_game.sh` +
  `scripts/web_browser_smoke.mjs`，W4 实测数据记录于 `docs/status/web-release-status.md`）。
- 纯静态测试资产：不参与引擎构建，也不在 CaesuraTests 门禁范围内；
  可整体删除后随时用上述命令重新生成。除本目录外不影响任何其他文件。