; =============================================================================
; Caesura KAG 教程 07 —— 存档与读档
;
; 目标：掌握存档系统：保存（[save]）、读取（[load]）、存档结果判断
;       （tf.save_result / tf.load_result）。
;
; 概念：
;   - 存档槽位 slot：-2..99（通常 0..99 为手动槽位）
;   - [save slot=N] 把当前场景/变量/解锁状态写入存档
;   - [load slot=N] 从存档恢复；成功后变量/跳转目标恢复
;   - 引擎无存档后端时（如 Web 播放器），save/load 返回 error，
;     剧本应通过 tf.save_result / tf.load_result 优雅降级
;
; 本教程覆盖命令：[save] [load] [set] [if] [else] [endif] [ch] [p] [end]
; =============================================================================

[font face="default" size=22]
[pt speed=60]

[bg storage="assets/bg/classroom.png"]
[wait time=300]

; ---- 1. 保存到槽位 ---------------------------------------------------------
; [save] 是阻塞命令：写入完成后继续。结果在 tf.save_result（"ok"/"error"）。
[ch name="Narrator" text="先把进度保存到槽位 1。"]
[p]
[save slot=1]

[if exp="tf.save_result == 'ok'"]
[ch name="Narrator" text="保存成功！存档槽位 1 已写入。"]
[p]
[else]
[ch name="Narrator" text="当前环境不支持存档（Web 播放器），剧本继续运行。"]
[p]
[endif]

; ---- 2. 改变状态后读档 -----------------------------------------------------
; 先改一个变量，再读档 —— 读档成功后变量会被存档里的值覆盖。
[set f.coins = 100]
[ch name="Narrator" text="我捡到了 100 枚金币（f.coins=100）。现在读档……"]
[p]
[load slot=1]

[if exp="tf.load_result == 'ok'"]
[ch name="Narrator" text="读档成功！变量恢复为存档时的值。"]
[p]
[else]
[ch name="Narrator" text="读档不可用（错误标记 tf.load_result=error），金币保持 100。"]
[p]
[endif]

; ---- 3. 读档结果后的条件逻辑 ----------------------------------------------
; 生产剧本常用模式：load 失败时跳到安全标签，避免恢复半状态。
[if exp="tf.load_result == 'error'"]
[ch name="Narrator" text="（示例：失败路径可 [jump target=*safe] 跳转到安全场景。）"]
[p]
[endif]

; ---- 4. 收尾 ---------------------------------------------------------------
[ch name="Narrator" text="存档教程完成！下一个教程：系统 UI（tutorial_08）。"]
[p]
[end]
