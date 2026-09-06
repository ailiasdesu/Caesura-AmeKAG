local M={}
function M.duration(params)
    local function number(value)
        if value==nil or value=='' then return nil end
        local result=tonumber(value)
        if not result or result~=result then error('Invalid wait duration',0) end
        return result
    end
    local ms=number(params.ms) or number(params[1]) or number(params.duration) or number(params.time) or 1000
    return math.max(0,math.min(60000,ms))
end
function M.prepare(value,scene,tokens,index)
    if value==nil then return nil end
    local token=tokens and tokens[index]
    local command=token and (token[1] or token.cmd)
    if type(value)~='table' or value.scene~=scene or value.token_index~=index
        or (command~='wait' and command~='delay') then error('Invalid saved wait cursor',0) end
    local remaining=value.remaining_ms
    if type(remaining)~='number' or remaining~=remaining or remaining<0
        or remaining>M.duration(token[2] or token.params or {}) then error('Invalid saved wait duration',0) end
    return {scene=scene,token_index=index,remaining_ms=remaining}
end
function M.capture(ctx)
    local active=ctx._waitState
    return M.prepare(active or ctx._restoredWait,ctx.current_scene or ctx.currentScene or '',
        ctx.tokens,active and (ctx._executing_index or ctx.token_index) or (ctx._resume_index or ctx.token_index))
end
return M
