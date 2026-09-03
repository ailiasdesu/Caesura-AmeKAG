# Caesura Studio Rust 壳（Tauri 2）立项草案 — 2026-09-03

> **决策（用户 2026-09-03 拍板，修订 033 决策 #7）**：Caesura Studio 套壳**不用 Electron**，改用 **Rust**（Tauri 2）——保证效率（系统 WebView 原生渲染 vs Chromium 全量；内存/体积/启动三优）。
> 本文件=技术方案+范围+里程碑（DRAFT，派发执行批前请用户确认）。

## 1. 现状与决策依据

| 项 | 现状 |
|---|---|
| 壳雏形 | `editor/electron/main.cjs`（1-25 行）+ package.json electron/electron-builder 依赖（:30-31，`npm start=electron .` :14）——无打包配置（033 #7 曾定 Windows Electron） |
| Studio Web 形态 | 已可用：`CAESURA_EDITOR_WEBROOT=editor/dist` + 引擎 --editor → 浏览器直开 9876（t156 落地，同源零 CORS） |
| 前端 token | rpc.ts `setToken()` 注入；可用 `?token=` URL（t5 验证过与 localStorage 并存） |
| 本机环境 | rustc/cargo **1.98.0** ✓；MSVC 2022 ✓（记忆：dsh-desktop 构建先例——Tauri 项目本机构建可行；coreutils link 劫持 MSVC link.exe 的坑已记录） |
| 备选 | wry（裸 WebView，~1-2MB）：体积最小但打包/生命周期/userData 全自理——记不选因（Studio 需分发/升级/图标/多平台，Tauri 价值远超体积差） |

## 2. 架构（最小职责壳）

```
[studio/ Rust crate (Tauri 2)]
	    │ ① 定位引擎（同目录 CaesuraAmeKAG.exe；无→选择面板）
	    │ ② spawn 引擎 --editor（env CAESURA_EDITOR_WEBROOT=<安装>/editor/dist）
	    │ ③ 轮询 127.0.0.1:9876 /api/ping 就绪
	    │ ④ WebView 加载 http://127.0.0.1:9876/?token=<读 .caesura-editor-token>
	    │ ⑤ 退出（窗口关/用户退出）→ child.kill 引擎；引擎崩溃→错误页+重启选项
	    └ 端口占用前置检查（9876 已占用→清晰提示而非静默失败）
```

- **同源契约**：UI 与 /api/* 同端口同源（现有设计）→ 零 CORS 改动；禁止壳内开平行 RPC（021 纪律延续）
- **token**：壳读 token 文件拼 URL（/_?token=\_ 路径已验证）；壳不参与 token 门控逻辑
- **跨平台**：Windows 优先（WebView2）；macOS(WKWebView)/Linux(WebKitGTK) 随 Phase2

## 3. 范围与里程碑

| 阶段 | 内容 | 出口 |
|---|---|---|
| S1 | `studio/` Tauri 2 crate 骨架：起引擎→WebView ready→本机 e2e（壳拉起→页面达→/api/ping ok→退出杀引擎） | 本机可跑 |
| S2 | 生命周期完善：端口占用提示/引擎崩溃恢复/优雅退出/日志 | 壳 e2e 全绿 |
| S3 | 发布接线：cargo build（或 tauri build）产物进发布包+verify 断言+CMake/CI | Windows 包含壳 |
| S4 | electron 退役：package.json 去 electron 依赖+main.cjs 移除+README/033 同步 | 仓内零 electron |
| S5（Phase2） | macOS/Linux 壳+WebView2 runtime 处理（Evergreen bootstrap） | 三平台 |

## 4. 风险

1. **WebView2 runtime**：Win10 需 Evergreen（发布 bootstrap 或文档前置）；Win11 自带——S3 验证时确认本机/CI
2. **9876 冲突**：双实例（记忆：双栈陷阱）——壳做前置占用检查（预检 127.0.0.1:9876 监听）
3. **CI 加 Rust 工具链**（Windows job 装 rustup/cargo——约 +2min；S3 接线）
4. Tauri 依赖面（tauri crate 链大）+ Windows 构建核心坑（MSVC link coreutils 劫持——记忆已录解法：PATH 前置 MSVC bin）

## 5. 033 #7 修订注（用户拍板 2026-09-03）

> 原：#7 Windows 优先 Electron。## 修订：**Windows 优先 Rust 壳（Tauri 2）**——用户拍板「不用 Electron，用 Rust 套，保证效率」；其余 #7 语义（Windows 优先/三平台引擎侧不受影响）不变。electron 壳与依赖退役（S4）。
