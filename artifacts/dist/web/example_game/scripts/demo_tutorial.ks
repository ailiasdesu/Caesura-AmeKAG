; ===========================================================================
;  Caesura (AmeKAG) — demo_tutorial.ks
;  语言教程配套场景：逐能力演示 KAG Neo-Genesis 特性。
;  对应文档：docs/guides/kag-language-tour.md
;  验证：tests/scripts/test_tutorial_scene.lua（确定性执行 + 状态断言）
; ===========================================================================

*start

; --- 2. 对话与文本 ---
[ch name="Narrator" text="KAG Neo-Genesis 教程演示"]
[pt speed=30]
[ch name="Hero" text="对话：带说话人"]
[text text="旁白：无说话人"]
[l]
[ch name="Hero" text="换行后继续"]

; --- 5. 流程控制: if/else ---
[set f.hp 80]
[if exp="f.hp > 50"]
[ch text="HP 充足 (f.hp=${f.hp})"]
[else]
[ch text="HP 不足"]
[endif]

; --- 5. 流程控制: while 循环 ---
[set f.i 0]
[while exp="f.i < 3"]
[inc f.i 1]
[emb exp="'循环第 ' .. f.i .. ' 次'"]
[endwhile]

; --- 6. 变量与表达式 ---
[set f.gold 100]
[inc f.gold 50]
[ch text="金币：${f.gold}（插值）"]
[random f.dice 1 6]
[ch text="骰子：${f.dice}"]

; --- 8. 参数化宏 ---
[macro shout args="who,msg"]
[ch name="%who%" text="%msg%！"]
[endmacro]

[shout who="Hero" msg="参数化宏"]
[erasemacro shout]

; --- 7. 选择分支 ---
[select]
[sel text="走左边" target=*left]
[sel text="走右边" target=*right]
[endselect]

*left
[ch text="左路线"]
[jump target=*merge]

*right
[ch text="右路线"]

*merge

; --- 9. Lua 混合 ---
[iscript]
  f.lua_result = 6 * 7
[/endscript]
[eval exp="f.lua_result = f.lua_result + 1"]
[ch text="Lua 计算：${f.lua_result}"]

; --- 5. 流程控制: for 循环 + break ---
[for var="k" start="1" end="10" step="1"]
[if exp="f.k >= 3"]
[break]
[endif]
[endfor]
[ch text="for 循环 break 后 k=${f.k}"]

; --- 10. 存档/回滚/历史 ---
[save 1]
[history]

; --- 5. 跳转与子程序 ---
[call target=*sub]
[ch text="回到主流程"]
[jump target=*ending]

*sub
[ch text="子程序（call/return）"]
[return]

*ending
[ch text="教程演示结束"]
[end]
