# Caesura Galgame 引擎核心就绪度排查方案

> 2026-06-18 | 3 开发 + 1 文档 | 周期预估 5-7 天

## 策略

**核心体验面优先 + 模块并行排查 + 共享问题表**。

先确保一条完整 galgame 链路跑通，再横向覆盖。模块耦合 ≤4，修一个不会牵一发动全身。

---

## 就绪度清单（76 项，按 galgame 功能域分组）

每项验证通过打 ✓，失败标注根因和责任人。

### D1 · 启动与初始化（5 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D1.1 | Engine 正常启动，无崩溃 | `./CaesuraAmeKAG.exe` | ⬜ |
| D1.2 | SDL3 窗口创建、尺寸正确 | 启动后检查窗口标题+尺寸 | ⬜ |
| D1.3 | 命令行 `--editor` 参数生效 | 带 --editor 启动 | ⬜ |
| D1.4 | 所有 21 个后端注册成功（无 nullptr） | Engine::init 后检查 BackendRegistry | ⬜ |
| D1.5 | Lua 5.4 VM 初始化，scripts/init.lua 执行 | 启动日志检查 | ⬜ |

**负责人：A（架构师）** | 模块：entry/di/platform

### D2 · 背景与图层显示（8 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D2.1 | PNGA/JPG 纹理加载到 TextureManager | loadTexture() 后 isValid=true | ⬜ |
| D2.2 | 纹理在屏幕上正确显示（非纯色、非拉伸） | 视觉检查 | ⬜ |
| D2.3 | BG 图层设置背景 | layers.set_layer_image("bg", texId) | ⬜ |
| D2.4 | FG 图层设置前景（立绘） | layers.set_layer_image("fg", texId) | ⬜ |
| D2.5 | 图层 z-order 正确（BG < FG < MSG） | 多图层同时可见，无遮挡错误 | ⬜ |
| D2.6 | 图层透明度渐变（fadeIn/fadeOut） | opacity 0→1 平滑过渡 | ⬜ |
| D2.7 | 图层缩放/定位 | scale + position 参数生效 | ⬜ |
| D2.8 | dirty-rect 优化不导致丢帧/闪烁 | 快速切换图层，无残留 | ⬜ |

**负责人：B（渲染）** | 模块：render（BgfxLayerManager, TextureManager, BgfxDraw）

### D3 · 文字渲染（8 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D3.1 | FreeType 中文字符正常渲染 | renderText 显示中文 | ⬜ |
| D3.2 | 日文字符（平假名/片假名/汉字）正常 | renderText 显示日文 | ⬜ |
| D3.3 | 文字颜色自定义 | r/g/b/a 参数生效 | ⬜ |
| D3.4 | Ruby 注音（振假名）正常 | renderRuby 上方小字 | ⬜ |
| D3.5 | 文字换行/溢出处理 | 长文本不崩溃 | ⬜ |
| D3.6 | 字体切换 | setFont 后生效 | ⬜ |
| D3.7 | 文字在 VIEW_MAIN 显示（不被图层覆盖） | 文字在图层上方 | ⬜ |
| D3.8 | 文字淡入淡出 | alpha 渐变 | ⬜ |

**负责人：B（渲染）** | 模块：render（FreeTypeContext, BgfxShaderManager）

### D4 · 音频系统（8 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D4.1 | BGM 播放 (MP3/OGG/WAV) | playBGM → isBGMPlaying=true | ⬜ |
| D4.2 | BGM cross-fade 过渡 | stopBGM(fadeTime) 平滑停止 | ⬜ |
| D4.3 | Voice 播放 | playVoice → 中断前一条 | ⬜ |
| D4.4 | SE 音效播放 | playSE → 不中断 BGM/Voice | ⬜ |
| D4.5 | BGM/Voice/SE 三总线独立音量 | setBusVolume 分别生效 | ⬜ |
| D4.6 | Voice 播放完毕回调 | onVoiceComplete 事件 | ⬜ |
| D4.7 | 3D 空间音效基本定位 | playSE3D(x,y,z) | ⬜ |
| D4.8 | 全局音量控制 | setGlobalVolume | ⬜ |

**负责人：C（脚本+功能）** | 模块：audio（SoLoudAudioEngine）

### D5 · KAG 脚本系统（12 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D5.1 | .ks 文件正常 tokenize | tokenizer.parse_file 返回 tokens | ⬜ |
| D5.2 | [bg] 命令切换背景 | KAG: [bg path="classroom.png"] | ⬜ |
| D5.3 | [fg] 命令显示立绘 | KAG: [fg path="hana.png" layer="fg"] | ⬜ |
| D5.4 | [ch] 命令显示文本 | KAG: [ch text="你好"] | ⬜ |
| D5.5 | [p] 命令暂停等待点击 | KAG: [p] | ⬜ |
| D5.6 | [l] / [r] 点击继续/换行 | KAG: [l][r] | ⬜ |
| D5.7 | [jump] 场景内跳转 | KAG: [jump target="label_name"] | ⬜ |
| D5.8 | [call]/[return] 子场景调用 | KAG: [call target="*scene"] | ⬜ |
| D5.9 | [if]/[else]/[endif] 条件分支 | KAG: [if exp="f.flag==1"] | ⬜ |
| D5.10 | [eval] 内嵌 Lua 执行 | KAG: [eval exp="..."] | ⬜ |
| D5.11 | [iscript]...[endiscript] Lua 块 | KAG: 内嵌多行 Lua | ⬜ |
| D5.12 | [emb] 嵌入表达式到文本 | KAG: [emb exp="sf.name"] | ⬜ |

**负责人：C（脚本+功能）** | 模块：script（tokenizer, scheduler, kag_runner, kag.lua）

### D6 · 存档/读档（10 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D6.1 | 手动存档 (slot 1-99) | SaveManager::save() → 文件存在 | ⬜ |
| D6.2 | 手动读档 | SaveManager::load() → 状态恢复 | ⬜ |
| D6.3 | 快速存档 (F5) | quicksave → slot -1 | ⬜ |
| D6.4 | 快速读档 (F6) | quickload → slot -1 | ⬜ |
| D6.5 | 自动存档 (定时/事件触发) | autosave → slot -2 | ⬜ |
| D6.6 | 存档包含完整状态（flags/labels/backlog） | 读档后检查 | ⬜ |
| D6.7 | AES-256-GCM 加密存档正常 | 加密后读档 | ⬜ |
| D6.8 | 未加密存档兼容 | 无密钥读档 | ⬜ |
| D6.9 | Schema Migration 自动升级 | 旧版本存档→新版本可读 | ⬜ |
| D6.10 | 存档列表正确排序（时间/槽位） | listSaves() | ⬜ |

**负责人：C（脚本+功能）** | 模块：storage（SaveManager, ISaveProvider）

### D7 · 转场与特效（6 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D7.1 | crossfade 转场 | transition.start_crossfade | ⬜ |
| D7.2 | wipe 转场（各方向） | transition.start_wipe | ⬜ |
| D7.3 | rule-based 转场（图片规则） | transition.start_rule | ⬜ |
| D7.4 | 转场时长精确 | dur 参数精确到 0.1s 级 | ⬜ |
| D7.5 | 转场可被取消 | cancel_transition | ⬜ |
| D7.6 | VFX 特效（shake/shakex/shakey） | VFX.quake | ⬜ |

**负责人：B（渲染）** | 模块：render（transition, BgfxShaderManager）

### D8 · 资源加载管线（6 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D8.1 | 同步加载纹理（文件系统） | loadTexture("assets/...") | ⬜ |
| D8.2 | 异步加载纹理 | AsyncLoader::enqueue → poll | ⬜ |
| D8.3 | 资源加载失败不崩溃（返回占位纹理） | 加载不存在的文件 | ⬜ |
| D8.4 | 大纹理（>4096px）正常加载 | 加载 4K 图片 | ⬜ |
| D8.5 | 纹理预算 LRU 驱逐正常 | 超预算时自动驱逐旧纹理 | ⬜ |
| D8.6 | 多资源并发加载 | 同时 enqueue 10 个纹理 | ⬜ |

**负责人：B（渲染）** | 模块：render/resource（TextureManager, AsyncLoader）

### D9 · 输入与交互（7 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D9.1 | 鼠标点击 → 推进文本 | 点击 → scheduler resume | ⬜ |
| D9.2 | 键盘 Space/Enter → 推进文本 | 按键 → scheduler resume | ⬜ |
| D9.3 | 右键 → 隐藏文本框 | 右键 toggle | ⬜ |
| D9.4 | KAG↔Game 焦点切换 | InputRouter.setFocus | ⬜ |
| D9.5 | 窗口 resize → 回调正常 | resizeWindow → 通知 | ⬜ |
| D9.6 | Ctrl 快进（skip mode） | Ctrl 按住 → 快速跳过 | ⬜ |
| D9.7 | 回溯 (backlog) | 滚轮上滚 → 显示历史 | ⬜ |

**负责人：A（架构师）** | 模块：input/platform（InputRouter, SDL3）

### D10 · Live2D / 立绘（4 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D10.1 | PNG 立绘正常显示（NullAnimationBackend） | 非 Live2D SDK 模式 | ⬜ |
| D10.2 | 立绘表情切换 | setExpression | ⬜ |
| D10.3 | 立绘透明度控制 | setOpacity | ⬜ |
| D10.4 | 立绘显示/隐藏 | showModel/hideModel | ⬜ |

**负责人：B（渲染）** | 模块：live2d/render

### D11 · 全流程集成（2 项）

| # | 验证项 | 验证方式 | 状态 |
|---|--------|----------|------|
| D11.1 | 完整 demo 脚本从头跑到尾 | demo/galgame_demo.ks 无错误 | ⬜ |
| D11.2 | 跑完脚本后正常退出（无 crash/leak） | 退出码 0，无残留资源 | ⬜ |

**负责人：全员（各自模块验证后，A 集成跑一次）**

---

## 三人分工

### A：架构师 — 核心层（23 项）
**模块**：entry/di/platform/input/job/debug  
**项目**：D1(5) + D9(7) + D11(2) + 负责最终集成测试

### B：渲染 — 视觉层（32 项）  
**模块**：render/resource/live2d  
**项目**：D2(8) + D3(8) + D7(6) + D8(6) + D10(4)

### C：脚本+功能 — 逻辑层（21 项）  
**模块**：script/audio/storage  
**项目**：D4(8) + D5(12) + D6(10) + 同时维护 KAG 命令文档

---

## 共享问题追踪表

审计期间使用共享问题追踪表（当时为 `docs/debug/audit-tracker.md`，审计结束后已删除；最终结果见下方勾选表与 `docs/design/engine-capability-matrix.md`），格式：

```markdown
| ID | 域 | 验证项 | 状态 | 根因 | 修复方式 | 负责人 | 日期 |
|----|-----|--------|------|------|----------|--------|------|
| 1 | D2 | D2.1 纹理加载 | ❌ | bimg格式 | 已修复 | B | 06-18 |
| 2 | D5 | D5.4 ch命令 | ✅ | — | — | C | 06-18 |
```

**规则**：
- 每发现一个问题的根因后**先记录再修**，不在同一轮里修第二个
- A 每天检查一次问题表，发现跨模块问题立即同步其他两人
- 一个模块全部 ✓ 后，A 做一次交叉验证

---

## 执行节奏

| 天 | 行动 |
|----|------|
| **Day 1** | A/B/C 各自领模块，逐项验证，标记 ✓/❌。发现问题只记录不修 |
| **Day 2 AM** | 三人集中评审问题表，按优先级排序，查重复 |
| **Day 2-4** | 各自修复分配的问题，每修一个跑对应测试 |
| **Day 5** | A 集成全流程测试 (D11.1)，B/C 交叉验证 |
| **Day 5-6** | 三轮回归：修→跑全流程→发现新问题→修 |
| **Day 7** | 全绿冻结，跑 `python scripts/count_coupling.py --ci` |
