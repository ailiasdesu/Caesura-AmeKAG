-- test_sma_check.lua — SMA asset validator tests (Battle 4d S3, round 19):
-- good-asset acceptance, a 20+ case bad-asset matrix (each violation must
-- be located by field path), validate_file smoke, and the sma.load/validate
-- integration. The checker is pure Lua (no engine binding needed).
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local sma_check = require("kag.sma_check")

-- ---------------------------------------------------------------------------
-- 1. Good asset: full design-doc shape (dual-bone weights, parts with two
-- variants, two clips, texture/atlas metadata) — must validate clean.
-- ---------------------------------------------------------------------------
local good = {
    texture = "chara/hero.png",
    atlas = { w = 512, h = 512 },
    bones = {
        { id = 0, parent = -1, pivot = { 0.5, 0.9 } },
        { id = 1, parent = 0, pivot = { 0.5, 0.5 } },
        { id = 2, parent = 1, pivot = { 0.2, 0.5 } },
    },
    mesh = {
        positions = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } },
        uvs = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } },
        indices = { 0, 1, 2, 0, 2, 3 },
        weights = {
            { bone = 0, w = 0.6 }, { bone = 1, w = 0.4 },
            { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 },
            { bone = 0, w = 0.5 }, { bone = 1, w = 0.5 },
            { bone = 1, w = 1.0 }, { bone = 0, w = 0.0 },
        },
    },
    animations = {
        idle = { duration = 2.0, tracks = {
            { bone = 1, frames = { { t = 0, rot = 0 }, { t = 1, rot = 0.1, scale = 1.0, offset = { 0, 0 } } } },
        } },
        wave = { duration = 1.0, tracks = {
            { bone = 2, frames = { { t = 0, rot = 0 }, { t = 0.5, rot = 0.5 }, { t = 1, rot = 0 } } },
        } },
    },
    parts = {
        { id = "body", current = "default", tex = 1, variants = {
            default = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, uvs = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 } },
            armored = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, uvs = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 } },
        } },
    },
}
local res = sma_check.validate(good)
check("good: validates clean", res.ok == true and #res.errors == 0)

-- Legacy single-bone layout (1 entry per vertex) also validates clean.
local single = {
    bones = { { id = 0, parent = -1, pivot = { 0.5, 0.5 } } },
    mesh = {
        positions = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
        uvs = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
        indices = { 0, 1, 2 },
        weights = { { bone = 0, w = 1.0 }, { bone = 0, w = 1.0 }, { bone = 0, w = 1.0 } },
    },
    animations = { idle = { tracks = { { bone = 0, frames = { { t = 0 } } } } } },
}
check("good: legacy single-bone layout", sma_check.validate(single).ok == true)

-- Omitted weights/uvs/duration are tolerated (runtime defaults exist).
local minimal = {
    bones = { { id = 0, parent = -1, pivot = { 0.5, 0.5 } } },
    mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 } },
    animations = { idle = { tracks = { { bone = 0, frames = { { t = 0 } } } } } },
}
check("good: minimal asset (no weights/uvs/duration)", sma_check.validate(minimal).ok == true)

-- Multi-part asset without a top-level mesh is valid (spawn consumes parts).
local partsOnly = {
    bones = { { id = 0, parent = -1, pivot = { 0.5, 0.5 } } },
    parts = { { id = "p", variants = { a = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 } } } } },
    animations = { idle = { tracks = { { bone = 0, frames = { { t = 0 } } } } } },
}
check("good: parts-only asset (no mesh)", sma_check.validate(partsOnly).ok == true)
local noMeshNoParts = {
    bones = { { id = 0, parent = -1, pivot = { 0.5, 0.5 } } },
    animations = { idle = { tracks = { { bone = 0, frames = { { t = 0 } } } } } },
}
check("bad: neither mesh nor parts", sma_check.validate(noMeshNoParts).ok == false)

-- ---------------------------------------------------------------------------
-- 2. Bad asset matrix: every class of violation must be located.
-- ---------------------------------------------------------------------------
local function bad(name, asset, pattern)
    local r = sma_check.validate(asset)
    local found = false
    if not r.ok then
        for _, e in ipairs(r.errors) do
            if e:find(pattern, 1, true) then found = true break end
        end
    end
    check(name, found)
end

local B = {
    bones = { { id = 0, parent = -1, pivot = { 0.5, 0.5 } } },
    mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 } },
    animations = { idle = { tracks = { { bone = 0, frames = { { t = 0 } } } } } },
}

bad("bad: root not an object", "nope", "root")
bad("bad: bones missing", { mesh = B.mesh, animations = B.animations }, "bones")
bad("bad: bones empty", { bones = {}, mesh = B.mesh, animations = B.animations }, "bones")
bad("bad: bone id missing", { bones = { { parent = -1, pivot = { 0, 0 } } }, mesh = B.mesh, animations = B.animations }, ".id")
bad("bad: duplicate bone id",
    { bones = { { id = 0, parent = -1, pivot = { 0, 0 } }, { id = 0, parent = -1, pivot = { 0, 0 } } }, mesh = B.mesh, animations = B.animations },
    "duplicate bone id")
bad("bad: dangling parent",
    { bones = { { id = 0, parent = 7, pivot = { 0, 0 } } }, mesh = B.mesh, animations = B.animations },
    "undefined bone 7")
bad("bad: parent cycle",
    { bones = {
        { id = 0, parent = 1, pivot = { 0, 0 } },
        { id = 1, parent = 0, pivot = { 0, 0 } },
    }, mesh = B.mesh, animations = B.animations },
    "cycle")
bad("bad: pivot malformed",
    { bones = { { id = 0, parent = -1, pivot = { 0.5 } } }, mesh = B.mesh, animations = B.animations },
    "pivot")
bad("bad: positions empty",
    { bones = B.bones, mesh = { positions = {}, indices = { 0 } }, animations = B.animations },
    "positions")
bad("bad: position malformed",
    { bones = B.bones, mesh = { positions = { { 0, 0 }, { 1 } }, indices = { 0, 1, 0 } }, animations = B.animations },
    "positions[2]")
bad("bad: uvs length mismatch",
    { bones = B.bones, mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, uvs = { { 0, 0 } }, indices = { 0, 1, 2 } }, animations = B.animations },
    "uvs")
bad("bad: indices not multiple of 3",
    { bones = B.bones, mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1 } }, animations = B.animations },
    "multiple of 3")
bad("bad: index out of range",
    { bones = B.bones, mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 9 } }, animations = B.animations },
    "out of range")
bad("bad: weight layout length",
    { bones = B.bones,
      mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 },
               weights = { { bone = 0, w = 1.0 }, { bone = 0, w = 1.0 } } },
      animations = B.animations },
    "layout")
bad("bad: weight bone undefined",
    { bones = B.bones,
      mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 },
               weights = { { bone = 3, w = 1.0 }, { bone = 0, w = 0.5 }, { bone = 0, w = 0.5 } } },
      animations = B.animations },
    "defined bone")
bad("bad: same bone twice on one vertex",
    { bones = B.bones,
      mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 },
               weights = { { bone = 0, w = 0.5 }, { bone = 0, w = 0.5 }, { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 }, { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 } } },
      animations = B.animations },
    "twice")
bad("bad: weight sum not 1",
    { bones = B.bones,
      mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 },
               weights = { { bone = 0, w = 0.7 }, { bone = 1, w = 0.4 }, { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 }, { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 } } },
      animations = B.animations },
    "weight sum")
bad("bad: weight w out of range",
    { bones = B.bones,
      mesh = { positions = { { 0, 0 }, { 1, 0 }, { 0, 1 } }, indices = { 0, 1, 2 },
               weights = { { bone = 0, w = 1.5 }, { bone = 1, w = 0.0 }, { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 }, { bone = 0, w = 1.0 }, { bone = 1, w = 0.0 } } },
      animations = B.animations },
    "[0,1]")
bad("bad: duration not positive",
    { bones = B.bones, mesh = B.mesh, animations = { idle = { duration = 0, tracks = { { bone = 0, frames = { { t = 0 } } } } } } },
    "duration")
bad("bad: track bone undefined",
    { bones = B.bones, mesh = B.mesh, animations = { idle = { duration = 1, tracks = { { bone = 5, frames = { { t = 0 } } } } } } },
    "tracks[1].bone")
bad("bad: empty frames",
    { bones = B.bones, mesh = B.mesh, animations = { idle = { duration = 1, tracks = { { bone = 0, frames = {} } } } } },
    "frames")
bad("bad: descending frame time",
    { bones = B.bones, mesh = B.mesh, animations = { idle = { duration = 1, tracks = { { bone = 0, frames = { { t = 1 }, { t = 0 } } } } } } },
    "ascending")
bad("bad: missing frame timestamp",
    { bones = B.bones, mesh = B.mesh, animations = { idle = { duration = 1, tracks = { { bone = 0, frames = { { rot = 0.1 } } } } } } },
    "timestamp")
bad("bad: clip with no frames and no duration",
    { bones = B.bones, mesh = B.mesh, animations = { idle = { tracks = {} } } },
    "no keyframes")
bad("bad: part variant mesh broken",
    { bones = B.bones, mesh = B.mesh, animations = B.animations,
      parts = { { id = "p", variants = { a = { positions = { 1 } } } } } },
    "variants.a")
bad("bad: duplicate part id",
    { bones = B.bones, mesh = B.mesh, animations = B.animations,
      parts = { { id = "p", variants = { a = B.mesh } }, { id = "p", variants = { a = B.mesh } } } },
    "duplicate part")
bad("bad: texture not a string", { texture = 42, bones = B.bones, mesh = B.mesh, animations = B.animations }, "texture")

-- ---------------------------------------------------------------------------
-- 3. validate_file smoke (temp file in the CWD) + meta summary.
-- ---------------------------------------------------------------------------
local tmp = "tmp_sma_check_test.json"
local f = io.open(tmp, "w")
f:write('{"texture":"t.png","bones":[{"id":0,"parent":-1,"pivot":[0.5,0.5]}],'
        .. '"mesh":{"positions":[[0,0],[1,0],[0,1]],"indices":[0,1,2]},'
        .. '"animations":{"idle":{"duration":1.0,"tracks":[{"bone":0,"frames":[{"t":0}]}]}}}')
f:close()
local fr = sma_check.validate_file(tmp)
check("file: good asset ok", fr.ok == true)
check("file: meta summary", fr.meta.bones == 1 and fr.meta.verts == 3
      and fr.meta.tris == 1 and fr.meta.parts == 0 and #fr.meta.anims == 1)
os.remove(tmp)
local mf = sma_check.validate_file("definitely_missing.json")
check("file: missing file reports error", mf.ok == false and mf.errors[1]:find("cannot open", 1, true) ~= nil)

-- ---------------------------------------------------------------------------
-- 4. kag.sma integration: sma.load(validate=true) gate + sma.validate.
-- ---------------------------------------------------------------------------
local sma = require("kag.sma")
local goodJson = '{"bones":[{"id":0,"parent":-1,"pivot":[0.5,0.5]}],'
    .. '"mesh":{"positions":[[0,0],[1,0],[0,1]],"indices":[0,1,2]},'
    .. '"animations":{"idle":{"tracks":[{"bone":0,"frames":[{"t":0}]}]}}}'
local badJson = '{"bones":[{"id":0,"parent":-1,"pivot":[0.5,0.5]}],'
    .. '"mesh":{"positions":[[0,0]],"indices":[0,1,2]}}'
local okLoad = pcall(function() sma.load(goodJson, { validate = true }) end)
check("sma.load: valid asset passes gate", okLoad == true)
local failLoad = pcall(function() sma.load(badJson, { validate = true }) end)
check("sma.load: invalid asset raises", failLoad == false)
local vr = sma.validate(goodJson)
check("sma.validate: ok on valid", vr.ok == true)
local vr2 = sma.validate(badJson)
check("sma.validate: error located", vr2.ok == false and vr2.errors[1]:find("indices", 1, true) ~= nil)

print(("SUMMARY sma_check: %d passed, %d failed"):format(passed, failed))
if failed > 0 then os.exit(1) end
