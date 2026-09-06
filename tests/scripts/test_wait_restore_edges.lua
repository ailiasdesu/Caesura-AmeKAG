package.path='scripts/?.lua;scripts/?/init.lua;'..package.path
local passed,failed=0,0
local function check(name,value)
    if value then passed=passed+1;print('PASS '..name)
    else failed=failed+1;print('FAIL '..name) end
end
-- Minimal host bindings for config's real startup path; no decoder or renderer
-- is substituted for the timing logic exercised below.
KAG={set_bus_volume=function() end}
Render={}
DevCore={set_resolution=function() end,set_fullscreen=function() end}
require('kag.init')
check('normal startup preloads wait restoration',package.loaded['kag.wait_state']~=nil)
local system=require('kag.commands.system')
local original_require=require
_G.require=function(name)
    if package.loaded[name]~=nil then return package.loaded[name] end
    error('Sandbox: module not preloaded: '..name)
end
local ctx={current_scene='edge.ks',token_index=1,tokens={{'wait',{time=1}}}}
local co=coroutine.create(function() system.wait(ctx,{time=1}) end)
local started=coroutine.resume(co)
check('first wait runs under loaded-only require',started)
_G.require=original_require
if started then coroutine.resume(co,1) end
check('finished wait releases its capture state',ctx._waitState==nil)
local Wait=require('kag.wait_state')
for _,params in ipairs({{time=''},{ms='',time=1000},{duration='',time=1000}}) do
    local value={scene='edge.ks',token_index=1,remaining_ms=900}
    local ok,state=pcall(Wait.prepare,value,'edge.ks',{{'wait',params}},1)
    check('empty optional duration follows the command schema',ok and state.remaining_ms==900)
end
local pending={current_scene='edge.ks',_resume_index=1,token_index=1,tokens={{'wait',{time=1000}}},
    _restoredWait={scene='edge.ks',token_index=1,remaining_ms=900}}
local value=Wait.capture(pending)
check('capture retains a wait pending its first update',value and value.remaining_ms==900)
print(string.format('WAIT RESTORE EDGES: %d passed, %d failed',passed,failed))
os.exit(failed==0 and 0 or 1)
