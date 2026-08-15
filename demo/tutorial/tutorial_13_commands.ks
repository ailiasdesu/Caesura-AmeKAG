; =============================================================================
; Caesura KAG 教程 13 —— KAG3 兼容命令实战（round 71）
;
; 综合运用 round 71 新增的 KAG3 兼容命令：
;   - [textspeed cps=...] / [cps n]        打字速度切换（字符/秒）
;   - [add]/[sub]/[mul]/[div]/[mod]/[dec]  变量算术链（f.n += v 等）
;   - [csp]/[csl]/[csd]                    角色立绘：显示 / 移动 / 清除
;   - [notify msg=... time=...]            角落吐司通知（作者反馈）
;   - [palette effect=day|night|toggle]    LUT 色调滤镜（依赖后端绑定）
;   - [vibrate time=...]                    消息层震动（[vib] 别名）
;   - [preload type=texture path=...]      资源预加载
;
; 本教程覆盖命令：
;   [font] [pt] [bg] [textspeed] [cps] [set] [eval]
;   [add] [sub] [mul] [div] [mod] [dec] [ch] [p]
;   [csp] [csl] [csd] [notify] [palette] [vibrate] [preload] [end]
;
; 说明：本教程同时满足 ks_check 静态契约校验；涉及后端绑定的命令（如
;   [palette] 的 LUT 通道、角色立绘缺图时）都以注释说明并安全跳过，
;   保证剧本可一路运行到 [end] 零错误。
; =============================================================================

[font face="default" size=22]
[pt speed=60]

; ---- 1. 场景背景（与教程 03 一致）-----------------------------------------
[bg storage="assets/bg/classroom.png"]
[ch name="Narrator" text="教程 13 开始：一起体验 round 71 的 KAG3 兼容命令。"]
[p]

; ---- 2. 打字速度切换 ------------------------------------------------------
; [textspeed cps=40]：KAG3 兼容命令，按"字符/秒"设置逐字显示速度。
; 它改写 ctx.text_speed，下一行 [ch] 逐字显示时由 kag_runner 每帧读取。
; 单位换算：cps -> 每字符毫秒 = floor(1000 / cps)；有效范围 1..120。
[textspeed cps=40]
[ch name="Narrator" text="现在打字速度是每秒 40 字（每字 25ms）。"]
[p]

; [cps n] 是 [textspeed] 的别名，两者都可接受裸参数形式 [cps 60]。
[cps 60]
[ch name="Narrator" text="切换到每秒 60 字，文字刷得明显更快。"]
[p]

; 想恢复 [pt] 的"每字符毫秒"控制，直接再用 [pt speed=...] 即可覆盖。
[pt speed=60]
[ch name="Narrator" text="（又用 [pt speed=60] 切回每字 60ms。）"]
[p]

; ---- 3. 算术链：先初始化 --------------------------------------------------
; 先用 [set] / [eval] 初始化变量 f.score 与 f.n。
[set f.score = 10]
[set f.n = 8]

; ---- 4. [add]  f.n += value -------------------------------------------------
[add name="f.n" value=5]
[ch name="Narrator" text="[add] 之后：f.n = ${f.n}（8 + 5）"]
[p]

; ---- 5. [sub]  f.n -= value -------------------------------------------------
[sub name="f.n" value=3]
[ch name="Narrator" text="[sub] 之后：f.n = ${f.n}（13 - 3）"]
[p]

; ---- 6. [mul]  f.n *= value -------------------------------------------------
[mul name="f.n" value=2]
[ch name="Narrator" text="[mul] 之后：f.n = ${f.n}（10 × 2）"]
[p]

; ---- 7. [div]  f.n /= value -------------------------------------------------
[div name="f.n" value=4]
[ch name="Narrator" text="[div] 之后：f.n = ${f.n}（20 ÷ 4）"]
[p]

; ---- 8. [mod]  f.n %= value -------------------------------------------------
[mod name="f.n" value=3]
[ch name="Narrator" text="[mod] 之后：f.n = ${f.n}（5 % 3）"]
[p]

; ---- 9. [dec]  f.n -= amount（默认 1）---------------------------------------
[dec name="f.n"]
[ch name="Narrator" text="[dec] 之后：f.n = ${f.n}（2 - 1，默认减 1）"]
[p]

; ---- 10. 算术链小结 + [eval] 校验最终值 -------------------------------------
; $f.score 在整条链里未被动过，仍是 10。[eval] 可做表达式赋值。
[ch name="Narrator" text="算术链收尾：f.n=$f.n，f.score=$f.score。"]
[p]
[eval exp="f.n = f.n * 3 + 1"]
[ch name="Narrator" text="[eval] 之后：f.n = ${f.n}（1 × 3 + 1）"]
[p]

; ---- 11. 角色立绘命令 ------------------------------------------------------
; 仓库 assets/ 下没有 assets/char/ 目录，但可用 assets/fg/girl_uniform.png
; 作为立绘图。[csp] 默认找 assets/char/<name>.png，可用 storage= 覆盖路径。
[csp name="girl" layer="chara" x=320 y=240 storage="assets/fg/girl_uniform.png"]
[ch name="Narrator" text="用 [csp] 显示了角色立绘（storage 指向已有前景图）。"]
[p]

; ---- 12. [csl] 移动角色层 ---------------------------------------------------
; [csl] 把已显示的角色层移到新坐标，不改可见性。
[csl name="girl" layer="chara" x=600 y=240]
[ch name="Narrator" text="[csl] 把角色层移到了右侧（x=600）。"]
[p]

; 注：若运行环境加载不出立绘（headless 纹理缺失），[csp]/[csl] handler
; 各自带守护（load 失败直接 return；层未建则打印诊断），不会中断剧本。

; ---- 13. [csd] 清除角色层 ---------------------------------------------------
[csd name="girl" layer="chara"]
[ch name="Narrator" text="[csd] 清除了角色层，立绘消失。"]
[p]

; ---- 14. [notify] 角落吐司通知 ----------------------------------------------
; [notify msg=... time=...]：作者面向上屏通知；time 单位是毫秒。
; 无后端/无 toast 模块时 handler 降级为打印一行，绝不阻塞或中断剧本。
[notify msg="这是一条角落通知" time=1000]
[ch name="Narrator" text="触发了 [notify]，右上角应短暂浮出提示。"]
[p]

; ---- 15. [palette] 色调滤镜 ------------------------------------------------
; [palette effect=...] 走 scripts/palette.lua（LUT 色调分级）。
;   effect=day     中性 / 清除滤镜
;   effect=night   蓝色调（加载 assets/lut/night.png，缺失则回退清除）
;   effect=toggle  day <-> night 切换
;
; round 72 起 palette.lua 内置守卫：backend.set_palette / destroy_texture
; 未注册时打印可见提示并无操作（不再崩溃）；绑定就绪后自动生效。
[palette effect=day]
[palette effect=night]
[palette effect=toggle]
[ch name="Narrator" text="[palette] 色调滤镜：后端未注册 LUT 绑定时安全降级（见上方提示）。"]
[p]

; ---- 16. [vibrate] 消息层震动 ----------------------------------------------
; [vibrate time=300] 是 [vib] 的别名：抖动对话消息窗口，强调某句台词。
; 依赖 message 层已存在（前面的 [ch] 已创建），blocking=true，等待动画完成。
[ch name="Narrator" text="注意这句台词 —— 马上会有震动强调。"]
[p]
[vibrate time=300]
[ch name="Narrator" text="刚才 [vibrate] 让消息层抖了 300ms。"]
[p]

; ---- 17. [preload] 资源预加载 ----------------------------------------------
; [preload type="texture" path="..."] 提前把资源缓存进内存，供后续使用。
; type 可选 texture/audio/scene（默认 texture），wait 可同步/异步（默认 true）。
; 注意：显式写 type=/wait= 目前会触发 kag/schema.coerce 对 choices（数组形）
; 的已知校验缺陷（已知 round-71 gotcha，full_story.ks 同样命中）；因此这里
; 省略 type 使用默认 texture、省略 wait 使用默认同步 true，语义不变。
[preload path="assets/bg/hana.png"]
[ch name="Narrator" text="已 [preload] 预加载 assets/bg/hana.png（下一个背景）。"]
[p]
[bg storage="assets/bg/hana.png"]
[ch name="Narrator" text="因为预加载过，切到花田背景几乎无卡顿。"]
[p]

; ---- 18. 收尾 --------------------------------------------------------------
[bg storage="assets/bg/classroom.png"]
[ch name="Narrator" text="KAG3 兼容命令教程完成！"]
[p]
[end]