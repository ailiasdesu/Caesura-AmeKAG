; =============================================================================
; Caesura KAG 教程 05 —— 变量、分支与跳转
;
; 目标：掌握剧本逻辑：变量赋值（[set]）、条件分支（[if]/[else]/[endif]）、
;       标签跳转（[jump]）。
;
; 变量命名空间：
;   - f.xxx  = 全局变量（跨场景持久）
;   - sf.xxx = 场景内变量
;   - tf.xxx = 临时变量
;   - 表达式写法：f.luck = math.random(2)
;
; 本教程覆盖命令：[set] [if exp=] [else] [endif] [jump target=*label]
;               [wait] [ch] [p] [end]
; =============================================================================

[font face="default" size=22]
[pt speed=60]

[bg storage="assets/bg/classroom.png"]
[wait time=300]

; ---- 1. 变量赋值 -----------------------------------------------------------
; [set] 计算右侧表达式并存入变量。math.random(2) 返回 1 或 2（抛硬币）。
[set f.luck = math.random(2)]

; ---- 2. 条件分支 -----------------------------------------------------------
; [if exp=...] 表达式为真时执行分支体，否则走 [else]。
[if exp="f.luck == 1"]
[ch name="Narrator" text="硬币正面朝上！今天运气不错。"]
[p]
[else]
[ch name="Narrator" text="硬币反面朝上……看来是普通的一天。"]
[p]
[endif]

; ---- 3. 标签与跳转 ---------------------------------------------------------
; *label 定义标签；[jump target=*label] 跳到该标签继续执行。
; 这里用变量决定走哪个结局分支。
[set f.path = "A"]

[if exp="f.path == 'A'"]
[jump target=*path_a]
[else]
[jump target=*path_b]
[endif]

*path_a
[ch name="Narrator" text="你走了路线 A（绿色通道）。"]
[p]
[jump target=*ending]

*path_b
[ch name="Narrator" text="你走了路线 B（红色通道）。"]
[p]

*ending
; ---- 4. 收尾 ---------------------------------------------------------------
[ch name="Narrator" text="无论哪条路，最后都汇合到了这里——分支教学完成！"]
[p]
[ch name="Narrator" text="下一个教程：特效与转场（tutorial_06）。"]
[p]
[end]
