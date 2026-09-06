-- U11: compare real TextScene submissions across a persistent state restore.
package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path
local text = require("kag.text_scene")
local save = require("kag.commands.save")
local state = require("kag.save_state")
local compiler = require("kag.compiler")
local passed, failed = 0, 0
local function check(name, value)
    if value then passed=passed+1 else failed=failed+1; print("FAIL: "..name) end
end
local function draw_trace(ctx)
    local calls = {}
    text.render(ctx, {
        render_text = function(...) calls[#calls+1] = {"text", ...} end,
        render_ruby = function(...) calls[#calls+1] = {"ruby", ...} end,
    })
    return compiler.encode_lua_literal(calls)
end

local ctx = {f={},sf={},tf={},lf={},mp={},variables={},
    current_scene="tests/projects/u11_restore/base.ks",token_index=3,
    textCursorX=40,textCursorY=140,text_speed=50,waiting_input=true,
    reveal={total=8,elapsed=200,last_shown=4},nvl_mode=true,
    text_state={opacity=180,cursor_x=40,cursor_y=140,font_size=24},
}
text.add_text(ctx,"ABCD",40,100,{255,120,40,255},"story",1.5,true,false,false,true)
text.add_text(ctx,"EFGH",40,140,{40,100,220,220},"story",1,false,true,false,false)
text.add_ruby(ctx,"漢字","かんじ",{x=40,y=180,group="ruby",font_size=24})
ctx.text_state.reveal_chars=5
ctx.text_state.page_src={{kind="text",src="original line",scene=ctx.current_scene,
    opts={msgX=40,msgY=100,maxWidth=800,lineHeight=24,color={255,120,40,255}}}}
ctx.text_state.draws[1]._page_src=true
local original = draw_trace(ctx)
local captured = save.capture_state(ctx)
check("save includes a text snapshot", type(captured.text_snapshot)=="table")
check("transient reveal caches are excluded", captured.text_snapshot
    and captured.text_snapshot.state.draws[1]._shown==nil
    and captured.text_snapshot.state.draws[1]._shown_len==nil)
ctx.text_state.draws[1].text="future text"
ctx.text_state.page_src[1].src="future source"
local candidate = state.prepare(captured,save._safeScenePath,require("flow").prepare_scene)
local restored = {text_state={draws={{kind="text",text="future"}}},reveal={total=99}}
state.apply_values(restored,candidate)
check("visible text/ruby submissions match",draw_trace(restored)==original)
check("page source survives independently",restored.text_state.page_src[1]
    and restored.text_state.page_src[1].src=="original line")
check("reveal timing and prior sound boundary survive",restored.reveal
    and restored.reveal.total==8 and restored.reveal.elapsed==200 and restored.reveal.last_shown==4)
check("waiting and speed survive",restored.waiting_input==true and restored.text_speed==50)
check("text cursors survive",restored.textCursorX==ctx.textCursorX and restored.textCursorY==ctx.textCursorY)
check("NVL mode survives",restored.nvl_mode==true)

for _, mutate in ipairs({
    function(s) s.state.draws[1].x="bad" end,
    function(s) s.state.draws[1].kind="unknown" end,
    function(s) s.state.reveal_chars=-1 end,
    function(s) s.reveal.elapsed=math.huge end,
    function(s) s.reveal.total=7.5 end,
    function(s) s.reveal.last_shown=false end,
    function(s) s.state.reveal_chars=1.5 end,
    function(s) s.waiting_input="yes" end,
    function(s) s.state.draws[1].group=math.huge end,
    function(s) s.state.page_src[1].scene={} end,
    function(s) s.state.page_src[1].opts.msgY="bad" end,
    function(s) s.state.page_src[1].opts.nvl="yes" end,
    function(s) s.state.page_src[1].opts.color={r="bad",g=255,b=255,a=255} end,
}) do
    if captured.text_snapshot then
        local bad = state.copy(captured)
        mutate(bad.text_snapshot)
        check("malformed text state rejects before apply",
            not pcall(state.prepare,bad,save._safeScenePath,require("flow").prepare_scene))
    else check("malformed text state rejects before apply",false) end
end
print(string.format("U11 TEXT RESTORE: %d passed, %d failed",passed,failed))
os.exit(failed==0 and 0 or 1)
