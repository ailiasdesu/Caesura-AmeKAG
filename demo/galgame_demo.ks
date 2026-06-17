; =============================================================================
;  Caesura (AmeKAG) — Complete Galgame Demo
;  Tests: bg/fg/ch/text/ruby/audio/transition/vfx/eval/iscript/if/jump/save
;  Uses actual assets from assets/ folder
; =============================================================================

; --- Scene 0: Opening with bgm + bg + title text ---
[font face="default" size=22]
[pt speed=60]

[bg storage="assets/bg/classroom.png"]
[wait time=300]

[playbgm storage="assets/bgm/daily.wav" loop="true"]

[ch name="系统" text="Caesura (AmeKAG) 引擎完整演示"]
[ch name="系统" text="KAG + Lua 混合编程 — 双向互操作"]
[wait time=500]

; Hybrid Lua: initialize game state variables
[eval exp="f.day = 1"]
[eval exp="f.score = 0"]
[eval exp="f.playerName = '访客'"]

; Use iscript for multi-line Lua initialization
[iscript]
    f.flags = f.flags or {}
    f.flags.met_teacher = false
    f.flags.met_student = false
    f.flags.found_notebook = false
    print("[Demo] Game state initialized: day=" .. f.day .. ", score=" .. f.score)
[endiscript]

[ch name="系统" text="游戏状态已初始化——Lua 混合编程就绪。"]
[p]

; --- Scene 1: Classroom — meet the teacher ---
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=500]

[ch name="旁白" text="午后的阳光透过窗户洒进教室，空气中漂浮着细小的尘埃。"]

[playse storage="assets/se/click.wav"]
[ch name="老师" text="欢迎来到我们的课堂。"]
[ch name="老师" text="今天我们要学习一些有趣的内容。"]

; Hybrid [if] branching — read Lua variable
[if exp="f.day == 1"]
    [ch name="老师" text="这是第一天上课，请多关照。"]
[endif]

; Hybrid [eval] to modify state from KAG
[eval exp="f.score = f.score + 10"]
[eval exp="f.flags.met_teacher = true"]

[ch name="老师" text="你的当前积分：[emb exp='f.score']"]
[ch name="老师" text="记住——你的选择会改变故事的走向。"]
[p]

; --- Scene 2: Hallway — meet a student ---
[cl]
[bg storage="assets/bg/hana.png"]
[wait time=400]

[ch name="旁白" text="下课后，你走到走廊上。樱花正盛开着。"]

[fg storage="assets/fg/girl_uniform.png"]
[position layer="fg" pos="right"]

[playvoice storage="assets/voice/line01.wav"]
[ch name="？？？" text="啊，你好！你是新来的同学吗？"]

; Hybrid iscript — complex state manipulation
[iscript]
    f.flags.met_student = true
    f.score = f.score + 20

    -- Check conditions set by script flow
    if f.flags.met_teacher then
        kag["ch"](ctx, {name = "同学", text = "我看到你刚才在教室里和老师说话。"})
        kag["ch"](ctx, {name = "同学", text = "老师教得还不错吧？"})
    else
        kag["ch"](ctx, {name = "同学", text = "你是第一次来这所学校吗？"})
    end

    -- Branch narrative based on score
    if f.score >= 30 then
        kag["ch"](ctx, {name = "同学", text = "你的积分已经达到 " .. f.score .. " 了！真厉害！"})
    end
[endiscript]

[ch name="同学" text="我叫樱，很高兴认识你。"]
[ch name="同学" text="放学后要不要一起去图书馆？"]
[p]

; --- Scene 3: Library — notebook discovery ---
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[ch name="旁白" text="放学后，你和樱一起来到了图书馆。"]

; Hybrid [if] with multi-condition branching
[eval exp="f.flags.found_notebook = true"]
[eval exp="f.score = f.score + 15"]

[if exp="f.flags.found_notebook and f.flags.met_student"]
    [ch name="樱" text="看！这里有一本旧笔记本——"]
    [ch name="樱" text="上面写着一些有趣的笔记。"]
    [ch name="旁白" text="你翻开笔记本，发现里面记录着前人的学习心得。"]
[else]
    [ch name="旁白" text="图书馆很安静，夕阳透过窗户洒进来。"]
[endif]

; Ruby text test
[ruby text="振仮名" ruby="ふりがな"]
[ch name="系统" text="这是日语振假名[emb exp='\"テスト\"']的演示。"]

[p]

; --- Scene 4: Transition + VFX ---
[cl]
[bg storage="assets/bg/hana.png"]

; Use iscript to control scene flow with Lua logic
[iscript]
    print("[Demo] Scene 4: Testing transitions and VFX")

    -- Conditional scene branch
    if f.score >= 40 then
        kag["ch"](ctx, {name = "樱", text = "今天真是太开心了！你的积分达到了 " .. f.score .. "！"})
        kag["ch"](ctx, {name = "樱", text = "明天也要一起学习哦。"})
    else
        kag["ch"](ctx, {name = "樱", text = "时间不早了，我们该回家了。"})
    end
[endiscript]

; Transition effect
[trans method="crossfade" time=1000]
[wait time=200]

; --- Scene 5: Ending with auto-save ---
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[ch name="旁白" text="夜深了。你回到自己的房间，回顾今天的经历。"]

; Hybrid eval for final state
[eval exp="f.day = f.day + 1"]

[ch name="系统" text="第一天结束。"]
[ch name="系统" text="最终积分：[emb exp='f.score']"]

; Use iscript for auto-save + summary
[iscript]
    print("[Demo] Day " .. f.day .. " complete. Final score: " .. f.score)

    -- List all flags set during the game
    local flags_set = {}
    for k, v in pairs(f.flags or {}) do
        if v then table.insert(flags_set, k) end
    end
    print("[Demo] Flags: " .. table.concat(flags_set, ", "))

    -- Auto-save progress
    kag.save_game(ctx, -2, "第一天自动存档")
    print("[Demo] Auto-saved to slot -2")

    kag["ch"](ctx, {name = "系统", text = "进度已自动保存。"})
    kag["ch"](ctx, {name = "系统", text = "触发的剧情标志: " .. table.concat(flags_set, "、")})
[endiscript]

[p]

; --- Final: credits ---
[cl]
[bg storage="assets/bg/hana.png"]
[wait time=300]

[ch name="Caesura (AmeKAG)" text="引擎完整演示 — 结束"]
[ch name="Caesura (AmeKAG)" text="KAG 脚本 + Lua 混合编程"]
[ch name="Caesura (AmeKAG)" text="感谢观看！"]

[wait time=1500]

; Stop BGM
[stopbgm]

[ch name="系统" text="演示结束。按任意键退出。"]
[p]

[end]
