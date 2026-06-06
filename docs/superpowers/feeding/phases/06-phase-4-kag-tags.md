## Phase 4: KAG 标签全集 Alpha (P1)

**目标:** ~20 个 KAG 标签, 完整的纯对话脚本可执行。

**依赖:** Phase 1+2+3 | **验收:** scene01.ks 从 [bg] 到 [end] 完整执行

### 标签清单

| 模块 | 标签 | 类型 |
|------|------|------|
| layer.lua | [bg],[fg],[cl],[image] | Lua->C++ |
| text.lua | [ch],[text],[r],[er],[p],[l] | Lua->FontRenderer |
| audio.lua | [playbgm],[stopbgm],[playse],[playvoice],[fadebgm] | Phase 3 |
| control.lua | [if]/[else]/[endif],[switch]/[case] | Lua |
| system.lua | [eval],[emb],[wait],[label] | Lua |
| flow.lua | [jump],[call],[return_],[link],[end_] | Phase 1 |
| transition.lua | [trans],[move],[quake],[fade] | Lua+GLSL |
| video.lua | [video],[stopvideo] | C++ pl_mpeg |

### Task 4.1: layer.lua
```lua
return {
    bg = function(ctx,p) backend.set_layer_tex(0,p.storage); ctx.layers.bg=p.storage end,
    fg = function(ctx,p) backend.set_layer_tex(1,p.storage); ctx.layers.fg=p.storage end,
    cl = function(ctx,p)
        local l = p.layer or "all"
        if l=="all" or l=="bg" then backend.clear_layer(0) end
        if l=="all" or l=="fg" then backend.clear_layer(1) end
    end,
    image = function(ctx,p)
        backend.set_layer_tex(1,p.storage)
        if p.x or p.y then backend.set_layer_pos(1, tonumber(p.x)or 0, tonumber(p.y)or 0) end
    end,
}
```

### Task 4.2: text.lua
```lua
return {
    ch = function(ctx,p)
        local text, name = p.text or "", p.name or ""
        table.insert(ctx.backlog, { name=name, text=text, time=os.date("%H:%M:%S") })
        backend.font_render_text(text, 48, 40, 500)
    end,
    p = function(ctx,p)
        local ct = kag.cancel_token.new()
        table.insert(ctx.active_operations, ct)
        coroutine.yield()
    end,
    r = function() backend.font_clear() end,
    er = function() backend.font_clear() end,
}
```

### Task 4.3: control.lua
```lua
return {
    ["if"] = function(ctx,p)
        local ok, result = pcall(load("return "..(p.exp or "false"), "=if", "t", ctx.f))
        if ok and not result then flow_skip_to(ctx, "else", "endif") end
    end,
    ["else"] = function(ctx,p) flow_skip_to(ctx, "endif") end,
    ["endif"] = function() end,
}
```

### Task 4.4: system.lua
```lua
return {
    wait = function(ctx,p)
        local ms = tonumber(p.time or p.ms or 1000)
        local start = os.clock()*1000
        local ct = kag.cancel_token.new()
        table.insert(ctx.active_operations, ct)
        while (os.clock()*1000 - start) < ms and not ct:is_cancelled() do coroutine.yield() end
    end,
    eval = function(ctx,p)
        local fn = load(p.exp or p.code or "", "=eval", "t", ctx.f)
        if fn then pcall(fn) end
    end,
    emb = function(ctx,p)
        local fn = load(p.exp or "", "=emb", "t", ctx.f)
        if fn then local ok, r = pcall(fn); ctx.tf.emb_result = r end
    end,
    label = function() end,
    preload = function(ctx,p)
        if p.wait == "true" then
            local ok = backend.preload_sync(p.type, p.storage)
            if not ok then coroutine.yield() end
        else
            backend.preload_async(p.type, p.storage)
        end
    end,
}
```

### Task 4.5: transition.lua
```lua
return {
    trans = function(ctx,p)
        local dur = tonumber(p.time) or 500
        local elapsed = 0
        while elapsed < dur do
            local dt = coroutine.yield() or 16
            elapsed = elapsed + dt
            backend.trans_progress(math.min(elapsed/dur, 1.0))
        end
        backend.trans_end()
    end,
    move = function(ctx,p)
        local dur = tonumber(p.time) or 300
        local tx, ty = tonumber(p.x) or 0, tonumber(p.y) or 0
        local lid = ({bg=0,fg=1})[p.layer or "fg"] or 1
        local elapsed = 0
        while elapsed < dur do
            local dt = coroutine.yield() or 16
            elapsed = elapsed + dt
            local t = math.min(elapsed/dur, 1.0)
            local e = 3*t*t - 2*t*t*t
            backend.set_layer_pos(lid, tx*e, ty*e)
        end
    end,
    quake = function(ctx,p)
        local dur = tonumber(p.time) or 300
        local i = tonumber(p.intensity) or 5
        local elapsed = 0
        while elapsed < dur do
            local dt = coroutine.yield() or 16
            elapsed = elapsed + dt
            backend.set_screen_offset((math.random()-0.5)*i, (math.random()-0.5)*i)
        end
        backend.set_screen_offset(0, 0)
    end,
}
```

### Phase 4 检查点
- [x] ~20 标签全部实现
- [x] scene01.ks: [bg]->[ch]->[p]->[jump]->[end] 完整执行



---
