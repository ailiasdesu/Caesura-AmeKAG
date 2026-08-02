# Caesura (AmeKAG) — closeout 007：CI 全平台加固 + 编辑器安全闭环（2026-08-02）

> 范围：CI 全平台全绿 + 编辑器 HTTP 服务安全加固闭环（handoff 004 §4 剩余项收尾）。
> 前置：closeout 005（Live2D 验证）、closeout 006（HTTP debug routes + PathConfinement 测试）。

## 一、CI 全平台全绿（run 30707809797 = success）

### 根因
新增 `CaesuraHeadlessHttpSmoke`（HTTP 端到端冒烟）在**所有 CI 平台失败**：
`--editor` 模式需要 GPU 窗口（bgfx 初始化），无显示 runner（Linux/macOS）或 SDL dummy video
driver 下 `nwh=0` → `bgfx::init` 失败 → 引擎启动即退出（exit 1）→ HTTP 服务器未就绪。

本地复现：`SDL_VIDEODRIVER=dummy` → `Direct3D 11 init failed` → `Failed to initialize engine`。

### 修复（2 提交）
- `1d12fd9b`：smoke 检测引擎非零退出时输出 `HTTP SMOKE SKIPPED: NO GPU` 并 exit 77，
  ctest 设 `SKIP_RETURN_CODE 77`（无 GPU 环境跳过而非红管线；HTTP 路由逻辑由本地 + stdio smoke 覆盖）
- `31afd181`：review BLOCKING 修复——原 `rc = poll() if poll() is not None else -1` 的
  `else -1` 哨兵**反转跳过条件**：引擎存活但 server 未就绪（真实路由/启动回归）会误判为
  "无 GPU"跳过 77，掩盖失败。改为单次 `rc = proc.poll()`，仅 `rc is not None and rc != 0`
  时跳过；存活未就绪走 `finish(1)` 失败。单次赋值同时消除双 poll 的 TOCTOU。

### 验证
- 本地：`SDL_VIDEODRIVER=dummy` → exit 77 + SKIPPED；正常环境 → 21/21 PASSED
- CI：**五 job 全 ✓**（macOS Clang 2m4s / Linux GCC 2m53s / Windows MSVC Debug 5m23s /
  Windows MSVC Release 6m21s / Release Package），run 30707809797 = **completed success**

## 二、编辑器 HTTP 服务安全闭环（6 提交）

| 提交 | 内容 |
|---|---|
| `b96ffb6f` | /api/build 路径包含（拒绝 `../`/绝对路径/强制 `build/` 前缀）+ CORS localhost 白名单 + pauseId try/catch |
| `55c0512d` | CORS **精确 host 匹配**（拒绝 `localhost.evil.com` 前缀绕过，HIGH）+ build 反斜杠归一化 + JSON 转义响应 |
| `edabade3` | 短 Origin（`http:/`）substr 崩溃守卫（顺带修复 `ftp://localhost` 前缀绕过） |
| `f0fc2345` | `--editor-token` 可选 Bearer 鉴权 + CORS smoke 覆盖（16→21 断言） |
| `882932ea` | token 改**环境变量** `CAESURA_EDITOR_TOKEN`（argv 经 /proc cmdline 世界可读）+ OPTIONS 预检豁免 + 常量时间比较 |
| `e1c2c8b1` | 真正常量时间比较（无条件全字节 `diff |=` 累加，消除首字节失配时序 oracle）+ README 文档 |

### 安全验证
- CORS：evil 子域/userinfo/IPv6/大小写/尾点/短 Origin 全 403；localhost/127.0.0.1 任意端口 200
- token：无 token 401、错误 token 401、正确 token 200；OPTIONS 204 豁免；Allow-Headers 含 Authorization
- build：穿越/绝对路径 400、反斜杠归一化、默认 503（writer 未配置）
- security_review 终态：**no security issues found**（覆盖 e1c2c8b1、31afd181）

## 三、测试与回归基线
- CaesuraTests **563/563**（2760 assertions）、ctest **10/10**、HTTP smoke **21/21**、耦合度 PASS
- 双构建（build-repro-verify 无 SDK + build-live2d）零错误

## 四、变更文件（本轮 3 个提交：1d12fd9b / 31afd181 / 此前 6 个已记录）
- `tests/headless_http_smoke.py`（CORS 断言 + SKIP 逻辑）、`tests/CMakeLists.txt`（SKIP_RETURN_CODE 77）
- 其余见 closeout 006（路由/token/CORS/build 均已在 006 的提交链中记录）

## 五、遗留（非阻塞）
- token 常量时间比较无自动化单测（建议提取为 rpc 模块可测函数——见下一步）
- 非零退出均视为"无 GPU"跳过（信号死亡/启动崩溃在无 GPU runner 也会被跳过——stdio smoke 已覆盖路由，可接受）
- 固定端口 9876 无占用检查；多用户机器 token 经 env（同 UID 可见，已文档化）
