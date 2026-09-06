---
module: audio
tags: [soloud, save-restore, seek, ogg, wav, ownership]
problem_type: runtime-crash
---

# SoLoud 流式音频恢复的格式分派与样本游标

U11 新增 BGM 恢复时，真实混音器读取已准备的 WAV 并跳转到 0.25 秒，出现崩溃；另一轮相同路径返回约 429 万秒的位置。CPU 读取与准备成功不能证明播放实例的 seek 正确。

`WavStreamInstance::mCodec` 是 union。旧 seek 只判断 `mCodec.mOgg` 非空，WAV/FLAC/MP3 的有效指针也满足这个条件，因而被传入 Vorbis API。修复先检查 `mParent->mFiletype == WAVSTREAM_OGG`；其他格式使用原有通用 seek 路径。

随后 Ogg 回归揭示第二条独立问题：底层 seek 返回值被忽略，位置查询使用了 vendored stb 明确说明在 pull-data seek 后不可靠的 `get_sample_offset`，读取又继续走 `get_frame_float` 与旧帧缓存。结果不仅是显示位置偏差：实际 PCM 片段也与连续解码的同一位置不一致。

修复遵守 vendored `stb_vorbis.c` 的调用契约：

- 整数转换前验证目标采样点范围，并传播 seek 失败。
- seek 成功后记录目标采样点，后续使用 planar `get_samples_float` 读取。
- 移除本地 Ogg 帧缓存，让解码器管理其剩余样本，避免输出旧位置的缓存。

验证位于 `tests/cpp/test_audio_restore.cpp`：WAV/Ogg/FLAC 准备与播放、错误位置拒绝、配额清理，以及先读部分帧再 seek 的真实 PCM 对照。对照来自完整解码后的非静音片段，逐声道比较，不只断言位置数字。旧实现最大样本误差为 0.0181327，修复后低于 0.00001。

原始失败及修复日志：`artifacts/validation/u11-restore/audio-prepare-green.log`、`audio-restore-trace.log`、`ogg-seek-red.log`、`ogg-pcm-red.log`、`ogg-pcm-green.log`。定向 5 用例/78 断言通过；后续完整 C++ 1255/1255 通过的记录为 `audio-cpp-full-2.json` 及同名日志。

这些证据验证混音器/解码输出及恢复生命周期，不代表声卡实际输出、各平台设备验证或整页冷启动作品验收。第三方原许可证保留，局部改动在源码中注明。
