# C 域排查执行指南

> 负责人：C（脚本+功能） | 21 项 | 覆盖 D4+D5+D6 | 基于代码审查完成

## 前置条件

- 引擎可正常构建 (`cmake --build build --config Debug --parallel`)
- 测试全绿 (`cd build/tests/Debug && ./CaesuraTests.exe`)
- 飞书 Base 已打开: https://mcn95ia2oj1a.feishu.cn/base/SKH2bMea7aF0TlsykBkcRdM1nWH
- 了解 Lua 5.4 + KAG 脚本系统

## 各域当前状态

### D4 音频系统 (7/8 通过)

| ID | 验证项 | 状态 | 说明 |
|----|--------|------|------|
| D4.1 | BGM播放 | ✅ | SoLoudAudioEngine::playBGM + cross-fade |
| D4.2 | BGM cross-fade | ✅ | fadeVolume + scheduleStop |
| D4.3 | Voice播放 | ✅ | playVoice + auto-stop前一条 |
| D4.4 | SE音效 | ✅ | playSE + handle追踪 |
| D4.5 | 总线独立音量 | ✅ | setBusVolume(BGM/Voice/SE) |
| D4.6 | Voice回调 | P2 | _CAESURA_VOICE_COMPLETE仅有白名单 |
| D4.7 | 3D音效 | ✅ | playSE3D + update3dListener |
| D4.8 | 全局音量 | ✅ | setGlobalVolume/getGlobalVolume |

**待验证的运行时项（需要音频文件）**：
- D4.1: 启动引擎 → demo → 确认BGM播放 (assets/bgm/daily.ogg)
- D4.3: 确认语音播放 (assets/voice/)
- D4.4: 确认SE播放 (assets/se/)
- D4.5: 修改音量 → setBusVolume("bgm", 0.5) → 确认音量改变
- D4.7: playSE3D(x,y,z) → 确认3D定位
- D4.8: setGlobalVolume(0.5) → 确认全局音量改变

### D5 KAG脚本系统 (12/12 通过 ✅)

所有12项已通过代码审查，无需额外运行时验证。

| ID | 验证项 | 实现位置 |
|----|--------|----------|
| D5.1 | .ks tokenize | tokenizer.lua LPeg解析器 |
| D5.2 | [bg]命令 | scripts/kag/commands/layer.lua |
| D5.3 | [fg]命令 | scripts/kag/commands/layer.lua |
| D5.4 | [ch]命令 | scripts/kag/commands/text.lua |
| D5.5 | [p]命令 | scheduler等待点击 |
| D5.6 | [l]/[r] | scripts/kag/commands/text.lua |
| D5.7 | [jump] | scheduler.lua jump处理 |
| D5.8 | [call]/[return] | scheduler.lua call_stack |
| D5.9 | [if]/[else] | scheduler.lua条件分支 |
| D5.10 | [eval] | scheduler.lua unified scope |
| D5.11 | [iscript] | scheduler.lua sandbox |
| D5.12 | [emb] | tokenizer支持emb |

**建议**：写一份简单的 .ks 脚本测试这12个命令，从tokenize到执行全链路。

### D6 存档/读档 (10/10 通过 ✅)

| ID | 验证项 | 实现位置 |
|----|--------|----------|
| D6.1 | 手动存档 | SaveManager::save() JSON+AES |
| D6.2 | 手动读档 | SaveManager::load() + migration |
| D6.3 | 快速存档F5 | quicksave全局 (slot -1) |
| D6.4 | 快速读档F6 | quickload全局 (slot -1) |
| D6.5 | 自动存档 | autosave全局 (slot -2) |
| D6.6 | 完整状态序列化 | flags/labels/backlog/call_stack |
| D6.7 | AES加密 | AES-256-GCM + CAES magic |
| D6.8 | 未加密兼容 | 无加密key时直接读 |
| D6.9 | Schema Migration | v1→v5内置迁移 (4步) |
| D6.10 | 存档列表 | listSaves()按时间排序 |

**待验证的运行时项**：
- D6.1/D6.2: 运行demo → 存档到一个slot → 重新启动 → 读档 → 确认状态恢复
- D6.3/D6.4: 运行demo → 按F5快速存档 → 按F6快速读档
- D6.7/D6.8: setEncryptionKey → save → load → 解密成功
- D6.9: 创建v4格式存档 → 加载 → 确认自动迁移到v5

## 排查 SOP

1. 打开飞书Base → 筛选负责人=C → 筛选状态≠通过
2. D5和D6已经是 ✅ 全通过——只需要做运行时验证确认没有遗漏
3. D4需要实际音频文件验证（至少需要 BGM/Voice/SE 各一个）
4. 通过 → 在飞书记为 ✅ 通过
5. 失败 → 记录现象和根因 → 记入 audit-tracker.md

## 关键源文件

```
src/audio/SoLoudAudioEngine.cpp       - 音频引擎实现
src/audio/api/IAudioBackend.h         - 音频接口
src/storage/SaveManager.cpp           - 存档管理
src/storage/api/ISaveManager.h        - 存档接口
scripts/scheduler.lua                 - KAG调度器
scripts/tokenizer.lua                 - KAG解析器
scripts/kag_runner.lua                - 上下文管理
scripts/kag.lua                       - Lua↔KAG桥接
scripts/kag/commands/                 - 9类68个命令实现
scripts/system.lua                    - 系统设施(save/load/config)
scripts/flow.lua                      - 场景流控制
```

## KAG命令文档维护

C同时负责KAG命令文档维护。在排查过程中：
- 遇到未文档化的命令行为 → 记录到 docs/api/kag-commands.md
- 发现文档与实际行为不符 → 以实际行为为准更新文档
- 新增命令 → 按文档模板添加

## 已知P2后续

- D4.6: Voice回调 — 需要C++侧添加回调机制 + Lua侧注册回调函数
- D2.6/D3.8: layer/text fade — 需要Lua层或transition系统特效支持
