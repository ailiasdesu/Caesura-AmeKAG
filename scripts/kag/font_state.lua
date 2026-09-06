local copy=require("kag.save_state").copy
local M={}

local function validate(snapshot)
    local value=copy(snapshot)
    if type(value)~="table" or value.version~=1 or type(value.active)~="boolean" then
        error("Invalid saved font state",0)
    end
    if not value.active then return {version=1,active=false} end
    if type(value.font)~="number" or value.font~=math.floor(value.font) or value.font<0 or value.font>2
        or type(value.path)~="string" or type(value.size)~="number" or value.size~=math.floor(value.size)
        or value.size<1 or value.size>256 then error("Invalid saved font selection",0) end
    if value.font==2 then
        if #value.path==0 or #value.path>4096 or value.path:sub(1,1)=="/"
            or value.path:find("..",1,true) or value.path:find("[%z\1-\31\\:]") then
            error("Invalid saved font asset path",0)
        end
    elseif value.path~="" or value.size~=(value.font==0 and 16 or 32) then
        error("Invalid saved bitmap font",0)
    end
    return {version=1,active=true,font=value.font,path=value.path,size=value.size}
end

function M.capture()
    local api=rawget(_G,"Restore")
    if api and type(api.capture_font)=="function" then return validate(assert(api.capture_font())) end
    return {version=1,active=false}
end

function M.prepare(snapshot)
    local api=rawget(_G,"Restore")
    if snapshot==nil then
        if api and type(api.default_font)=="function" then snapshot=assert(api.default_font())
        elseif api and type(api.prepare_font)=="function" then error("Default font unavailable",0)
        else snapshot={version=1,active=false} end
    end
    local value=validate(snapshot)
    local prepared={api=api,used=false}
    if api and type(api.prepare_font)=="function" then prepared.ticket=assert(api.prepare_font(value))
    elseif value.active then error("Font preparation unavailable",0) end
    return prepared
end

function M.discard(prepared)
    prepared.used=true
    local ticket=prepared.ticket
    prepared.ticket=nil
    if ticket then prepared.api.discard_font(ticket) end
end

function M.apply(prepared)
    if prepared.used then error("Font candidate already consumed",0) end
    prepared.used=true
    local ticket=prepared.ticket
    prepared.ticket=nil
    if ticket then assert(prepared.api.apply_font(ticket)) end
    return true
end

function M.clear()
    local api=rawget(_G,"Restore")
    if api and type(api.clear_font)=="function" then return api.clear_font() end
    return true
end

return M
