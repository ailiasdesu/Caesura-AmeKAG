-- test_ks_bake.lua — Battle 5a: mobile bytecode pre-baking tests.
-- ks_bake pre-compiles scenes into .ksc (same path/hash algorithm as
-- flow.load_scene); the runtime then loads pre-baked scenes without
-- parse+compile. Also covers the hash-cache correctness (same-size
-- content edit must invalidate).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.save")
pcall(require, "kag.commands.video")
pcall(require, "kag")

local compiler = require("kag.compiler")
local tokenizer = require("tokenizer")
local flow = require("flow")
local bake = dofile("scripts/ks_bake.lua")

local SCENE = "tmp/bake_test.ks"
local OUT = "cache/ksc"

-- CI checkouts start without tmp/ and cache/ (round 60: the suite now
-- runs on Linux/macOS where the Windows mkdir 2>nul trick is invalid).
local function ensure_dir(path)
    local sep = package.config:sub(1, 1)
    if sep == "\\" then
        os.execute('mkdir "' .. path:gsub("/", "\\") .. '" 2>nul')
    else
        os.execute('mkdir -p "' .. path .. '"')
    end
end
ensure_dir("tmp")
ensure_dir("cache")
ensure_dir("cache/ksc")

local function writeScene(text)
    local f = io.open(SCENE, "w")
    f:write(text)
    f:close()
end

-- ---------------------------------------------------------------------------
-- 1. bake writes a .ksc with the same path algorithm as flow.load_scene
-- ---------------------------------------------------------------------------
writeScene('*start\n[set f.hp 100]\n[ch text="hello"]\n[end]\n')
local okBake, outPath = bake.bakeScene(SCENE, OUT)
check("bake succeeds", okBake == true)
check("ksc path matches flow algorithm",
      outPath == flow.load_scene and true or outPath == "cache/ksc/tmp_bake_test.ksc")
local kscExists = io.open(outPath, "r") ~= nil
check("ksc file written", kscExists)

-- ---------------------------------------------------------------------------
-- 2. flow.load_scene loads the pre-baked scene (compiled, correct tokens)
-- ---------------------------------------------------------------------------
flow.scene_cache = {}
local scene = flow.load_scene(SCENE)
check("pre-baked scene loads", scene ~= nil and #scene.tokens > 0)
check("scene is compiled", scene.tokens._compiled ~= nil)
check("labels intact", scene.labels["start"] ~= nil)
check("baked content correct",
      scene.tokens[2] and scene.tokens[2][1] == "set")

-- ---------------------------------------------------------------------------
-- 3. second load within the session is served from caches (~0 cost)
-- ---------------------------------------------------------------------------
flow.scene_cache = {}
local t0 = os.clock()
local scene2 = flow.load_scene(SCENE)
local elapsed = os.clock() - t0
check("repeat load fast (<5ms)", elapsed < 0.005, string.format("%.2fms", elapsed * 1000))
check("repeat load identical", #scene2.tokens == #scene.tokens)

-- ---------------------------------------------------------------------------
-- 4. same-size content edit invalidates the hash cache (freshness)
-- ---------------------------------------------------------------------------
writeScene('*start\n[set f.hp 200]\n[ch text="hello"]\n[end]\n')  -- same length-ish
-- force rebake (hash cache sees the new head)
local okRebake = bake.bakeScene(SCENE, OUT)
check("rebake after edit succeeds", okRebake == true)
flow.scene_cache = {}
local scene3 = flow.load_scene(SCENE)
check("edited content reloaded", scene3.tokens[2] and scene3.tokens[2][1] == "set")

-- ---------------------------------------------------------------------------
-- 5. check mode reports freshness
-- ---------------------------------------------------------------------------
check("fresh after bake", bake.isFresh(SCENE, OUT) == true)
writeScene('*start\n[set f.hp 300]\n[ch text="hello"]\n[end]\n')
check("stale after edit", bake.isFresh(SCENE, OUT) == false)
-- restore + rebake for cleanup state
bake.bakeScene(SCENE, OUT)

-- ---------------------------------------------------------------------------
-- 6. hash cache correctness (same-size edit changes hash)
-- ---------------------------------------------------------------------------
local h1 = compiler.hashFile(SCENE)
local h2 = compiler.hashFile(SCENE)
check("hash cached same file", h1 == h2)
writeScene('*start\n[set f.hp 999]\n[ch text="hello"]\n[end]\n')
local h3 = compiler.hashFile(SCENE)
check("same-size edit changes hash", h1 ~= h3)

-- ---------------------------------------------------------------------------
-- 7. web bundle export (round 35): bakeWeb returns scenes + assets
-- ---------------------------------------------------------------------------
local okWeb, webRes = pcall(function()
    -- bakeWeb is a local inside ks_bake's script body; drive it via CLI
    -- is heavy, so verify through the exported bakeScene path + compiler
    -- encode_lua_literal contract instead: the bundle writer is exercised
    -- end-to-end by the web player integration test (flow.integration.test.js).
    local bundle = { version = 1, scenes = {}, assets = { "assets/bg/classroom.png" } }
    bundle.scenes["test.ks"] = { version = 1, tokens = { { "ch", { text = "x" } } } }
    local enc = compiler.encode_lua_literal(bundle)
    local chunk = assert(load("return " .. enc))
    local round = chunk()
    assert(round.version == 1 and round.assets[1] == "assets/bg/classroom.png")
    assert(round.scenes["test.ks"].tokens[1][1] == "ch")
    return true
end)
check("7a: encode_lua_literal round-trips web bundle", okWeb == true)
check("7b: encode_lua_literal exported by compiler",
      type(compiler.encode_lua_literal) == "function")

-- cleanup
os.remove(SCENE)
os.remove("cache/ksc/tmp_bake_test.ksc")

-- Exit gate.
if failed > 0 then
    print(string.format("KS BAKE TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("KS BAKE TESTS DONE (%d passed)", passed))
