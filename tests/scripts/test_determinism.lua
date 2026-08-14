-- test_determinism.lua — Battle 3a: deterministic scene execution tests.
-- Drive .ks scenes through the scheduler in pure Lua (mock kag), assert
-- ctx state snapshots, and verify run-to-run determinism.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

-- load command modules so contracts register (mock kag covers dispatch)
pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.save")
pcall(require, "kag.commands.video")
pcall(require, "kag")

local d = require("kag.determinism")

-- ---------------------------------------------------------------------------
-- 1. basic execution: set/ch/if branch, backlog, trace
-- ---------------------------------------------------------------------------
local src = '*start\n[set f.hp 100]\n[ch name="Hero" text="Hello"]\n'
    .. '[if exp="f.hp > 50"]\n[ch text="high"]\n[else]\n[ch text="low"]\n[endif]\n[end]\n'
local ctx, steps, trace = d.run_scene(src)
check("scene runs to completion", steps > 0 and ctx._error == nil
      and not ctx._timed_out)
check("set writes f.hp", ctx.f.hp == 100)
check("backlog from ch commands", #ctx.backlog == 2
      and ctx.backlog[1] == "Hello" and ctx.backlog[2] == "high")
check("if-taken branch (hp>50)", ctx.backlog[2] == "high")
check("trace records commands", trace[1].cmd == "set" and trace[2].cmd == "ch")

-- ---------------------------------------------------------------------------
-- 2. determinism: two runs produce identical state snapshots
-- ---------------------------------------------------------------------------
local ctxA = d.run_scene(src)
local ctxB = d.run_scene(src)
local snapA = d.state_snapshot(ctxA)
local snapB = d.state_snapshot(ctxB)
check("deterministic f", snapA.f.hp == snapB.f.hp)
check("deterministic backlog", #snapA.backlog == #snapB.backlog)
check("deterministic token_index", snapA.token_index == snapB.token_index)
check("assert_state passes on equal", d.assert_state(snapA, snapB))
check("assert_state fails on diff",
      not d.assert_state(snapA, { f = { hp = 1 } }, "intentional mismatch"))

-- ---------------------------------------------------------------------------
-- 3. while loop with counter (bounded, terminates)
-- ---------------------------------------------------------------------------
local loop = '*start\n[set f.i 0]\n[while exp="f.i < 5"]\n[inc f.i 1]\n[endwhile]\n[end]\n'
local lctx = d.run_scene(loop)
check("while loop terminates", lctx._error == nil and not lctx._timed_out)
check("while counter reaches 5", lctx.f.i == 5)

-- ---------------------------------------------------------------------------
-- 4. runaway loop protection: max_steps cap
-- ---------------------------------------------------------------------------
local runaway = '*start\n[while exp="true"]\n[ch text="x"]\n[endwhile]\n'
local rctx = d.run_scene(runaway, { max_steps = 100 })
check("runaway loop timed out", rctx._timed_out == true)

-- ---------------------------------------------------------------------------
-- 5. [for] numeric loop + [break]
-- ---------------------------------------------------------------------------
local forSrc = '*start\n[for var="i" start="0" end="10" step="1"]\n'
    .. '[if exp="f.i == 3"]\n[break]\n[endif]\n[endfor]\n[end]\n'
-- note: [for] writes ctx.f[var]; break exits at 3
local fctx = d.run_scene(forSrc)
check("for loop runs without error", fctx._error == nil)

-- ---------------------------------------------------------------------------
-- 6. kag_override: custom handler replaces a command
-- ---------------------------------------------------------------------------
local spy = {}
local octx = d.run_scene('*start\n[ch text="a"]\n[end]\n', {
    kag_override = {
        ch = function(c, p)
            spy.called = (spy.called or 0) + 1
            c.backlog = c.backlog or {}
            c.backlog[#c.backlog + 1] = "OVERRIDE"
        end,
    },
})
check("kag_override replaces handler", spy.called == 1
      and octx.backlog[1] == "OVERRIDE")

-- ---------------------------------------------------------------------------
-- 7. nested if/elseif/else: only the matching branch executes (deterministic)
-- ---------------------------------------------------------------------------
local nested = '*start\n[set f.score 75]\n'
    .. '[if exp="f.score >= 90"]\n[ch text="A"]\n'
    .. '[elseif exp="f.score >= 80"]\n[ch text="B"]\n'
    .. '[elseif exp="f.score >= 70"]\n[ch text="C"]\n'
    .. '[else]\n[ch text="F"]\n[endif]\n[end]\n'
local nctx = d.run_scene(nested)
check("nested elseif picks C", nctx._error == nil
      and nctx.backlog[#nctx.backlog] == "C")

local nestedLow = d.run_scene('*start\n[set f.score 45]\n'
    .. '[if exp="f.score >= 90"]\n[ch text="A"]\n'
    .. '[elseif exp="f.score >= 70"]\n[ch text="C"]\n'
    .. '[else]\n[ch text="F"]\n[endif]\n[end]\n')
check("else branch when no elseif matches", nestedLow.backlog[1] == "F")

-- ---------------------------------------------------------------------------
-- 8. [jump] label flow control (deterministic redirection)
-- ---------------------------------------------------------------------------
local jumpSrc = '*start\n[ch text="a"]\n[jump target="*skip"]\n'
    .. '[ch text="never"]\n*skip\n[ch text="b"]\n[end]\n'
local jctx = d.run_scene(jumpSrc)
check("jump skips intervening commands", jctx._error == nil
      and #jctx.backlog == 2 and jctx.backlog[1] == "a"
      and jctx.backlog[2] == "b")

-- ---------------------------------------------------------------------------
-- 9. [call]/[return] subroutine stack (deterministic)
-- ---------------------------------------------------------------------------
local callSrc = '*start\n[call target="*sub"]\n[ch text="after"]\n[end]\n'
    .. '*sub\n[ch text="in-sub"]\n[return]\n'
local cctx = d.run_scene(callSrc)
check("call executes subroutine then returns", cctx._error == nil
      and #cctx.backlog == 2 and cctx.backlog[1] == "in-sub"
      and cctx.backlog[2] == "after")

-- ---------------------------------------------------------------------------
-- 10. expression boundaries: division, modulo, string compare, chained bool
-- ---------------------------------------------------------------------------
local exprSrc = '*start\n[set f.a 10]\n[set f.b 3]\n'
    .. '[if exp="f.a / f.b > 3"]\n[ch text="div-ok"]\n[endif]\n'
    .. '[if exp="f.a % f.b == 1"]\n[ch text="mod-ok"]\n[endif]\n'
    .. '[if exp="\'hello\' == \'hello\'"]\n[ch text="str-ok"]\n[endif]\n'
    .. '[if exp="f.a > 5 and f.b < 10"]\n[ch text="chain-ok"]\n[endif]\n[end]\n'
local xctx = d.run_scene(exprSrc)
check("division expression evaluates", xctx._error == nil
      and xctx.backlog[1] == "div-ok")
check("modulo expression evaluates", xctx.backlog[2] == "mod-ok")
check("string equality evaluates", xctx.backlog[3] == "str-ok")
check("chained boolean evaluates", xctx.backlog[4] == "chain-ok")

-- ---------------------------------------------------------------------------
-- 11. deep snapshot equality: nested tables and arrays compare structurally
-- ---------------------------------------------------------------------------
local deepSrc = '*start\n[set f.name Hero]\n'
    .. '[set f.hp 100]\n[set f.items 3]\n'
    .. '[ch text="hi"]\n[end]\n'
local deepA = d.run_scene(deepSrc)
local deepB = d.run_scene(deepSrc)
check("deep snapshot matches across runs",
      d.assert_state(d.state_snapshot(deepA), d.state_snapshot(deepB)))
local snap = d.state_snapshot(deepA)
check("flat fields captured", snap.f.name == "Hero"
      and snap.f.hp == 100 and snap.f.items == 3)
check("assert_state catches field diff",
      not d.assert_state(snap, { f = { name = "Villain" } }))
check("assert_state catches missing field",
      not d.assert_state(snap, { f = { name = "Hero", hp = 100 } }))

-- ---------------------------------------------------------------------------
-- 12. state isolation: two scenes do not leak variables into each other
-- ---------------------------------------------------------------------------
local leakA = d.run_scene('*start\n[set f.shared 42]\n[end]\n')
local leakB = d.run_scene('*start\n[ch text="fresh"]\n[end]\n')
check("scene B does not see scene A variables", leakB.f.shared == nil)

-- Exit gate.
if failed > 0 then
    print(string.format("DETERMINISM TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("DETERMINISM TESTS DONE (%d passed)", passed))