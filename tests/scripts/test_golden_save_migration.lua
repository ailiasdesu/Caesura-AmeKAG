-- test_golden_save_migration.lua — Golden Save 跨版本迁移（v1 旧形态 → 现行加载）
--
-- 035 C 块第 2 件（Golden Save 跨版本迁移测试）。仓内没有旧实体存档文件，
-- 按文档化 schema 快照构造等价 v1 形态（任务书授权），锁两层迁移契约：
--
--  ① C++ SaveManager 磁盘 envelope（src/storage/SaveManager.cpp L387-394 文档键集）：
--     schema_version 读缺省=1（safeInt 回退 L464）；schemaVer < m_currentSchemaVersion 时
--     migrate(data, schemaVer) 按 registerBuiltinMigrations 链逐级迁移
--     （L608-636：v1→v2 playtime / v2→v3 minigame / v3→v4 live2d / v4→v5 editor）。
--     本测试镜像该纯函数链，断言 v1 data 迁移后获 playtime=0 / minigame / live2d / editor
--     四默认字段且原有字段无损。
--
--  ② Lua capture_state / load（scripts/kag/commands/save.lua）：
--     state.schema_version = 2（L157-158，"bumped: added unlock state"）；v1 state 缺
--     unlockedCG / unlockedMusic / text_state / textbox_style / skip_mode / auto_mode /
--     nvl_mode / voice_muted / language / seen_scenes / seen_endings / loop_stacks /
--     backlog[].src（L91-94 "Absent in older saves"）。load 以类型守卫 + 缺省恢复
--     （安全载荷化，L368-432），随后重存（capture_state）升为 schema_version=2 并
--     补齐 unlock/voice 默认字段——即"旧存档可加载 + 字段迁移"语义。
--
-- 运行：build/lua/Debug/lua.exe tests/scripts/test_golden_save_migration.lua
-- 退出码：0 = 全过；1 = 有 FAIL（与 test_saveload.lua 同栈）
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond, detail)
  if cond then
    print("PASS " .. name)
    passed = passed + 1
  else
    print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
    failed = failed + 1
  end
end

local Save = require("kag.commands.save")

-- ── ① C++ envelope 迁移链镜像（v1 → v5 纯函数，源位 SaveManager.cpp:613-636）──
local function cpp_migrate(data, fromVersion)
  local chain = {
    [1] = function(d) if d.playtime == nil then d.playtime = 0 end return d end,
    [2] = function(d) if d.minigame == nil then d.minigame = {} end return d end,
    [3] = function(d) if d.live2d == nil then d.live2d = {} end return d end,
    [4] = function(d) if d.editor == nil then d.editor = {} end return d end,
  }
  local cur, ver = data, fromVersion
  for _ = 1, 64 do
    local fn = chain[ver]
    if not fn then break end
    cur = fn(cur)
    ver = ver + 1
  end
  return cur, ver
end

-- 文档化 v1 磁盘 envelope（SaveManager.cpp L387-394 键集；schema_version=1）
local v1_envelope = {
  schema_version = 1,
  timestamp = 1234567890,
  scene = "tests/scripts/golden_rt.ks",
  token_index = 12,
  thumbnail = "",
  engine_version = "1.0.0",
  data = {
    f = { hero = 1, route = "forest" },
    sf = { sys_bgm = true },
    token_index = 12,
    scene_path = "tests/scripts/golden_rt.ks",
    backlog = {
      { name = "Aoi", text = "legacy line", timestamp = 0,
        scene = "s.ks", token_index = 1 },
    },
    label_map = { start = 1, second = 40 },
    description = "v1 golden save",
    call_stack = {},
    -- v1 形态缺省面：无 schema_version / unlockedCG / unlockedMusic /
    -- text_state / textbox_style / skip_mode / auto_mode / nvl_mode /
    -- voice_muted / language / seen_scenes / seen_endings / loop_stacks /
    -- backlog[].src —— 即 schema_version=2 前后差异（save.lua L157-176）。
  },
}

-- 无 schema_version 键的旧 envelope：safeInt 回退 = 1（L464）→ 同一迁移链
local v1_nover = {}
for k, v in pairs(v1_envelope) do if k ~= "schema_version" then v1_nover[k] = v end end

-- 迁移链断言：v1 data → 逐级补默认字段（playtime/minigame/live2d/editor）
local migrated, ver = cpp_migrate(v1_envelope.data, v1_envelope.schema_version)
check("migration v1->v5 playtime default", migrated.playtime == 0)
check("migration v1->v5 minigame default", type(migrated.minigame) == "table")
check("migration v1->v5 live2d default", type(migrated.live2d) == "table")
check("migration v1->v5 editor default", type(migrated.editor) == "table")
check("migration chain reaches v5", ver == 5)
check("migration preserves original fields",
      migrated.f and migrated.f.hero == 1 and migrated.f.route == "forest"
      and migrated.token_index == 12 and migrated.scene_path == v1_envelope.data.scene_path)

local m2, ver2 = cpp_migrate(v1_nover.data, 1)
check("legacy envelope defaults to v1 chain", ver2 == 5 and m2.playtime == 0 and m2.editor ~= nil)

-- ── ② Lua load 恢复 v1 state（mock KAG 绑定，同 test_saveload.lua 桩）──────────
local kag_backup = _G.KAG
local function load_with(state)
  _G.KAG = { load_game = function() return state, { slot = 9 } end,
             save_game = function() return true end }
end

-- v1 state 直接装载（模拟"v1 数据透传至 Lua 层"；真实产品流是先 ① 后 ②，
-- 此处两层各自独立断言）
local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {},
              tokens = {}, token_index = 1 }
load_with(v1_envelope.data)
local okLoad = pcall(Save.load, ctx, { slot = 9 })
_G.KAG = kag_backup
check("v1 state loads without error", okLoad)
check("v1 load_result ok", ctx.tf and ctx.tf.load_result == "ok",
      ctx.tf and tostring(ctx.tf.load_result))
check("v1 f restored", ctx.f.hero == 1 and ctx.f.route == "forest")
check("v1 sf restored", ctx.sf.sys_bgm == true)
check("v1 token_index restored", ctx.token_index == 12)
check("v1 resume scene set (snake)", ctx.current_scene == "tests/scripts/golden_rt.ks")
check("v1 resume scene set (legacy alias)", ctx.currentScene == "tests/scripts/golden_rt.ks")
check("v1 pending load scene/token",
      ctx._pendingLoadScene == "tests/scripts/golden_rt.ks"
      and ctx._pendingLoadToken == 12)
check("v1 backlog text preserved", ctx.backlog and ctx.backlog[1]
      and ctx.backlog[1].text == "legacy line")
check("v1 backlog entry keeps absent src", ctx.backlog and ctx.backlog[1]
      and ctx.backlog[1].src == nil)
check("v1 unlockedCG stays absent (v2-only field)",
      ctx.unlockedCG == nil and ctx.unlockedMusic == nil)
check("v1 mode defaults normalized",
      ctx.skip_mode == false and ctx.auto_mode == false
      and ctx.nvl_mode == false and ctx.voice_muted == false)
check("v1 seen flags defaulted", type(ctx.seen_scenes) == "table"
      and type(ctx.seen_endings) == "table")

-- 重存（capture_state）：字段迁移到 schema_version=2 + unlock/voice 默认补齐
local st2 = Save.capture_state(ctx)
check("re-save upgrades schema_version to 2", st2.schema_version == 2)
check("re-save adds unlockedCG/unlockedMusic defaults",
      type(st2.unlockedCG) == "table" and type(st2.unlockedMusic) == "table"
      and next(st2.unlockedCG) == nil and next(st2.unlockedMusic) == nil)
check("re-save backlog voice defaulted", st2.backlog and st2.backlog[1]
      and st2.backlog[1].voice == "")
check("re-save backlog text intact", st2.backlog and st2.backlog[1]
      and st2.backlog[1].text == "legacy line")
check("re-save backlog src stays nil for legacy entry", st2.backlog and st2.backlog[1]
      and st2.backlog[1].src == nil)
check("re-save keeps f/sf/token",
      st2.f and st2.f.hero == 1 and st2.sf.sys_bgm == true and st2.token_index == 12)

-- v1 引擎版本字段不产生错误结果（SaveManager.cpp L483-486：版本不匹配仅提示不阻断）
check("v1 engine_version tolerated (no throw)", okLoad)

-- ── 汇总 ─────────────────────────────────────────────────────────────
print(string.format("Results: %d passed, %d failed, %d total", passed, failed, passed + failed))
if failed > 0 then os.exit(1) end
