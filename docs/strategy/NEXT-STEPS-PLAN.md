# Caesura (AmeKAG) 下一步计划

> 更新: 2026-06-09 | 状态: Beta 冲刺 9/12 完成，技术债 21/23

## 当前状态

引擎完成度 ~93% (Beta)。Electron 编辑器 E1-E8 + RPC 帧预览全部完成。

## 已完成

| 阶段 | 内容 | 状态 |
|------|------|:--:|
| 存档系统升级 | SU-1~6: AES-GCM + ISaveProvider + 快存 + Schema v1→v5 | ✅ |
| 技术债务修复 | U1-U4 + TD-23 + TD-20 | ✅ 21/23 |
| Electron 编辑器 | E1-E8: 舞台/时间线/属性/AI面板/RPC/多后端/打包 | ✅ |
| Beta A1 | FFmpeg 默认 ON + QUIET fallback | ✅ |
| Beta A2 | GLSL PBR-lite shader (DX11 就位) | ✅ |
| Beta A3 | Demo 4 场景 | ✅ |
| Beta A4 | AI 面板增强 (61 命令 + token 预算) | ✅ |
| Beta B1 | RPC 帧预览 (--editor + getFrame + Live Preview) | ✅ |
| Beta B3 | Electron 打包路径修正 | ✅ |
| Beta C1-C4 | 教程 + KAG API + MiniGame API + BUILD.md | ✅ |

## Beta 冲刺待完成

| IU | 内容 | 优先级 |
|----|------|:---:|
| B2 | 编辑器 UI 打磨 (主题/布局/快捷键) | P3 |

## 下一阶段选项

| # | 方向 | 预估 | 说明 |
|---|------|------|------|
| 1 | **编辑器 UI 打磨** | 小 | 暗色主题、布局优化、快捷键 |
| 2 | **MiniGame shader 跨平台编译** | 中 | shaderc + Metal/GL 二进制 |
| 3 | **Electron 打包试运行** | 小 | npm run package 验证 |
| 4 | **移动端适配** | 大 | Android + iOS，MobileAdapter 已就位 |

## 移动端适配路线图 (新增)

> MobileAdapter 已完整实现：触摸→鼠标注入、生命周期管理、DPI 缩放、手势。

| 阶段 | 内容 | 依赖 |
|------|------|------|
| **M1** | Android CMake 工具链 + SDL3 Android 后端 | NDK + bgfx GLES |
| **M2** | iOS CMake 工具链 + SDL3 iOS 后端 | Xcode + bgfx Metal |
| **M3** | 触屏输入路由 (MobileAdapter 接入 InputRouter) | M1/M2 |
| **M4** | 移动端 UI 适配 (DPI/安全区/字体缩放) | M3 |
| **M5** | 移动端打包 (APK/IPA) + 商店发布 | M4 |

**约束**: 需 Android/iOS 实机测试，当前 Windows 环境无法验证。M1-M2 可做构建配置，M3-M5 需共同开发者或实机环境。