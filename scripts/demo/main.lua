-- ============================================================================
-- Caesura (AmeKAG) Demo — 完整引擎能力演示
-- ============================================================================
-- 场景 1: 古典教室 — KAG 文本 + 立绘 + BGM + 转场
-- 场景 2: Live2D 动态角色 — 呼吸 + 表情切换 (需 CAESURA_HAS_LIVE2D)
-- 场景 3: MiniGame 3D 迷宫 — 立方体 + 碰撞 + 点光源
-- 场景 4: 视频过场 + 存档/读档演示
-- ============================================================================

local backend = require("backend")
local kag = require("kag.kag")
local minigame = require("kag.minigame")
local save = require("save")

-- ------------------------------------------------------------------
-- 场景 1: 古典教室
-- ------------------------------------------------------------------
local function scene1_classroom()
    kag.show_image("bg", "classroom_day", 0, 0)
    kag.play_bgm("gentle_piano", 0.6, true)
    kag.transition("fade_in", 1.5)

    kag.show_text(nil, "午后的阳光洒进教室，空气中飘浮着细小的尘埃。")
    kag.wait_click()

    kag.show_image("fg", "desk_front", 320, 400, { opacity = 0.9 })
    kag.show_text(nil, "桌上摊开的笔记本，写满了未完成的诗句。")
    kag.wait_click()

    kag.show_image("char", "heroine_uniform", 800, 200, { opacity = 1.0 })
    kag.show_text("她", "你来了。今天的风，很适合写故事呢。")
    kag.wait_click()

    kag.show_text(nil, "她微微侧过头，目光穿过窗户，落在远处摇曳的樱树上。")
    kag.wait_click()

    kag.transition("fade_out", 1.0)
    kag.clear_screen()
    kag.stop_bgm(1.0)
end

-- ------------------------------------------------------------------
-- 场景 2: Live2D 动态角色演示
-- ------------------------------------------------------------------
local function scene2_live2d()
#if CAESURA_HAS_LIVE2D then
    kag.show_text(nil, "【Live2D 模式】")
    kag.wait_click()

    kag.show_image("bg", "stage_night", 0, 0)
    kag.transition("fade_in", 1.0)

    -- 加载 Live2D 模型
    kag.live2d_load("heroine", "assets/live2d/haruka/haruka.model3.json")
    kag.live2d_set_animation("heroine", "idle_01", true)
    kag.show_text(nil, "Live2D 角色载入中... 动态立绘实时渲染。")
    kag.wait_click()

    -- 表情切换
    kag.live2d_set_expression("heroine", "smile")
    kag.show_text("遥", "こんにちは！今日はいい天気ですね。")
    kag.wait_click()

    kag.live2d_set_expression("heroine", "surprise")
    kag.show_text("遥", "えっ？もうこんな時間？")
    kag.wait_click()

    kag.live2d_remove("heroine")
    kag.clear_screen()
#else
    -- Fallback: Static sprite替代
    kag.show_image("bg", "stage_night", 0, 0)
    kag.transition("fade_in", 1.0)
    kag.show_image("char", "heroine_uniform", 800, 200)
    kag.show_text(nil, "【注: Live2D 未编译 — 使用静态立绘代替】")
    kag.wait_click()
    kag.show_text("遥", "こんにちは！今日はいい天気ですね。")
    kag.wait_click()
    kag.clear_screen()
#endif
end

-- ------------------------------------------------------------------
-- 场景 3: MiniGame 3D 迷宫探索
-- ------------------------------------------------------------------
local function scene3_minigame()
    kag.show_text(nil, "【MiniGame 3D 模式】")
    kag.wait_click()

    -- 初始化 3D 场景
    minigame.init({ width = 1280, height = 720 })
    minigame.set_camera({ pos = { 0, 5, 15 }, lookAt = { 0, 2, 0 }, fov = 60 })

    -- 设置材质
    minigame.set_material({ roughness = 0.4, metallic = 0.1, specStr = 0.6 })

    -- 设置光照: 1 方向光 + 2 点光源
    minigame.set_lighting({
        dir = { 0.5, -1.0, 0.3, 0.8 },
        points = {
            { pos = { 5, 3, 0 }, color = { 1, 0.8, 0.4 }, intensity = 2.0, range = 20 },
            { pos = { -5, 2, 3 }, color = { 0.3, 0.6, 1.0 }, intensity = 1.5, range = 15 },
        }
    })

    -- 生成迷宫几何体
    minigame.spawn_floor({ size = { 20, 0.5, 20 }, pos = { 0, -1, 0 }, color = { 0.2, 0.25, 0.3 } })

    -- 墙壁
    local walls = {
        { size = { 0.5, 3, 10 }, pos = { -5, 0.5, 0 } },
        { size = { 0.5, 3, 10 }, pos = { 5, 0.5, 0 } },
        { size = { 10, 3, 0.5 }, pos = { 0, 0.5, -5 } },
        { size = { 10, 3, 0.5 }, pos = { 0, 0.5, 5 } },
    }
    for _, w in ipairs(walls) do
        minigame.spawn_cube({ size = w.size, pos = w.pos, color = { 0.5, 0.4, 0.3 } })
    end

    -- 彩色目标立方体
    minigame.spawn_cube({ size = { 1, 1, 1 }, pos = { 3, 0.5, 3 }, color = { 1, 0.8, 0.2 } })
    minigame.spawn_cube({ size = { 1, 1, 1 }, pos = { -3, 0.5, -3 }, color = { 0.2, 0.8, 1 } })

    kag.show_text(nil, "使用方向键在 3D 迷宫移动，收集彩色立方体！")
    kag.wait_click()

    -- 模拟移动（实际游戏由输入系统驱动）
    for i = 1, 5 do
        kag.show_text(nil, string.format("移动中... 步数 %d/5", i))
        kag.wait_click()
    end

    -- 碰撞检测
    if minigame.check_collision({ 3, 0.5, 3 }, 1.5) then
        kag.show_text(nil, "获得金色立方体！✨")
        kag.wait_click()
    end

    minigame.cleanup()
    kag.clear_screen()
end

-- ------------------------------------------------------------------
-- 场景 4: 视频过场 + 存档演示
-- ------------------------------------------------------------------
local function scene4_video_save()
    kag.show_text(nil, "【视频过场 + 存档系统演示】")
    kag.wait_click()

    -- 自动存档
    save.auto_save()
    kag.show_text(nil, "自动存档完成。当前可安全退出。")
    kag.wait_click()

    -- 视频播放
    kag.show_text(nil, "正在播放开场动画...")
    kag.play_video("assets/video/opening.mp4", { loop = false, volume = 0.8 })
    kag.wait_video_end()

    -- 快速存档
    save.quick_save(1)
    kag.show_text(nil, "已保存到槽位 1 (F5 快速存档)。按 F6 可快速读档。")
    kag.wait_click()

    -- 显示存档信息
    local info = save.get_save_info(1)
    if info then
        kag.show_text(nil, string.format(
            "槽位 1: %s | 场景: %s | 时间: %s",
            info.timestamp or "未知",
            info.scene or "demo",
            info.playtime or "0:00"
        ))
    end
    kag.wait_click()

    kag.show_text(nil, "演示结束。感谢体验 Caesura (AmeKAG)！")
    kag.wait_click()

    save.auto_save()
    kag.clear_screen()
end

-- ==================================================================
-- Main Demo Entry
-- ==================================================================
local function run_demo()
    kag.show_text(nil, "=== Caesura (AmeKAG) 引擎演示 ===")
    kag.wait_click()

    scene1_classroom()
    scene2_live2d()
    scene3_minigame()
    scene4_video_save()

    kag.show_text(nil, "全部场景演示完成。退出中...")
    kag.wait_click()
end

return { run = run_demo }
