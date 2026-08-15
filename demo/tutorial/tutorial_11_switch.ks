; =============================================================================
; Caesura KAG 教程 11 —— 多路分支（[switch]）
;
; 目标：掌握 [switch] 两种选择器与 case 匹配规则。
;
;   [switch mode]                 裸参数 = 变量名（KAG3 兼容，查 f 表）
;   [switch exp="f.tier"]         表达式选择器（round 55，TJS 运算符可用）
;   [case 值] ... [default] ... [endswitch]
;       case 按 tostring 等值匹配；命中后不落入后续 case（无 fallthrough）；
;       无匹配走 [default]（可省略）；表达式失败/nil 时同样走 default。
;
; 本教程覆盖命令：[set] [ch] [p] [switch] [case] [default] [endswitch] [end]
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. 裸变量形式（KAG3 兼容）-------------------------------------------
[set f.mode = "fast"]
[ch name="Narrator" text="模式：$f.mode。"]
[p]
[switch mode]
[case fast]
[ch name="Narrator" text="选择了快速模式（裸变量）"]
[p]
[case slow]
[ch name="Narrator" text="选择了慢速模式"]
[p]
[default]
[ch name="Narrator" text="未知模式"]
[p]
[endswitch]
[ch name="Narrator" text="switch 一结束。"]
[p]

; ---- 2. 表达式选择器：数值匹配 --------------------------------------------
[set f.tier = 2]
[switch exp="f.tier"]
[case 1]
[ch name="Narrator" text="Tier 1（新手）"]
[p]
[case 2]
[ch name="Narrator" text="Tier 2（老兵）"]
[p]
[case 3]
[ch name="Narrator" text="Tier 3（精英）"]
[p]
[endswitch]

; ---- 3. 表达式选择器：布尔条件（TJS 运算符）-------------------------------
[set f.gold = 150]
[set f.level = 6]
[switch exp="f.gold >= 100 && f.level > 5"]
[case true]
[ch name="Narrator" text="高段位玩家！"]
[p]
[case false]
[ch name="Narrator" text="继续加油"]
[p]
[endswitch]

; ---- 4. 缺失变量 → default ------------------------------------------------
[switch exp="f.missing"]
[case a]
[ch name="Narrator" text="不会走到这里"]
[p]
[default]
[ch name="Narrator" text="变量缺失走了 default"]
[p]
[endswitch]

; ---- 5. 收尾 --------------------------------------------------------------
[ch name="Narrator" text="switch 教程完成！"]
[p]
[end]
