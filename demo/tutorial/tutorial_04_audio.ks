; =============================================================================
; Caesura KAG 教程 04 —— 音频命令
;
; 目标：掌握三总线音频系统：BGM（背景音乐）、SE（音效）、Voice（语音）。
; 每条总线可以独立控制音量、独立停止。
;
; 本教程覆盖命令：[playbgm] [setbgmvolume] [playse] [ch voice=] [voice_wait]
;               [fadebgm] [xfadebgm] [stopbgm] [stopse] [end]
;
; 注意：音频文件放在 assets/bgm/、assets/se/、assets/voice/。
;       引擎支持 wav/ogg 格式（本仓库示例提供 .wav 与 .ogg 双份）。
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. 播放 BGM（循环） ---------------------------------------------------
; [playbgm] 启动背景音乐。loop 默认 true（循环播放），volume 是 0..1.5 的音量。
[bg storage="assets/bg/classroom.png"]
[playbgm storage="assets/bgm/daily.wav" volume=0.7]
[wait time=300]
[ch name="Narrator" text="背景音乐开始播放了（daily.wav，音量 70%）。"]
[p]

; ---- 2. 调整 BGM 音量 ------------------------------------------------------
[setbgmvolume volume=0.3]
[ch name="Narrator" text="BGM 音量降到 30%，适合角色说话时压低音乐。"]
[p]
[setbgmvolume volume=0.7]
[ch name="Narrator" text="音量恢复 70%。"]
[p]

; ---- 3. 音效 SE ------------------------------------------------------------
; [playse] 播放一次性音效（点击、提示音等），默认不循环。
[playse storage="assets/se/click.wav"]
[ch name="Narrator" text="你听到了一声点击音效。"]
[p]

; ---- 4. 台词语音 Voice -----------------------------------------------------
; [ch] 的 voice 参数播放台词配音；[voice_wait] 让剧本等待语音播完。
[ch name="Sakura" text="这是我的台词配音！"
     voice="assets/voice/line01.wav" sprite="assets/fg/girl_uniform.png"]
[voice_wait]
[p]

; ---- 5. BGM 淡入淡出 -------------------------------------------------------
; [fadebgm] 把当前 BGM 音量渐变到 volume（time 为渐变时长）。
[fadebgm volume=0.5 time=800]
[ch name="Narrator" text="BGM 音量在 800ms 内渐变到了 50%。"]
[p]
; [xfadebgm] 交叉淡化换曲：旧曲淡出、新曲淡入（time 为过渡时长）。
[xfadebgm storage="assets/bgm/daily.wav" time=800]
[ch name="Narrator" text="交叉淡化结束，BGM 重新响起。"]
[p]

; ---- 6. 停止音频 -----------------------------------------------------------
; [stopse] 停止音效；[stopbgm] 停止背景音乐（time 可带淡出）。
[stopse]
[stopbgm time=500]
[ch name="Narrator" text="所有音频都停了。"]
[p]

; ---- 7. 收尾 ---------------------------------------------------------------
[ch name="Narrator" text="音频教程完成！下一个教程：分支与变量（tutorial_05）。"]
[p]
[end]
