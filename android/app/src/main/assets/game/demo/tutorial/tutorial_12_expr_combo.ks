; =============================================================================
; Caesura KAG 教程 12 —— 表达式组合实战
;
; 综合运用 round 53-63 的表达式能力：
;   - [eval exp="f.arr = {10, 20, 30}"]  表达式赋值（可建表）
;   - ${f.arr[f.flag ? 1 : 2]}             三元在索引内（round 61）
;   - [switch exp="f.tier ?? 1"]          ?? 空合并 + switch 表达式
;   - [for] 内 ${...}                      循环与插值组合
;   - [eval exp="f.total = ..."]         计算综合
;
; 本教程覆盖命令：[set] [eval] [ch] [p] [switch] [case] [endswitch] [for] [endfor] [end]
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. 数据准备 ---------------------------------------------------------
[eval exp="f.arr = {10, 20, 30}"]
[set f.flag = true]
[set f.tier = 2]
[ch name="Narrator" text="数据就绪：数组与变量已设置。"]
[p]

; ---- 2. 三元在索引内 -----------------------------------------------------
[ch name="Narrator" text="flag 为真取第一项：${f.arr[f.flag ? 1 : 2]}"]
[p]
[set f.flag = false]
[ch name="Narrator" text="flag 为假取第二项：${f.arr[f.flag ? 1 : 2]}"]
[p]

; ---- 3. ?? 空合并 + switch 表达式 ----------------------------------------
[switch exp="f.tier ?? 1"]
[case 1]
[ch name="Narrator" text="等级 1：新手（tier 缺失时走这里）"]
[p]
[case 2]
[ch name="Narrator" text="等级 2：老兵"]
[p]
[endswitch]

; ---- 4. 循环 + 插值组合 --------------------------------------------------
[for var="i" start="1" end="3"]
[ch name="Narrator" text="第 $f.i 项：${f.arr[f.i > 1 ? f.i : 1]}"]
[p]
[endfor]

; ---- 5. eval 综合计算 ----------------------------------------------------
[eval exp="f.total = f.arr[1] + (f.flag ? f.arr[2] : f.arr[3])"]
[ch name="Narrator" text="合计：$f.total（10 + 30，flag 已为假）"]
[p]

; ---- 6. 收尾 --------------------------------------------------------------
[ch name="Narrator" text="表达式组合教程完成！"]
[p]
[end]
