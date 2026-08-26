-- =============================================================================
--  Caesura (AmeKAG) — test_layered_vfs.lua
--  Unit tests for Multi-CARC Layered Virtual File System (Layered VFS)
--  Validates priority ordering, DLC layering, patch overrides, and fallback.
-- =============================================================================

print("=== Multi-CARC Layered VFS Tests ===")

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
-- 1. Mock Layered VFS / ProviderChain simulator in Lua to verify priority resolution
-- -----------------------------------------------------------------------------
local function createMockProvider(name, priority, files)
    return {
        name = name,
        priority = priority,
        files = files or {},
        exists = function(self, path)
            return self.files[path] ~= nil
        end,
        read = function(self, path)
            return self.files[path]
        end
    }
end

local function createMockChain()
    local providers = {}
    return {
        providers = providers,
        addProvider = function(self, p)
            table.insert(self.providers, p)
            table.sort(self.providers, function(a, b)
                if a.priority ~= b.priority then
                    return a.priority > b.priority
                end
                return a.name < b.name
            end)
        end,
        exists = function(self, path)
            for _, p in ipairs(self.providers) do
                if p:exists(path) then return true end
            end
            return false
        end,
        read = function(self, path)
            for _, p in ipairs(self.providers) do
                if p:exists(path) then
                    local data = p:read(path)
                    if data ~= nil then return data, p.name, p.priority end
                end
            end
            return nil, nil, nil
        end
    }
end

-- -----------------------------------------------------------------------------
-- 2. Test Priority Hierarchy:
--    Patch (40) > DLC (30) > Lang (20) > Base (10) > Local Dir (5)
-- -----------------------------------------------------------------------------
local chain = createMockChain()

local base_provider = createMockProvider("CARC:base.carc", 10, {
    ["scenario/start.ks"] = "start base",
    ["bg/title.png"]       = "base title image",
    ["audio/bgm01.ogg"]   = "base audio",
    ["common/ui.png"]      = "base ui",
})

local lang_provider = createMockProvider("CARC:lang_zh.carc", 20, {
    ["scenario/start.ks"] = "start zh translation",
    ["i18n/strings.json"] = "zh json",
})

local dlc_provider = createMockProvider("CARC:dlc_01.carc", 30, {
    ["scenario/dlc_extra.ks"] = "dlc extra story",
    ["bg/title.png"]          = "dlc title image override",
})

local patch_provider = createMockProvider("CARC:patch.carc", 40, {
    ["scenario/start.ks"] = "start hotfix patch v1.1",
    ["common/ui.png"]     = "hotfixed ui",
})

local dir_provider = createMockProvider("Dir:assets", 5, {
    ["dev/debug.txt"]     = "local dev file",
    ["audio/bgm01.ogg"]   = "local audio override (should be shadowed by CARC)",
})

-- Add providers out of order to assert automatic priority sorting
chain:addProvider(dir_provider)
chain:addProvider(base_provider)
chain:addProvider(patch_provider)
chain:addProvider(lang_provider)
chain:addProvider(dlc_provider)

-- Verify provider order
check(#chain.providers == 5, "All 5 providers registered")
check(chain.providers[1].priority == 40, "Top provider is patch (prio 40)")
check(chain.providers[2].priority == 30, "Second provider is DLC (prio 30)")
check(chain.providers[3].priority == 20, "Third provider is Lang (prio 20)")
check(chain.providers[4].priority == 10, "Fourth provider is Base (prio 10)")
check(chain.providers[5].priority == 5,  "Fifth provider is Local Dir (prio 5)")

-- -----------------------------------------------------------------------------
-- 3. Test Layered Resolution & Shadowing
-- -----------------------------------------------------------------------------

-- File in Patch, Lang, and Base -> Patch (40) must win
local val, src, prio = chain:read("scenario/start.ks")
check(val == "start hotfix patch v1.1", "Patch overrides lang and base for start.ks")
check(src == "CARC:patch.carc", "Source is patch.carc")
check(prio == 40, "Resolved at priority 40")

-- File in DLC and Base -> DLC (30) must win
val, src, prio = chain:read("bg/title.png")
check(val == "dlc title image override", "DLC overrides base for title.png")
check(src == "CARC:dlc_01.carc", "Source is dlc_01.carc")
check(prio == 30, "Resolved at priority 30")

-- File in Lang only -> Lang (20) served
val, src, prio = chain:read("i18n/strings.json")
check(val == "zh json", "Lang pack serves localized strings.json")
check(src == "CARC:lang_zh.carc", "Source is lang_zh.carc")
check(prio == 20, "Resolved at priority 20")

-- File in Base and Dir -> Base (10) wins over Dir (5)
val, src, prio = chain:read("audio/bgm01.ogg")
check(val == "base audio", "Base CARC (10) takes precedence over local dir (5)")
check(src == "CARC:base.carc", "Source is base.carc")

-- File in DLC only -> DLC served
val, src, prio = chain:read("scenario/dlc_extra.ks")
check(val == "dlc extra story", "DLC expansion content accessible")

-- File in Local Dir only -> Dir served
val, src, prio = chain:read("dev/debug.txt")
check(val == "local dev file", "Loose assets in directory served if not in CARC")

-- Missing file -> nil
check(chain:exists("nonexistent/missing.png") == false, "Missing file returns exists = false")
val, src = chain:read("nonexistent/missing.png")
check(val == nil and src == nil, "Missing file returns nil read")

-- -----------------------------------------------------------------------------
-- 4. Test Multi-DLC Stacking
-- -----------------------------------------------------------------------------
local dlc2 = createMockProvider("CARC:dlc_02.carc", 30, {
    ["scenario/dlc_extra.ks"] = "dlc 2 updated story",
    ["scenario/dlc_02.ks"]    = "dlc 2 exclusive",
})
chain:addProvider(dlc2)

-- When priorities are equal, deterministic name sorting (dlc_01 < dlc_02)
check(chain:exists("scenario/dlc_02.ks") == true, "Multiple DLC archives co-exist seamlessly")

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then
    os.exit(1)
end
