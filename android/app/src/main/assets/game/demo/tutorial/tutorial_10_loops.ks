; =============================================================================
; Caesura KAG 教程 10 —— 循环控制流（[for] / [while]）
;
; 目标：掌握有界循环与循环变量。
;
;   [for var="i" start="1" end="3"] ... [endfor]
;       计数循环：var 指定循环变量（写入 f 表，用裸名），start/end/step
;       支持表达式，步长可为负（step="-1" 倒序）。
;   [while exp="f.n > 0"] ... [endwhile]
;       条件循环：每轮检查表达式（TJS 运算符可用）。
;   [eval exp="f.n = f.n - 1"]
;       表达式赋值（[set] 不做表达式求值，递减请用 [eval]）。
;
; 安全：所有循环都有每场景 65536 次迭代守卫，死循环会被截断而非挂死。
; 交互：循环体内配合 [ch]+[p] 逐轮点击推进（Web 播放器同样适用）。
;
; 本教程覆盖命令：[ch] [p] [for] [endfor] [while] [endwhile] [eval] [set] [end]
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. [for] 正序计数 ------------------------------------------------
[ch name="Narrator" text="循环一：正序计数。"]
[p]
[for var="i" start="1" end="3"]
[ch name="Narrator" text="第 ${f.i} 次（i=$f.i）"]
[p]
[endfor]
[ch name="Narrator" text="循环一结束：i=$f.i（已越界）"]
[p]

; ---- 2. [for] 倒序步长 ------------------------------------------------
[ch name="Narrator" text="循环二：倒序（step=-1）。"]
[p]
[for var="j" start="3" end="1" step="-1"]
[ch name="Narrator" text="倒数第 $f.j 次"]
[p]
[endfor]
[ch name="Narrator" text="循环二结束：j=$f.j"]
[p]

; ---- 3. [while] 条件循环 + [eval] 递减 --------------------------------
[set f.n = 3]
[ch name="Narrator" text="循环三：while 递减。"]
[p]
[while exp="f.n > 0"]
[ch name="Narrator" text="还剩 $f.n 次"]
[p]
[eval exp="f.n = f.n - 1"]
[endwhile]
[ch name="Narrator" text="循环三结束：n=$f.n"]
[p]

; ---- 4. 收尾 ----------------------------------------------------------
[ch name="Narrator" text="循环教程完成！i=$f.i j=$f.j n=$f.n"]
[p]
[end]
