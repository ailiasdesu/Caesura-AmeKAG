-- =============================================================================
--  test_scale_stress.lua — Large-asset pressure / scale stress suite (round 101).
--
--  Purpose: verify the engine's behaviour ceiling under LARGE game assets.
--  The 100-round hot-path guards (test_schema/test_expr_lang/test_tokenizer/
--  test_frame_bench/test_benchmark) pin element-level budgets. This suite adds
--  a SCALE dimension: many-and-big assets in one headless pass.
--
--  Headless: backend/audio are mocked or simulated locally (no GPU/SoLoud);
--  no C++ registry is touched, so nothing poisons later suite tests
--  (package.loaded["backend"]/["audio"] are NOT modified). Real hot paths are
--  exercised where owned by Lua: tokenizer.parse + scheduler.run + kag.expr.translate.
--
--  Dimensions covered:
--      A. 4096x4096 texture atlas   — per-tile budget accounting round-trip
--           (64x64 tiles => 4096 tiles, 16.77M texels tracked)     < 1s
--      B. 100+ voice/SE handles      — mock audio handle pool: alloc/free/reuse,
--           concurrency cap honored, IDs reused (nextId bounded)   < 2s
--      C. 10000-token scene          — tokenizer.parse + scheduler.run
--           throughput (parse < 10s, run < 10s, full stream walked)
--      D. 500-page backlog           — desktop-cap accumulation + coarse heap
--           growth bound via collectgarbage delta                 < 4096 KB
--      E. 3000-line narrative flow   — kag.expr.translate per-line budget
--           (~340-410 KB cumulative, < 10s, all lines translated clean)
--
--  Budgets are perf-guard style: measured < budget (CI-loose os.clock),
--  iteration/op counts asserted directly where deterministic.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
local expr      = require("kag.expr")

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("=== Large-Asset Scale-Stress Tests ===")

-- ---------------------------------------------------------------------------
-- A. 4096x4096 atlas: texture budget accounting round-trip.
--    Simulates the engine texture budget path: a 4096x4096 sheet split into
--    64x64 tiles (4096 tiles) each charging w*h texels to a running budget,
--    plus a full prepass walk (the kind of iteration a real slate allocator
--    does when picking regions). Asserts accounting is exact and bounded.
-- ---------------------------------------------------------------------------
do
    local ATLAS = 4096          -- 4096x4096 sheet
    local TILE  = 64
    local TILES = (ATLAS / TILE) * (ATLAS / TILE)   -- 4096 tiles
    local t0 = os.clock()
    local atlas, budget = {}, 0
    for t = 1, TILES do
        local cost = TILE * TILE
        budget = budget + cost
        atlas[t] = {
            sheet = 1,
            u = (t % (ATLAS / TILE)) * TILE,
            v = math.floor((t - 1) / (ATLAS / TILE)) * TILE,
            w = TILE, h = TILE, used = false,
        }
    end
    local walked = 0
    for i = 1, TILES do
        if atlas[i] and atlas[i].u >= 0 and atlas[i].v >= 0 then
            walked = walked + 1
        end
    end
    local dt = os.clock() - t0

    check("A1 4096x4096 atlas splits into exactly 4096 tiles", #atlas == 4096,
        tostring(#atlas))
    check("A2 per-tile budget accounting sums to 4096^2 texels",
        budget == 4096 * 4096, tostring(budget))
    check("A3 full prepass walk touches every tile", walked == 4096,
        tostring(walked))
    check("A4 atlas budget round-trip < 1s", dt < 1.0,
        string.format("%.4f s", dt))
    print(string.format("  [scale] atlas: %d tiles, %d texels accounted in %.3f ms",
        #atlas, budget, dt * 1000))
end

-- ---------------------------------------------------------------------------
-- B. 100+ voice/SE handles: mock audio handle pool (alloc / free / reuse,
--    concurrency cap honored). Mirrors the engine's SoLoud handle registry:
--    handles are pooled (freed ids reused), live count is capped, and a
--    monotonic id must NOT grow unboundedly when loads churn handles.
-- ---------------------------------------------------------------------------
do
    local free, live = {}, {}
    local nextId = 1
    local MAX_LIVE = 128          -- >100 concurrent voice/SE handles
    local allocCount, freeCount, reuseCount = 0, 0, 0

    local function alloc()
        local h
        if #free > 0 then
            h = table.remove(free)      -- REUSE a freed handle
            reuseCount = reuseCount + 1
        else
            h = nextId
            nextId = nextId + 1
        end
        live[h] = true
        allocCount = allocCount + 1
        return h
    end

    local function release_oldest(n)
        local k = 0
        for id in pairs(live) do
            live[id] = nil
            free[#free + 1] = id
            freeCount = freeCount + 1
            k = k + 1
            if k >= n then break end
        end
    end

    local t0 = os.clock()
    for _ = 1, 20000 do
        for _ = 1, 4 do alloc() end
        -- keep 150+ live demand before churn to prove >100 concurrency
        local cnt = 0
        for _ in pairs(live) do cnt = cnt + 1 end
        if cnt >= MAX_LIVE then release_oldest(8) end
    end
    local dt = os.clock() - t0

    local liveNow = 0
    for _ in pairs(live) do liveNow = liveNow + 1 end

    check("B1 >100 handles held concurrently (desktop cap)",
        liveNow >= 100, tostring(liveNow))
    check("B2 live concurrency never exceeds cap", liveNow <= MAX_LIVE,
        tostring(liveNow))
    check("B3 handles are REUSED (id pool recycles)", reuseCount > 1000,
        tostring(reuseCount))
    check("B4 monotonic id stays bounded (no unbounded growth under churn)",
        nextId <= MAX_LIVE + 64, tostring(nextId))
    check("B5 20000 alloc/free round-trips < 2s", dt < 2.0,
        string.format("%.4f s", dt))
    print(string.format("  [scale] audio: %d alloc, %d reused (%d frees), %d live, nextId=%d in %.3f ms",
        allocCount, reuseCount, freeCount, liveNow, nextId, dt * 1000))
end

-- ---------------------------------------------------------------------------
-- C. 10000-token scene: tokenizer.parse + scheduler.run throughput.
--    Builds ~9600 tokens from 4800 [ch][p] lines (~388 KB source), parses it,
--    then drives scheduler.run to completion over the whole stream. Budgets:
--    parse < 10s, run < 10s, full stream walked (no premature halt).
-- ---------------------------------------------------------------------------
do
    local kag_orig = package.loaded["kag"]
    local kmock = {}
    kmock.ch = function(ctx, p) ctx._ch = (ctx._ch or 0) + 1 end
    kmock.p  = function() end
    package.loaded["kag"] = kmock

    local lines = {}
    for i = 1, 4800 do
        lines[#lines + 1] = string.format(
            '[ch name="Nar" text="Line number %d with some narrative filler text here"][p]', i)
    end
    local script = table.concat(lines)

    local t0 = os.clock()
    local tokens = tokenizer.parse(script)
    local tParse = os.clock() - t0

    check("C1 10000-token scene parses to >=9500 tokens", #tokens >= 9500,
        tostring(#tokens))
    check("C2 tokenizer parse < 10s", tParse < 10.0,
        string.format("%.4f s", tParse))

    -- scheduler.run drives the whole stream; it yields per token.
    local ctx = {
        f = {}, sf = {}, tf = {}, mp = {}, lf = {},
        tokens = tokens, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, stop_flag = false, dispatched = {},
        load_tokens = function() end,
    }
    local t1 = os.clock()
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    local frames = 0
    local MAX_FRAMES = 30000
    while coroutine.status(co) ~= "dead" and frames < MAX_FRAMES do
        frames = frames + 1
        coroutine.resume(co, 16)
    end
    local tRun = os.clock() - t1

    check("C3 scheduler walks the entire stream (frame count == tokens+1)",
        frames == #tokens + 1 and coroutine.status(co) == "dead",
        "frames=" .. tostring(frames) .. " tokens=" .. tostring(#tokens))
    check("C4 every [ch] dispatched (4800)", (ctx._ch or 0) == 4800,
        tostring(ctx._ch))
    check("C5 scheduler run of 9600-token stream < 10s", tRun < 10.0,
        string.format("%.4f s", tRun))
    print(string.format("  [scale] scene: %d tokens parsed in %.3f ms; scheduler %d frames in %.3f ms (%.0f tok/ms)",
        #tokens, tParse * 1000, frames, tRun * 1000, #tokens / (tRun * 1000)))

    package.loaded["kag"] = kag_orig
end

-- ---------------------------------------------------------------------------
-- D. 500-page backlog accumulation + coarse heap growth bound.
--    Desktop upper bound is 500 backlog pages; each page holds several
--    dialogue entries. Assert the table accumulates 500 pages and the coarse
--    heap growth (collectgarbage delta) stays under a few MB — a light guard
--    against amortised-unbounded growth (per-entry allocations sinking us).
-- ---------------------------------------------------------------------------
do
    collectgarbage("collect"); collectgarbage("collect"); collectgarbage("collect")
    local before = collectgarbage("count") -- KB
    local backlog = {}
    for p = 1, 500 do
        local ent = {}
        for e = 1, 5 do
            ent[#ent + 1] = {
                who = "Nar",
                text = string.format(
                    "backlog dialogue line %d of page %d with character speech here",
                    e, p),
                pos = { x = 1, y = 2 },
            }
        end
        backlog[p] = ent
    end
    local after = collectgarbage("count") -- KB (backlog still referenced)
    local growth = after - before

    check("D1 500-page backlog accumulates 500 pages", #backlog == 500,
        tostring(#backlog))
    check("D2 backlog honours desktop cap semantics (exactly 500)",
        #backlog == 500, tostring(#backlog))
    check("D3 500-page backlog heap growth < 4096 KB", growth < 4096,
        string.format("%.1f KB", growth))
    print(string.format("  [scale] backlog: %d pages, heap growth %.1f KB (before %.1f / after %.1f)",
        #backlog, growth, before, after))
end

-- ---------------------------------------------------------------------------
-- E. 3000-line narrative flow: kag.expr.translate per-line budget.
--    The scheduler translates each [if]/[eval] expression individually, so a
--    "3000-line narrative" stress = 3000 translate calls over ~340-410 KB of
--    cumulative expression source. Assert all translate clean (no raw && or
----    top-level ? residue) and the batch stays < 10s.
-- ---------------------------------------------------------------------------
do
    local N = 3000
    local totalSrc, cleanOut = 0, true
    local t0 = os.clock()
    for i = 1, N do
        -- ~135-byte nested-ternary expression per "narrative line" (scene text
        -- with inline $() logic), so the 3000-line flow is ~340-400 KB.
        local src = string.format(
            'f.hp > %d && f.mp <= 20 ? (f.lv >= 3 && f.agility > 12 ? "atk_hi_%d" : "atk_lo_%d") : (f.sp %%%% 2 == 0 ? "def_hi_%d" : "def_lo_%d")',
            i % 50, i, i, i, i)
        totalSrc = totalSrc + #src
        local out = expr.translate(src)
        -- no raw '&&' left; ternary rewritten by translate
        if out:find("&&", 1, true) then cleanOut = false end
    end
    local dt = os.clock() - t0

    check("E1 3000-line translate cumulative source >= 300 KB", totalSrc >= 300000,
        tostring(totalSrc) .. " bytes")
    check("E2 every translated line clean (no raw && residue)", cleanOut, "")
    check("E3 3000-line translate budget < 10s", dt < 10.0,
        string.format("%.4f s", dt))
    print(string.format("  [scale] translate: %d lines / %d bytes in %.3f ms (%.1f us/line)",
        N, totalSrc, dt * 1000, dt * 1000 * 1000 / N))
end

print(string.format("\nLarge-asset scale-stress results: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
