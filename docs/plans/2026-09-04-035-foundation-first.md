# 035 · Foundation First（底层优先）

> 状态：EXECUTING（2026-09-04 用户放行「全做，排除 steam/macos/ios」）
> 日期：2026-09-04
> 拍板：用户「先把底层完善了再做 Studio」（2026-09-04）；「底层欠账除了 steam 和 macOS 还有 iOS 之外全做」（2026-09-04）
> 排除项：Steam（发布链路待账号）、macOS（物理机/签名/公证）、iOS（真机/签名）——均保持硬件/账号 gated。palette 3D-LUT 包含在执行范围（P3 渲染新后端，独立大子批）。

## 1. 背景

用户拍板：**Caesura Studio（031/032/033/034 全线）挂起待命，先完善底层（Runtime 引擎核心 + 产品化根基），再做 Studio。**

对应总任务书（docs/plans/audit/Caesura-AmeKAG_产品化推进总任务书.md）的 Phase0（1.x 稳定化）定位：Studio 是表层前端，引擎本身的稳定性/兼容性/命令闭环/平台证据/崩溃可诊断性才是第三方开发者信任的根基。

## 2. 底层欠账清单（全部来自已定稿审计，未新做盘点）

### A. Capability Closure PARTIAL 清零 —— 用户 2026-09-03 高级阶段③ 收尾

现状：PARTIAL 51 → 37 → **3**（hr / typewriter / palette），2026-09-03 达成。

| 命令 | 缺口 | 处置 |
|---|---|---|
| typewriter | 音效接线 | **t201 在飞**（release-verify-2 实施，B 批） |
| hr | 效果面核真 | 待核真（大概率已闭环，仅需接线/验证） |
| palette | 真实缺口：原生 set_palette 幻影绑定；需 P3 3D-LUT 后端 | 维持 PARTIAL = honest backlog；P3 立项前不修 |

清零点 = 「0 UNWIRED + 0 PARTIAL」；palette 是唯一需要新后端的项，可挂 P3。

### B. TTFV 干跑审计遗留（2026-08-29 干跑）

- P1 --help：main.cpp 无 `--help` 分支（命令行帮助未实现）。唯一遗留 P1，小改动。
- KPI 首路径已重排（getting-started 路径 A Release ZIP 推荐）；北极星 30min TTFV 未复测。

### C. Phase0 稳定化三件（总任务书 Phase0，尚未做）

1. **docs/compatibility.md**：KAG3 兼容范围 / Neo-Genesis syntax / save·project 兼容 / 迁移策略 / breaking policy。
2. **Golden Project + Golden Save**：`tests/projects/golden_vn/` 跑满能力矩阵（dialogue/choices/save/rollback/backlog/NVL/i18n/audio/Live2D/tween/layout/replay/mod/Steam）+ 跨版本存档迁移测试。
3. **崩溃诊断友好错误**：玩家拿友好错误（项目/版本/平台/场景/行/命令 + Copy diagnostic + Open log folder），而非 Segfault；开发者模式 structured log / stack trace / runtime state / GPU backend。

### D. 平台证据补齐（Phase2 前置，分批）

- 平台矩阵（design/platform-support-matrix.md）大量格 '?'（无证据 = 未验证，诚实语义）——补证据流。
- Android：A2 四处改造（CMake ANDROID 分支 / MODULE 目标 / JNI / R6 资源）待 NDK；K40 真机链路在线（adb 192.168.8.11:5555）。
- iOS：探针 continue-on-error，未硬门（mac 硬件验证项 I4 未做）。
- 发行适配：Linux AppImage/Steam/Deck、macOS .app/DMG/signing、Windows portable/Steam。
- Steam 发布链路：预研完成，发布（AppID/depot/overlay 真验）待账号。

## 3. 建议批次

| 批 | 内容 | 理由 |
|---|---|---|
| 批1 | A 清零（t201 收尾 + hr 核真）+ B（--help） | 快、把 09-03 高级阶段③彻底收尾 |
| 批2 | C 三件（compatibility / golden / 崩溃诊断） | Phase0 主体，Studio 复启的信任根基 |
| 批3 | D 分批（Android K40 / iOS 硬门 / 发行适配 / Steam 发布） | 依赖真机/账号/平台账号，按可获得性拆分 |

## 4. Studio 复启条件

- 批1、批2 完成（closure 清零 + Phase0 三件落地）且用户明确口令；
- 034（Rust/Tauri 2 壳）已定稿，复启时直接执行 S1。

## 5. 决策记录

- 2026-09-04：用户拍板底层优先；034 保持 DRAFT 待命，不执行 S1。
