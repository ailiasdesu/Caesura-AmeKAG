; =============================================================================
;  Caesura (AmeKAG) — full_story.ks
;  Alpha 端到端测试剧本 — 覆盖全部 P0+P1 KAG 标签
; =============================================================================

; ── 启动 ──────────────────────────────────────────────────────────────────
*start
[font face="default" size=24 color="#ffffff"]
[pt speed=50]
[ch name="系统" text="Caesura Alpha 端到端测试开始。"]

; ── 测试1: 文本系统 [ch][text][l][r][er][p] ──────────────────────────────
*test_text
[ch name="旁白" text="这是角色对话测试。"]

[text text="这是旁白叙述文本，无角色名。"]

[l]  ; 换行
[r]  ; 回车
[er] ; 清空文本层
[ch name="旁白" text="文本层已清空，这是一条新消息。"]

[p]  ; 等待点击

; ── 测试2: 图层系统 [bg][fg][cl][image] ──────────────────────────────────
*test_layers
[ch name="旁白" text="正在加载图层..."]

[bg storage="test_bg.png"]
[wait time=300]
[ch name="旁白" text="背景已设置。"]

[fg storage="test_chara.png"]
[wait time=300]
[ch name="旁白" text="前景立绘已设置。"]

[image layer="fg" storage="test_chara.png"]
[ch name="旁白" text="image 命令执行完毕。"]

[cl layer="fg"]
[ch name="旁白" text="fg 图层已清除。"]

; ── 测试3: 图层定位 [position][layopt] ───────────────────────────────────
*test_position
[fg storage="test_chara.png"]
[position layer="fg0" x=0.3 y=0.5 scale=1.0]
[ch name="主角" text="我站在画面左侧 (x=0.3, y=0.5)。"]

[layopt layer="fg0" opacity=0.6]
[ch name="旁白" text="立绘半透明 (opacity=0.6)。"]

[layopt layer="fg0" opacity=1.0 visible=true]
[ch name="旁白" text="立绘恢复不透明。"]

; ── 测试4: 排版增强 [ruby][font][reset][pt] ──────────────────────────────
*test_typography
[ruby text="漢字" ruby="かんじ"]
[ch name="旁白" text="上方应显示 ruby 注音。"]

[font face="Noto Serif" size=28 color="#ffcc00"]
[ch name="旁白" text="这段使用 Noto Serif 28px 金色字体。"]

[reset]
[ch name="旁白" text="字体已重置为默认。"]

[pt speed=150]
[ch name="旁白" text="打字机速度改为150ms/字。"]

[pt speed=50]
[ch name="旁白" text="打字机速度恢复50ms/字。"]

; ── 测试5: 音频系统 [playbgm][fadebgm][playse][playvoice][stopbgm][stopse] 
*test_audio
[playbgm storage="bgm/test.ogg" volume=0.8]
[ch name="旁白" text="BGM 开始播放。"]

[wait time=300]

[fadebgm volume=0.3 time=500]
[ch name="旁白" text="BGM 音量淡出到 0.3。"]

[playse storage="se/click.wav" volume=0.5]
[ch name="旁白" text="SE 已触发。"]

[stopse]
[ch name="旁白" text="SE 已停止。"]

[playvoice storage="voice/line001.ogg"]
[ch name="主角" text="这条消息附带语音。"]

[stopbgm fadeout=500]
[ch name="旁白" text="BGM 已淡出停止。"]

; ── 测试6: 转场特效 [trans][move][quake][fade] ───────────────────────────
*test_transitions
[ch name="旁白" text="执行转场效果..."]

[trans time=500 kind="crossfade"]
[ch name="旁白" text="crossfade 转场完成。"]

[move layer="fg0" x=0.7 y=0.5 time=500 easing="ease-in-out"]
[ch name="主角" text="立绘已滑动到右侧 (x=0.7)。"]

[quake time=300 intensity=5]
[ch name="旁白" text="屏幕震动效果。"]

[fade layer="fg0" time=300 from=255 to=100]
[ch name="旁白" text="立绘淡出中..."]

; ── 测试7: 视频系统 [video][stopvideo] ───────────────────────────────────
*test_video
[ch name="旁白" text="播放测试视频..."]

[video storage="movie/test.mpg"]
[wait time=500]

[stopvideo]
[ch name="旁白" text="视频已停止。"]

; ── 测试8: VFX 特效 [vfx] ────────────────────────────────────────────────
*test_vfx
[ch name="旁白" text="粒子特效测试..."]

[vfx type="shake" time=300 intensity=3]
[ch name="旁白" text="shake 震动特效结束。"]

[vfx type="flash" time=200 color="#ffffff" opacity=0.5]
[ch name="旁白" text="flash 闪白特效结束。"]

[vfx type="stop"]
[ch name="旁白" text="VFX 特效已停止。"]

; ── 测试9: 资源预加载 [preload] ───────────────────────────────────────────
*test_preload
[ch name="旁白" text="预加载资源..."]

[preload type="texture" storage="test_bg.png" wait="true"]
[preload type="texture" storage="test_chara.png"]
[ch name="旁白" text="预加载完成。"]

; ── 测试10: 存档系统 [save][load][saveplace][loadplace] ──────────────────
*test_save
[saveplace]
[ch name="旁白" text="临时书签已保存。"]

[ch name="主角" text="继续对话至分支点。"]
[wait time=200]

[loadplace]
[ch name="主角" text="从临时书签恢复成功。"]

[save slot=1]
[ch name="旁白" text="存档1已保存。"]

[ch name="主角" text="存档后的新对话。"]

[load slot=1]
[ch name="旁白" text="读档1完成——回到存档点。"]

; ── 测试11: 流程控制 [jump][call][return][if][link] ──────────────────────
*test_flow
[ch name="旁白" text="测试流程控制命令。"]

[emb exp="ctx.f.visited_test_flow = true"]
[ch name="旁白" text="emb 已设置 f.visited_test_flow = true。"]

[if exp="ctx.f.visited_test_flow == true"]
[jump target="*flow_ok"]
[endif]

[jump target="*flow_fail"]

*flow_ok
[ch name="旁白" text="if/jump 流程控制正确。"]

[call target="*sub_scene"]
[ch name="旁白" text="从子场景返回——call/return 正常。"]

[jump target="*scene_b"]

*flow_fail
[ch name="旁白" text="流程控制测试失败！"]
[end]

*sub_scene
[ch name="配角" text="这是 call 调用的子场景。"]
[return]

; ── 测试12: 跳转与历史 [history][skip] ───────────────────────────────────
*scene_b
[ch name="旁白" text="已跳转到场景B——jump 正常。"]

[skip]
[ch name="旁白" text="skip 模式开启—自动推进。"]
[ch name="旁白" text="跳过模式中的第二条。"]
[skip]
[ch name="旁白" text="skip 模式关闭。"]

[history]
[ch name="旁白" text="Backlog 已查看。"]

; ── 测试13: 变量与运算 [eval] ────────────────────────────────────────────
*test_eval
[emb exp="ctx.f.score = 100"]
[eval exp="ctx.f.score + 50"]
[emb exp="ctx.tf.bonus = ctx.tf.eval_result or 0"]
[ch name="旁白" text="eval 表达式已执行。"]

; ── 测试14: 边界情况 ─────────────────────────────────────────────────────
*test_edge
[ch name="\"特殊字符\"" text="含引号的名称和对话内容。"]

[wait time=100]
[ch name="旁白" text="短暂等待成功。"]

[l][l]  ; 连续换行
[ch name="旁白" text="连续换行后仍有此文本。"]

; ── 结束 ─────────────────────────────────────────────────────────────────
*end_game
[ch name="系统" text="=========================================="]
[ch name="系统" text="Caesura Alpha 全部标签测试通过。"]
[ch name="系统" text="=========================================="]

[emb exp="ctx.tf.test_complete = true"]
[end]

