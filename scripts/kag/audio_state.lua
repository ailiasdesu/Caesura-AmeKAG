local copy = require("kag.save_state").copy
local backend = require("backend")
local M = {}

local function sample_integer(value, maximum)
    return type(value)=="number" and value>=0 and value<=maximum and value%1==0
end

local function validate(snapshot)
    snapshot=copy(snapshot==nil and {version=1,bgm=false} or snapshot)
    if type(snapshot)~="table" or snapshot.version~=1 then error("Invalid saved audio version",0) end
    if snapshot.bgm==false then return {version=1,bgm=false} end
    local bgm=snapshot.bgm
    if type(bgm)~="table" or type(bgm.path)~="string" or #bgm.path==0 or #bgm.path>4096
        or bgm.path:sub(1,1)=="/" or bgm.path:find("..",1,true) or bgm.path:find("[%z\1-\31\\:]")
        or type(bgm.position)~="number" or bgm.position<0 or bgm.position>86400
        or type(bgm.gain)~="number" or bgm.gain<0 or bgm.gain>16
        or type(bgm.looping)~="boolean" then error("Invalid saved BGM state",0) end
    local value={path=bgm.path,position=bgm.position,gain=bgm.gain,looping=bgm.looping}
    if bgm.cursor~=nil then
        local cursor=bgm.cursor
        if type(cursor)~="table" or not sample_integer(cursor.frame,17592186044415)
            or not sample_integer(cursor.fraction,1048575)
            or not sample_integer(cursor.source_rate,4294967295) or cursor.source_rate==0
            or not sample_integer(cursor.output_rate,4294967295) or cursor.output_rate==0 then
            error("Invalid saved audio sample cursor",0)
        end
        value.cursor={frame=cursor.frame,fraction=cursor.fraction,
            source_rate=cursor.source_rate,output_rate=cursor.output_rate}
    end
    return {version=1,bgm=value}
end

function M.capture()
    local api=rawget(_G,"Restore")
    if api and type(api.capture_audio)=="function" then return validate(assert(api.capture_audio())) end
    if type(backend.audio_is_playing)=="function" and backend.audio_is_playing("bgm")==true then
        error("Playing audio has no restore interface",0)
    end
    -- Pure script hosts with no audio have an explicit silent declaration.
    return {version=1,bgm=false}
end

function M.prepare(snapshot)
    local value=validate(snapshot)
    local api=rawget(_G,"Restore")
    local prepared={api=api,used=false}
    if api and type(api.prepare_audio)=="function" then
        prepared.ticket=assert(api.prepare_audio(value))
    elseif value.bgm~=false then error("Audio preparation unavailable",0) end
    return prepared
end

function M.discard(prepared)
    prepared.used=true
    local ticket=prepared.ticket
    prepared.ticket=nil
    if ticket then prepared.api.discard_audio(ticket) end
end

function M.stop()
    local api=rawget(_G,"Restore")
    if api and type(api.stop_audio)=="function" then return api.stop_audio() end
    if type(backend.audio_stop)=="function" then
        for _,kind in ipairs({"bgm","voice","se"}) do backend.audio_stop(kind) end
    end
    return true
end

function M.apply(prepared)
    if prepared.used then error("Audio candidate already consumed",0) end
    prepared.used=true
    local ticket=prepared.ticket
    prepared.ticket=nil
    if ticket then assert(prepared.api.apply_audio(ticket))
    else assert(M.stop()) end
    return true
end

return M
