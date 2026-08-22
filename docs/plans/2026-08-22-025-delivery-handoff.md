# Caesura (AmeKAG) — 交接文档（2026-08-22 第 25 号 / WSL Linux 验证 + 跨平台修复）

> 面向后续 agent 的完整上下文。承接 024（产品化 Sprint 4-5c + Steam SDK）。
> 本文件记录 **WSL Linux 全量测试与 8 项跨平台 Bug 修复**（round 133-135）。

---

## 1. 背景

目标：用 WSL（Ubuntu 26.04 WSL2）完成引擎 Linux 全量测试并修复发现的 Bug。
此前引擎从未在真实 Linux 环境（非 CI）验证过 out-of-tree 构建与无音频设备场景。

## 2. WSL 环境搭建（可复用）

| 项 | 内容 |
|---|---|
| 发行版 | Ubuntu 26.04 LTS（GCC 15.2 / CMake 3.28.3 官方二进制 / Lua 5.4.8 / SDL3 3.4.2 系统包） |
| SDK 依赖 | apt: build-essential libsdl3-dev libfreetype6-dev libzstd-dev libssl-dev + X11/pulse/wayland 全套 + nodejs |
| 源码位置 | **复制到 /root/caesura**（Windows 括号中文路径触发 CMake/make 引号问题——勿直接构建 /mnt/d） |
| 构建目录 | /root/build-linux（out-of-tree） |
| CMake 版本 | 必须 3.28（4.x 与 external/freetype 老 CMakeLists 不兼容） |
| 音频 | /root/.asoundrc 配 ALSA null 默认设备（headless WSL 无声卡） |
| WSL 坑 | 0x8007274c/E_UNEXPECTED 间歇性服务故障；勿 wsl --shutdown（用户 deepswe 在跑）；输出经 Windows wrapper 落 /d/ 读取 |

## 3. 修复的 8 项跨平台 Bug（均已 Windows 验证无回归 + CI 三平台）

1. **EditorServer 路径依赖 cwd**（P1）：out-of-tree 构建下 templates 列表空、项目端点 404
   → engineRoot() 向上探测 + CAESURA_SOURCE_DIR 宏优先（create/duplicate/import/list/meta 全接线）
2. **duplicate/import 漏网**（4b7 修订）：两 handler 仍用 current_path
3. **CAESURA_SOURCE_DIR 重复定义**：tests 本地 u8 宏 vs 全局窄宏冲突 → 中文路径 ACP 异常
   → 全局接口目标单一 u8 定义（tests 删除重复）
4. **测试 repoRoot 探测**（27 用例失败）：in-tree 假设 → 3 个测试文件宏优先
5. **SoLoud Linux 无声卡构建缺陷**：默认仅 NULL 后端→init 失败 7（CI 静默多年）
   → Linux 启用 SOLOUD_BACKEND_ALSA + CI 工作流配 /etc/asound.conf null 设备
6. **ALSA snd_pcm_drain(NULL) SIGABRT**：null 设备下音量 0 时序崩溃 → vendored soloud_alsa.cpp 判空
7. **Web dist 构建缺口**：CMake 从未复制 web/dist → POST_BUILD 复制（与 demo 同模式）
8. **create 未更新模板元数据 name**（显示 basic 而非项目名）+ smoke 产物路径 out-of-tree 兼容

## 4. Linux 验证终态（全部绿）

- **ctest 11/11 100%**（CI 等价门禁：HTTP smoke 72/72、CLI smoke、RPC smoke、audio、unit、全标签）
- C++ **991/991**（Steam 5 用例条件排除）；Lua **133 + 24**；golden **DONE:6376**；web index **CHECK OK 76**；耦合 PASS
- 引擎 --editor 完整启动（ALSA null 下音频正常进入主循环）
- Windows 回归：C++ 996/996、HTTP smoke 72/72、Golden 4/4

## 5. 提交链（93-95 轮区间）

4b3147c1（dup/import root）· a7e5071a（web dist 复制）· 6111386f（ALSA drain guard）·
616e9a8e（smoke 路径）· 806275cf（ALSA 后端）· e785b22d（CI null 设备）·
f1f9cea0/d5246cc0/abe62e54（round 133 三件套）

## 6. 下一步

- WSL 深 swe 空闲后可选重跑 http smoke 全量复核（ctest 已含同代码验证）
- Sprint 6 跨平台矩阵部分完成（Linux 引擎侧）——剩余 macOS/Web 多维浏览器
- 产品化剩余：Steam 发布（需账号）、第三方验证（需用户）
