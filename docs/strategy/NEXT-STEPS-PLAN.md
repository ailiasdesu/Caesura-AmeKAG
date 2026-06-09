# Caesura (AmeKAG) 下一步计划

> 更新: 2026-06-09 | 状态: Beta 冲刺 8/12 完成

## 当前状态

引擎完成度 ~92% (Beta)。Electron 编辑器 E1-E8 全部完成。Beta 冲刺 8/12 IU 完成。

## 已完成

| 阶段 | 内容 | 状态 |
|------|------|:--:|
| 存档系统升级 | SU-1~6: AES-GCM + ISaveProvider + 快存 + Schema v1→v5 | ✅ |
| 技术债务修复 | U1-U4: BackendRegistry/ParticleSystem/API去重/沙盒外抽 | ✅ |
| Electron 编辑器 | E1-E8: 舞台/时间线/属性/AI面板/RPC/多后端/打包 | ✅ |
| Beta A1 | FFmpeg 默认 ON + QUIET fallback | ✅ |
| Beta A2 | GLSL PBR-lite shader (DX11 就位, Metal/GL 待编译) | ✅ |
| Beta A3 | Demo 4 场景 (Classroom→Live2D→MiniGame→Video) | ✅ |
| Beta A4 | AI 面板增强 (61 命令 + token 预算 + 语法验证) | ✅ |
| Beta B3 | Electron 打包路径修正 | ✅ |
| Beta C1-C4 | 教程 + KAG API + MiniGame API + BUILD.md | ✅ |

## Beta 冲刺待完成

| IU | 内容 | 优先级 |
|----|------|:---:|
| B1 | RPC 帧预览 — 引擎渲染截图回传编辑器 | P2 |
| B2 | 编辑器 UI 打磨 (主题/布局/快捷键) | P3 |

## 下一步选项

| # | 方向 | 预估 |
|---|------|------|
| 1 | **RPC 帧预览** — 编辑器实时看到引擎渲染画面 | 中 |
| 2 | **编辑器 UI 打磨** — 暗色主题、布局优化、快捷键 | 小 |
| 3 | **跨平台 CI 修复** — macOS/Linux CI 去 continue-on-error | 中 |
| 4 | **MiniGame shader 跨平台编译** — 构建 shaderc + Metal/GL 二进制 | 中 |
| 5 | **Electron 打包试运行** — npm run package 验证安装包 | 小 |
| 6 | **引擎完善** — 新功能 / 性能优化 / Live2D 扩展 | 大 |