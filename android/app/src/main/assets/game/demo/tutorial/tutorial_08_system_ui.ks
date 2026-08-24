; =============================================================================
; Caesura KAG 教程 08 —— 系统 UI：画廊 / 音乐室 / 历史 / 章节
;
; 目标：掌握系统界面命令：CG 画廊（[gallery]）、音乐室（[music]）、
;       历史回看（[history]）、章节选择（[chapter]）与内容解锁（[unlock]）。
;
; 概念：
;   - [unlock type="cg" id=X] 解锁一张 CG；type="music" 解锁音乐
;   - [gallery] 打开画廊（浏览已解锁 CG）
;   - [music] 打开音乐室（试听已解锁 BGM）
;   - [history] 打开历史回看（backlog 覆盖层）
;   - [chapter id= label=] 章节选择界面（跳转到对应 *label）
;
; 注意：这些命令在桌面引擎会打开 UI 覆盖层；在 Web 播放器中
;       系统 UI 模块被桩化（打开调用安全跳过），剧本仍完整运行。
;
; 本教程覆盖命令：[unlock] [gallery] [music] [history] [chapter]
;               [bg] [ch] [p] [wait] [end]
; =============================================================================

[font face="default" size=22]
[pt speed=60]

[bg storage="assets/bg/hana.png"]
[wait time=300]

; ---- 1. 解锁内容 -----------------------------------------------------------
; 剧情推进到关键点时可解锁画廊 CG 与音乐室曲目。
[ch name="Narrator" text="剧情推进！解锁了一张 CG 与一首 BGM。"]
[unlock type="cg" id="scene_hana"]
[unlock type="music" id="daily_wav"]
[p]

; ---- 2. 打开画廊 -----------------------------------------------------------
; [gallery] 打开 CG 画廊。玩家可以用方向键浏览已解锁的图片。
[ch name="Narrator" text="按 G 键试试打开画廊（[gallery]）。"]
[p]
[gallery id="scene_hana"]

; ---- 3. 音乐室 -------------------------------------------------------------
; [music] 打开音乐室，试听解锁过的 BGM。
[ch name="Narrator" text="按 M 键打开音乐室（[music]）。"]
[p]
[music]

; ---- 4. 历史回看 -----------------------------------------------------------
; [history] 打开历史记录（backlog 覆盖层），回看之前的台词。
[ch name="Narrator" text="按 H 键打开历史记录（[history]）。"]
[p]
[history]

; ---- 5. 章节选择 -----------------------------------------------------------
; [chapter] 打开章节选择；跳转到对应 *chapter_* 标签。
[ch name="Narrator" text="按 C 键打开章节选择（[chapter]）。"]
[p]
[chapter id="end" label="*end"]

; ---- 6. 收尾 ---------------------------------------------------------------
[ch name="Narrator" text="教程 08 完成——系统界面。你已经掌握了 Caesura 剧本语言的这部分基础。"]
[p]
[end]
