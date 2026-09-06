-- Real Layers rendering with a recording backend; no GPU pixel claim.
package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path
local trace, uploads, destroyed, discarded = {}, {}, {}, {}
local nextRT, nextTexture, failedPath, failUpload, failRT = 10, 100, nil, nil, false
local evictDuringUpload=false
local sources = {[1]={kind="asset",path="assets/a.bmp"},
    [2]={kind="color",r=10,g=20,b=30,a=255}}
package.loaded.backend = {
    get_resolution=function() return 1280,720 end,
    create_viewport=function() if failRT then return 0 end; nextRT=nextRT+1; return nextRT end,
    destroy_viewport=function(id) destroyed[#destroyed+1]={"rt",id} end,
    destroy_texture=function(id) destroyed[#destroyed+1]={"texture",id}; sources[id]=nil end,
    is_valid_handle=function(_,id) return sources[id]~=nil end,
    submit_batch=function(batch)
        trace={}
        for i=1,batch[1] do
            local base=1+(i-1)*16
            local record={source=sources[batch[base+2]]}
            for offset=4,16 do record[offset-3]=batch[base+offset] end
            trace[#trace+1]=record
        end
    end,
}
_G.Restore = {
    describe_texture=function(id) return sources[id] end,
    prepare_image=function(path)
        if path==failedPath then return nil,"injected read failure" end
        return {source={kind="asset",path=path}}
    end,
    prepare_color=function(r,g,b,a) return {source={kind="color",r=r,g=g,b=b,a=a}} end,
    image_info=function() return 1,1 end,
    discard_image=function(ticket) discarded[#discarded+1]=ticket; ticket.used=true end,
    materialize_image=function(ticket)
        assert(not ticket.used); ticket.used=true
        if #uploads+1==failUpload then return nil,"injected GPU failure" end
        if evictDuringUpload and #uploads>0 then sources[uploads[#uploads]]=nil end
        nextTexture=nextTexture+1; sources[nextTexture]=ticket.source
        uploads[#uploads+1]=nextTexture
        return nextTexture
    end,
}
local Layers=require("layers")
local state=require("kag.layer_state")
local copy=require("kag.save_state").copy
local encode=require("kag.compiler").encode_lua_literal
local passed,failed=0,0
local function check(name,condition)
    if condition then passed=passed+1 else failed=failed+1; print("FAIL: "..name) end
end
local function picture()
    Layers.init()
    local parent=Layers.add_layer(nil,{id="group",x=10,y=20,z=1})
    local a=Layers.add_layer(parent,{id="a",w=120,h=80,x=3,y=4,z=2,opacity=180})
    a.tex=1; a.scaleX=-1; a.scaleY=2; a.pos_x=7; a.rotation=30
    a.clipX=1; a.clipY=2; a.clipW=80; a.clipH=60
    local b=Layers.add_layer(parent,{id="b",w=20,h=30,z=2})
    b.tex=2
    return a,b
end
local a,b=picture()
Layers.render()
local original=encode(trace)
local saved=state.capture()
local pending=state.prepare(saved)
check("prepare preserves old node identity",Layers.get_layer("a")==a and Layers.get_layer("b")==b)
check("prepare performs no GPU work",#uploads==0 and #destroyed==0)
local owner={}
assert(state.apply(pending,owner))
Layers.render()
check("actual submissions survive restore",encode(trace)==original)
check("nodes are reconstructed independently",Layers.get_layer("a")~=a)
check("equal-z child order survives",Layers.get_layer("group").children[1].id=="a")
check("unique images materialize once",#uploads==2)
check("cannot apply a consumed candidate",not pcall(state.apply,pending,{}))
assert(state.stop(owner))
check("stop clears the visible tree",Layers.count()==1)
local before=#destroyed
assert(state.stop(owner))
check("stop is idempotent",#destroyed==before)

a,b=picture()
local broken=copy(saved)
broken.nodes[4].source={kind="asset",path="assets/bad.bmp"}
failedPath="assets/bad.bmp"
local beforeDiscard=#discarded
check("failed second preparation rejects",not pcall(state.prepare,broken))
check("failed preparation releases earlier tickets",#discarded==beforeDiscard+1)
check("failed preparation preserves old tree",Layers.get_layer("a")==a and Layers.get_layer("b")==b)
failedPath=nil
for _,mutate in ipairs({
    function(s) s.nodes[3].parent="missing" end,
    function(s) s.nodes[3].id=s.nodes[2].id end,
    function(s) s.nodes[3].x=math.huge end,
    function(s) s.nodes[3].w=-1 end,
    function(s) s.nodes[3].w=4097 end,
    function(s) s.nodes[3].visible="false" end,
    function(s) s.nodes[3].source.path="../outside.bmp" end,
}) do
    local invalid=copy(saved); mutate(invalid)
    check("malformed description rejects before mutation",not pcall(state.prepare,invalid)
        and Layers.get_layer("a")==a)
end
local failedOwner={}
failUpload=#uploads+2
local ok=pcall(state.apply,state.prepare(saved),failedOwner)
check("second GPU failure propagates",not ok)
check("GPU failure clears partial tree and textures",Layers.count()==1
    and next(failedOwner._restoredTextures or {})==nil)
failUpload=nil
failRT=true
ok=pcall(state.apply,state.prepare(saved),{})
check("zero RTT is a commit failure",not ok and Layers.count()==1)
failRT=false
assert(state.apply(state.prepare(saved),{}))
Layers.render()
check("a later restore succeeds after failure",encode(trace)==original)
Layers.get_layer("a").quake.active=true
check("active visual effect is an explicit unsupported boundary",not pcall(state.capture))
local cancelled=state.prepare(saved)
state.discard(cancelled)
local current=Layers.get_layer("a")
check("discarded candidate cannot replace a live tree",not pcall(state.apply,cancelled,{})
    and Layers.get_layer("a")==current)
local first,second={},{}
assert(state.apply(state.prepare(saved),first))
assert(state.apply(state.prepare(saved),second))
current=Layers.get_layer("a")
assert(state.stop(first))
check("late old-owner cleanup preserves new tree",Layers.get_layer("a")==current)
local oldDiscard=Restore.discard_image
local calls=0
Restore.discard_image=function(ticket)
    calls=calls+1
    oldDiscard(ticket)
    if calls==1 then error("injected discard error") end
end
pending=state.prepare(saved)
local discardedOk=state.discard(pending)
check("discard tries every ticket after one failure",discardedOk==false and calls==2)
Restore.discard_image=oldDiscard
local oldDestroy=package.loaded.backend.destroy_texture
calls=0
package.loaded.backend.destroy_texture=function(id)
    calls=calls+1; oldDestroy(id)
    if calls==1 then error("injected destruction error") end
end
local stopped=state.stop(second)
check("stop tries all owned textures after one failure",stopped==false and calls==2 and Layers.count()==1)
package.loaded.backend.destroy_texture=oldDestroy
evictDuringUpload=true
local evictedOwner={}
local installed=pcall(state.apply,state.prepare(saved),evictedOwner)
check("eviction during upload cannot publish an incomplete picture",not installed and Layers.count()==1)
check("eviction failure retires the remaining owned textures",next(evictedOwner._restoredTextures or {})==nil)
evictDuringUpload=false
local hostOwner,hostCalls={},0
Layers.install_prepared=function(nodes)
    hostCalls=hostCalls+1
    check("host install receives validated tree",nodes[1].id=="_root" and nodes[3].parent=="group")
    for _,node in ipairs(nodes) do
        if node.image then
            check("host install runs after every image is materialized",node.image.ticket==nil
                and package.loaded.backend.is_valid_handle(0,node.image.id)==true)
        end
    end
    Layers.add_layer(nil,{id="host-installed"})
    return true
end
failRT=true
check("host install replaces native RTT installation",pcall(state.apply,state.prepare(saved),hostOwner)
    and hostCalls==1 and Layers.get_layer("host-installed")~=nil)
failRT=false
before=#destroyed
assert(state.stop(hostOwner))
check("host installation retains shared texture cleanup",#destroyed==before+2 and Layers.count()==1)
Layers.install_prepared=function()
    Layers.add_layer(nil,{id="partial-host"})
    error("injected host installation failure")
end
hostOwner={}
check("host install failure clears its partial tree and ownership",not pcall(state.apply,state.prepare(saved),hostOwner)
    and Layers.count()==1 and next(hostOwner._restoredTextures or {})==nil)
Layers.install_prepared=nil
print(string.format("U11 LAYER RESTORE: %d passed, %d failed",passed,failed))
os.exit(failed==0 and 0 or 1)
