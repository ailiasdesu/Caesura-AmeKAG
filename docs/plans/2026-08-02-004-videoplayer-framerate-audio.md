# 2026-08-02-004-videoplayer-framerate-audio.md — VideoPlayer 帧率与音频修复执行记录

## 背景

迭代 8 终检发现（#3）：VideoPlayer::update() 两条路径均 (void)dt，每次调用只解码一帧且无时间戳步进 —— 25fps 视频在 60Hz 下以 2.4 倍速播放；pl_mpeg 路径 plm_decode_audio 的结果被丢弃，视频完全无声。调研另发现 video_update 的 Lua 绑定无调用者，视频从未被驱动解码（死链）。

## 修复内容

### Phase 1 — IAudioBackend.playRawPCM（音频通道）
- src/audio/api/IAudioBackend.h：新增纯虚 playRawPCM
- SoLoudAudioEngine：实现（Wav::loadRawWave 拷贝模式 + SE bus + m_rawWaveCache 保活）；NullAudioBackend 与测试 mock 补实现
- tests/cpp/test_audio.cpp：新增 playRawPCM 用例

### Phase 2 — pl_mpeg 音频接线
- VideoState 加音频字段；onAudioDecoded（void* 签名）公开、drainAudio private
- open()：plm_set_audio_decode_callback + plm_set_audio_enabled
- onAudioDecoded 按 plm 指针匹配入队（m_audioMutex）；update() 调 drainAudio 分块喂 playRawPCM
- close()：停播 + 清队列

### Phase 3 — 帧率时间戳步进
- VideoState 加 frameRate/playhead；open 时 plm_get_framerate（默认 30）
- update：dt 累加 playhead，worker 循环解码直到 plm_get_time 追上（限 30 帧/调用）
- seek() 重置 playhead

### Phase 4 — 引擎自动驱动 + 文档
- IVideoPlayer::updateAll(dt) 新增（快照遍历防迭代器失效）
- Engine::render() 开头调 updateAll(dt) —— 引擎帧循环自动推进
- docs/api/lua-modules.md video_update 说明更新

## 遗留

- FFmpeg 路径 pts 步进：本地无法验证（FFmpeg 未启用），仍每帧 1 帧
- video_play 的 loop/volume 参数：绑定签名忽略第 2 参，需接口扩展
- drainAudio 块大小按 1 秒/块，真机音频延迟需调参

## 验证

- build-repro-verify Debug 零错误；CaesuraTests 567/567（+1）、2788 assertions、SUCCESS
- ctest 10/10、耦合度 PASS

---

## 遗留解决（2026-08-02，提交 1125d17e）

- **FFmpeg pts 步进**：frameRate（avg/r_frame_rate）+ playhead + 30 帧限步 + 保留 pts≤playhead 最后一帧（review blocking 修复：不丢未到期帧防冻结）；loop 重绕 + seek 重置 playhead
- **loop/volume 参数**：IVideoPlayer::setLoop/setVolume + 绑定解析 {loop, volume}；plm_set_loop / FFmpeg EOF 重绕；volume clamp [0,1] 应用
- **drainAudio 可测化**：planDrain 纯函数 + 单测（568/568 +1）；锁不变量注释
- **3 个 LOW**：insert 同 vector UB 修复、audioHandles 上限 4、close 无锁 erase 注释说明
