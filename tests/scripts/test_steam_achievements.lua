-- =============================================================================
--  Caesura (AmeKAG) — test_steam_achievements.lua
--  Unit tests for Steamworks achievements command and Cloud Save bridge.
-- =============================================================================

local Schema = require("kag.schema")
local system = require("kag.commands.system")

print("=== Steamworks Achievements & Cloud Bridge Tests ===")

local passed, failed = 0, 0
local function check(cond, msg)
    if cond then
        passed = passed + 1
        print("  [PASS] " .. msg)
    else
        failed = failed + 1
        print("  [FAIL] " .. msg)
    end
end

-- -----------------------------------------------------------------------------
-- 1. Schema Validation for [steam_achievement]
-- -----------------------------------------------------------------------------
local specs = Schema.specs("steam_achievement")
check(specs ~= nil, "steam_achievement command schema is registered")
check(specs.id ~= nil, "steam_achievement schema has 'id' property")
check(specs.name ~= nil, "steam_achievement schema has 'name' property")

local contracts = Schema.dumpContracts()
local meta = contracts["steam_achievement"] and contracts["steam_achievement"]._meta
check(meta ~= nil and meta.category == "system", "steam_achievement category is system")
check(meta ~= nil and meta.blocking == false, "steam_achievement is non-blocking")

-- Coerce valid parameters
local coerced, err = Schema.coerce("steam_achievement", { id = "ACH_PROLOGUE_CLEAR" })
check(err == nil or #err == 0, "Valid achievement id coerces cleanly")
check(coerced.id == "ACH_PROLOGUE_CLEAR", "Achievement id preserved")

-- -----------------------------------------------------------------------------
-- 2. Mock Steamworks Backend in Lua
-- -----------------------------------------------------------------------------
local unlocked_achievements = {}
local stats_int = {}
local stats_float = {}
local cloud_files = {}

local mock_steam = {
    unlock_achievement = function(id)
        unlocked_achievements[id] = true
        return true
    end,
    set_achievement = function(id)
        unlocked_achievements[id] = true
        return true
    end,
    is_achievement_unlocked = function(id)
        return unlocked_achievements[id] == true
    end,
    reset_achievement = function(id)
        unlocked_achievements[id] = nil
        return true
    end,
    reset_all_achievements = function()
        unlocked_achievements = {}
        return true
    end,
    set_stat_int = function(name, val)
        stats_int[name] = val
        return true
    end,
    get_stat_int = function(name)
        return stats_int[name] or 0
    end,
    set_stat_float = function(name, val)
        stats_float[name] = val
        return true
    end,
    get_stat_float = function(name)
        return stats_float[name] or 0.0
    end,
    store_stats = function()
        return true
    end,
    is_overlay_active = function()
        return false
    end,
    cloud_write = function(file, data)
        cloud_files[file] = data
        return true
    end,
    cloud_read = function(file)
        return cloud_files[file]
    end,
    cloud_file_size = function(file)
        local d = cloud_files[file]
        return d and #d or 0
    end,
    cloud_file_exists = function(file)
        return cloud_files[file] ~= nil
    end,
    cloud_delete = function(file)
        cloud_files[file] = nil
        return true
    end,
    cloud_list = function()
        local list = {}
        for k, _ in pairs(cloud_files) do
            table.insert(list, k)
        end
        return list
    end,
    cloud_quota_total = function()
        return 100 * 1024 * 1024
    end,
    cloud_quota_used = function()
        local used = 0
        for _, v in pairs(cloud_files) do
            used = used + #v
        end
        return used
    end
}

-- Inject mock steam into global environment / package cache
rawset(_G, "steam", mock_steam)
package.loaded["steam"] = mock_steam

-- -----------------------------------------------------------------------------
-- 3. Test [steam_achievement] Execution
-- -----------------------------------------------------------------------------
local ctx = { f = {}, sf = {}, tf = {} }

-- Test by id parameter
system.steam_achievement(ctx, { id = "ACH_CHAPTER_1" })
check(mock_steam.is_achievement_unlocked("ACH_CHAPTER_1") == true, "[steam_achievement id=...] unlocked achievement")

-- Test by name alias parameter
system.steam_achievement(ctx, { name = "ACH_CHAPTER_2" })
check(mock_steam.is_achievement_unlocked("ACH_CHAPTER_2") == true, "[steam_achievement name=...] unlocked achievement")

-- Test by bare positional argument
system.steam_achievement(ctx, { "ACH_TRUE_END" })
check(mock_steam.is_achievement_unlocked("ACH_TRUE_END") == true, "[steam_achievement ACH_TRUE_END] positional unlocked")

-- Reset achievement
mock_steam.reset_achievement("ACH_CHAPTER_1")
check(mock_steam.is_achievement_unlocked("ACH_CHAPTER_1") == false, "Achievement reset successfully")

mock_steam.reset_all_achievements()
check(mock_steam.is_achievement_unlocked("ACH_CHAPTER_2") == false, "All achievements reset successfully")

-- -----------------------------------------------------------------------------
-- 4. Test Steam Stats Surface
-- -----------------------------------------------------------------------------
mock_steam.set_stat_int("playtime_minutes", 120)
check(mock_steam.get_stat_int("playtime_minutes") == 120, "Int stat set and get verified")

mock_steam.set_stat_float("affinity_score", 98.5)
check(mock_steam.get_stat_float("affinity_score") == 98.5, "Float stat set and get verified")
check(mock_steam.store_stats() == true, "store_stats returns true")

-- -----------------------------------------------------------------------------
-- 5. Test Steam Cloud Storage Bridge
-- -----------------------------------------------------------------------------
local sample_save = '{"scene":"chapter_3.ks","token":42,"f":{"score":100}}'
mock_steam.cloud_write("save_01.json", sample_save)
check(mock_steam.cloud_file_exists("save_01.json") == true, "Cloud file written and exists")
check(mock_steam.cloud_file_size("save_01.json") == #sample_save, "Cloud file size matches")
check(mock_steam.cloud_read("save_01.json") == sample_save, "Cloud file content readback matches")

local files = mock_steam.cloud_list()
check(#files == 1 and files[1] == "save_01.json", "Cloud file enumeration returns save_01.json")

mock_steam.cloud_delete("save_01.json")
check(mock_steam.cloud_file_exists("save_01.json") == false, "Cloud file deleted")

-- -----------------------------------------------------------------------------
-- 6. Safe Null Fallback (No Steam SDK) — and it must LEAVE A TRACE
-- -----------------------------------------------------------------------------
rawset(_G, "steam", nil)
package.loaded["steam"] = nil
-- Command must complete without throwing when Steam is not available, but the
-- outcome must be OBSERVABLE: a silently-dropped achievement is a shipping bug.
local ok = pcall(function()
    system.steam_achievement(ctx, { id = "ACH_FALLBACK" })
end)
check(ok == true, "steam_achievement degrades safely when steam global is absent")
check(ctx.tf.steam_achievement_result == "unavailable",
    "absent Steam records result=unavailable (degradation is observable, not silent)")

-- -----------------------------------------------------------------------------
-- 7. Behavior: a refused unlock is distinguishable from a successful one
-- -----------------------------------------------------------------------------
-- A backend that declines the id (Null backend / achievement not declared in
-- the Steam app) must NOT be reported as unlocked.
local refusing = {
    set_achievement = function() return false end,
    is_achievement_unlocked = function() return false end,
}
rawset(_G, "steam", refusing)
ctx.tf.steam_achievement_result = nil
check(system.steam_achievement(ctx, { id = "ACH_UNDECLARED" }) == true,
    "refused achievement still returns true (a story never stalls on Steam)")
check(ctx.tf.steam_achievement_result == "refused",
    "refused achievement records result=refused, not unlocked")

-- A backend whose call raises must not take the scene down.
rawset(_G, "steam", { set_achievement = function() error("steam exploded") end })
ctx.tf.steam_achievement_result = nil
local okRaise = pcall(function() system.steam_achievement(ctx, { id = "ACH_RAISE" }) end)
check(okRaise == true, "a throwing Steam backend is contained, scene continues")
check(ctx.tf.steam_achievement_result == "refused",
    "a throwing backend is recorded as refused, never as unlocked")

-- A binding table without either unlock function is 'unavailable'.
rawset(_G, "steam", { is_achievement_unlocked = function() return false end })
ctx.tf.steam_achievement_result = nil
system.steam_achievement(ctx, { id = "ACH_NO_FN" })
check(ctx.tf.steam_achievement_result == "unavailable",
    "steam table without set/unlock_achievement records result=unavailable")

-- -----------------------------------------------------------------------------
-- 8. Behavior: cond gates the unlock; missing id is refused loudly
-- -----------------------------------------------------------------------------
rawset(_G, "steam", mock_steam)
package.loaded["steam"] = mock_steam
mock_steam.reset_all_achievements()

ctx.tf.steam_achievement_result = nil
ctx.f.chapter_cleared = false
system.steam_achievement(ctx, { id = "ACH_COND_FALSE", cond = "f.chapter_cleared" })
check(mock_steam.is_achievement_unlocked("ACH_COND_FALSE") == false,
    "cond=false does not unlock the achievement")
check(ctx.tf.steam_achievement_result == nil,
    "a cond-gated skip records no result at all")

ctx.f.chapter_cleared = true
system.steam_achievement(ctx, { id = "ACH_COND_TRUE", cond = "f.chapter_cleared" })
check(mock_steam.is_achievement_unlocked("ACH_COND_TRUE") == true,
    "cond=true unlocks the achievement")
check(ctx.tf.steam_achievement_result == "unlocked",
    "successful unlock records result=unlocked")

-- No id at all: must not throw and must not touch Steam.
ctx.tf.steam_achievement_result = nil
local okNoId = pcall(function() system.steam_achievement(ctx, {}) end)
check(okNoId == true, "missing id does not throw")
check(ctx.tf.steam_achievement_result == nil, "missing id records no unlock")

-- silent=true suppresses the warning but keeps the recorded outcome.
rawset(_G, "steam", refusing)
package.loaded["steam"] = nil
ctx.tf.steam_achievement_result = nil
system.steam_achievement(ctx, { id = "ACH_SILENT", silent = true })
check(ctx.tf.steam_achievement_result == "refused",
    "silent=true still records the refusal (only the log line is suppressed)")

-- Schema carries the two new fields so the editor / LSP can validate them.
check(specs.cond ~= nil, "steam_achievement schema has 'cond' property")
check(specs.silent ~= nil, "steam_achievement schema has 'silent' property")

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then
    os.exit(1)
end
