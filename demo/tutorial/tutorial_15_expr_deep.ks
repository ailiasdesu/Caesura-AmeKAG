; =============================================================================
; Caesura KAG 教程 15 —— 高级表达式实战（round 84）
;
; 深化 round 53-84 表达式语言的进阶形态：
;   - 嵌套三元  f.a ? f.b : (f.c ? f.d : f.e)
;     （round 68 括号内三元翻译：translate_parens 处理嵌套/内层分组）
;   - 多参函数调用 + 逗号分段  f.calc(f.flag ? 8 : 2, 5)
;     （round 84 修复：顶层逗号把参数表分段，三元留在自己的实参内，
;       不会把尾随实参吞进 else 分支）
;   - ?? 空合并  f.a ?? f.b ?? 99（nil/false 回退，0/"" 存活）
;   - [eval] 三元赋值  f.tier = f.hp > 50 ? "high" : "low"
;     （round 61/68 translateAssignment：首个顶层 = 拆 LHS，RHS 全管道翻译）
;   - 作用域前缀  sf./tf./mp./lf.
;   - 数值边界  1e3 / 0xFF / .5 / 大整数
;
; 说明：同前 14 个教程，通过 ks_check 静态校验（零警告）+ ks_bake 编译 +
;   Web 播放器一路运行到 [end] 零错误。
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. 开场 ----------------------------------------------------------------
[bg storage="assets/bg/hana.png"]
[ch name="Narrator" text="教程 15 开始：高级表达式。"]
[p]

; ---- 2. 准备表达式用变量 -----------------------------------------------------
; 用 [set] 初始化 f 表里的变量，[eval] 里再放一个辅助函数 f.calc。
[set f.hp = 70]
[set f.flag = true]
[set f.a = 1]
[set f.b = 2]
[set f.c = 3]
[set f.d = 4]
[set f.e = 5]
[eval exp="f.calc = function(x, y) return (x or 0) + (y or 0) end"]

; ---- 3. 嵌套三元 ------------------------------------------------------------
; f.a ? f.b : (f.c ? f.d : f.e)：a 为真取 b；否则再看 c，取 d 或 e。
[eval exp="f.nest = f.a ? f.b : (f.c ? f.d : f.e)"]
[ch name="Narrator" text="嵌套三元：f.nest = $f.nest（a 真，故取 b=2）"]
[p]
; 把 f.a 置假、f.c 置假再算一次，走到最内层 else（e=5）。
[set f.a = false]
[set f.c = false]
[eval exp="f.nest = f.a ? f.b : (f.c ? f.d : f.e)"]
[ch name="Narrator" text="嵌套三元再算：f.nest = $f.nest（a、c 皆假，走到 e=5）"]
[p]

; ---- 4. 多参函数调用 + 逗号分段（round 84）-----------------------------------
; f.calc(f.flag ? 8 : 2, 5)：参数表顶层逗号把 (三元, 5) 分成两段，
; 三元只作用于自己的实参，尾随 5 不会被吞进 else 分支。
[eval exp="f.two = f.calc(f.flag ? 8 : 2, 5)"]
[ch name="Narrator" text="多参调用：f.two = $f.two（flag 真，8+5=13）"]
[p]
; flag 置假时三元取 2，即 2+5=7。
[set f.flag = false]
[eval exp="f.two = f.calc(f.flag ? 8 : 2, 5)"]
[ch name="Narrator" text="多参调用再算：f.two = $f.two（flag 假，2+5=7）"]
[p]
; 三元在末位实参：f.calc(f.a, f.b, f.flag ? 1 : 2)，前导实参保持独立。
[eval exp="f.three = f.calc(f.calc(f.a, f.b), f.flag ? 1 : 2)"]
[ch name="Narrator" text="嵌套调用：f.three = $f.three（外层再套一层 calc）"]
[p]

; ---- 5. ?? 空合并 -----------------------------------------------------------
; f.missing 未定义，f.g 为 nil：?? 一路回退到 99。
[eval exp="f.fb = f.missing ?? f.g ?? 99"]
[ch name="Narrator" text="?? 空合并：f.fb = $f.fb（缺失一路回退到 99）"]
[p]
; 空字符串 "" 不是 nil，?? 存在时被保留。表达式里的字符串字面量用单引号，
; 避免与外层 exp="..." 的双引号冲突。
[eval exp='f.blank = ""']
[eval exp="f.keep = f.blank ?? 'anon'"]
[ch name="Narrator" text="?? 保留空串：f.keep = [$f.keep]（空串不是 nil 被保留）"]
[p]

; ---- 6. [eval] 三元赋值 ------------------------------------------------------
; RHS 走完整表达式管道：hp>50 ? "high" : "low"。
[eval exp="f.tier = f.hp > 50 ? 'high' : 'low'"]
[ch name="Narrator" text="eval 三元赋值：f.tier = $f.tier（hp=70 > 50，取 high）"]
[p]
; 数值边界：科学计数、十六进制、前导点小数、大整数。
[eval exp="f.big = 1e3 > 0xFF"]
[eval exp="f.small = .5"]
[eval exp="f.large = 9007199254740993"]
[ch name="Narrator" text="数值边界：big=$f.big small=$f.small large=$f.large"]
[p]

; ---- 7. 作用域前缀 -----------------------------------------------------------
; sf./tf./mp./lf. 分别对应四个作用域表，[eval] 里统一可见。
; 先给各作用域表写值，再让 ?? 链按 sf→tf→mp→lf 顺序取值。
[eval exp="sf.x = 10"]
[eval exp="tf.y = 20"]
[eval exp="mp.z = 30"]
[eval exp="lf.w = 40"]
[eval exp="f.scope_ok = sf.x ?? tf.y ?? mp.z ?? lf.w"]
[ch name="Narrator" text="作用域前缀：f.scope_ok = $f.scope_ok（sf 命中得 10）"]
[p]

[ch name="Narrator" text="高级表达式教程完成。"]
[p]
[end]
