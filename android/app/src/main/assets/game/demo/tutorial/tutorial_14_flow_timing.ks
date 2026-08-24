; =============================================================================
; Caesura KAG 教程 14 —— 计时与流程控制（round 80/83/84/87/88）
;
; 综合运用 round 80-88 新增/加固的流程与计时能力：
;   - [wait ms=...] / [delay ms=...]        基于帧让步的时间等待（round 87/88
;                                             与 stop_flag/_next_index 对齐：
;                                             场景中止或 Lua 跳转会立即结束等待）
;   - [goto] 与 [jump] 混合                  同场景标签跳转（goto 是 jump 的
;                                             KAG3 别名，round 83/84 加固循环栈
;                                             重置与反向往回边守卫）
;   - [jump target=...] 跳出 [for] 循环        任意跳转离开循环体时清空循环栈
;   - [i18n language=...]                     运行时热切换 UI 语言（round 76）
;   - 复数 i18n 字典值                        CLDR 风格复数变体表 {one=,other=}
;                                             （round 80/86），{items} 文本展开
;                                             取通用（other）形态，绝不泄漏表句柄
;
; 说明：同前 13 个教程，本场景通过 ks_check 静态契约校验（零警告）+ ks_bake
;   编译 + Web 播放器一路运行到 [end] 零错误。
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. 开场 ----------------------------------------------------------------
[bg storage="assets/bg/classroom.png"]
[ch name="Narrator" text="教程 14 开始：一起驾驭计时与流程控制。"]
[p]

; ---- 2. [wait] 与 [delay] 帧让步等待 ----------------------------------------
; [wait ms=100] 每帧 yield，让渲染继续走，直到累计 100ms。
; 当场景被 [stop]/[jump] 中止（ctx.stop_flag / _next_index）时（round 87/88
; 修复），等待会立即结束而非继续空转。
[wait ms=100]
[delay ms=100]
[ch name="Narrator" text="[wait ms=100] 与 [delay ms=100] 各让步了 100ms，没有卡住场景推进。"]
[p]

; ---- 3. [goto] 跳过中间文本（goto = jump 的同场景别名）-----------------------
[ch name="Narrator" text="下面演示 [goto]：跳到 *skip_target，中间的台词会被跳过。"]
[p]
[goto *skip_target]
[ch name="Narrator" text="这行会被 [goto] 跳过，永远不会显示。"]
[p]
*skip_target
[ch name="Narrator" text="已 [goto] 到 *skip_target 标签，中间的文本被跳过了。"]
[p]

; ---- 4. [jump] 跳出 [for] 循环（round 83 循环栈重置）-------------------------
; [for] 本应跑 i=1,2,3,4 共 4 次；当 i==2 时 [jump target=*early_exit] 提前
; 退出循环体。round 83 起，任意同场景跳转都会清空 _forStack/_whileStack 等
; 循环栈，陈旧条目不会污染后续同名的 [for]。
[set f.count = 0]
[for var="i" start="1" end="4"]
[add name="f.count" value=1]
[if exp="f.i == 2"]
[jump target="*early_exit"]
[endif]
[endfor]
*early_exit
[ch name="Narrator" text="在 i=2 时 [jump] 出了 [for] 循环，f.count 停在 $f.count，只累加了 2 次。"]
[p]

; ---- 5. 反向往回边守卫（round 84，宏/标签回绕保护）--------------------------
; 一个 [jump] 向后跳到更早的标签是「反向往回边」。round 84 的 _backJumps
; 守卫只允许同一边首次触发（防死循环回绕），下面这个向后 jump 是合法且只跑一次。
; （[jump] 的下一条内容会被跳过，这里紧跟标签以保持可达）
[jump target="*meridian"]
*meridian
[ch name="Narrator" text="到达 *meridian 标签，向后 [jump] 正常只执行一次。"]
[p]

; ---- 6. [i18n] 运行时热切换语言 + 复数字典值 ---------------------------------
; [i18n language="en"] 热切换 UI 语言（round 76/78 relocalize 整页重译）。
; {settings} 走 {key} 展开；{items} 是 assets/lang/en.lua 里的复数变体表
; （CLDR 风格），无 n 参数时安全落到通用（other）形态，绝不泄漏表句柄。
[i18n language="en"]
[ch name="Narrator" text="Now English. settings={settings} plural={items}."]
[p]

; 切回中文再验证一次 {key} 字典展开仍工作。
[i18n language="zh"]
[ch name="Narrator" text="回到中文。settings={settings} plural={items}。"]
[p]

; 注：真正按数量选复数形态要调用运行时 API
;   i18n.translate("items", { n = 1 }) -> "1 item"   （en one）
;   i18n.translate("items", { n = 3 }) -> "3 items"  （en other）
; 剧本文本里没有 n 参数，所以这里展示的是通用（other）形式。

; ---- 7. [stop] 与 [end] -----------------------------------------------------
; [stop] 与 [end] 一样立即终止场景（KAG3 的 [stop] 拼写）。round 87/88 让
; 一个正在 [wait]/[delay] 的协程在 stop_flag 置位时立刻退出。
; 为保持场景一定能跑到 [end]，这里用 [end] 收尾。
[ch name="Narrator" text="计时与流程教程完成。" ]
[p]
[end]
