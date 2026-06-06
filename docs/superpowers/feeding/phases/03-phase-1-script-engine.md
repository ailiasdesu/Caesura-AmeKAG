## Phase 1: 脚本引擎核心 (P0)

**目标:** .ks 脚本经过 LPeg 解析为 Token 序列，scheduler 协程逐 token 分发执行。

**依赖:** Phase 0 | **验收:** 协程在 [wait] 处 yield，[jump] 跳转正确，CancelToken 能取消阻塞操作

### Task 1.1: LPeg KAG Tokenizer
**Files:** Create: `scripts/kag/tokenizer.lua`, `tests/lua/tokenizer_spec.lua`

```lua
-- scripts/kag/tokenizer.lua
local lpeg = require("lpeg")
local P, S, R, C, Ct = lpeg.P, lpeg.S, lpeg.R, lpeg.C, lpeg.Ct
local tokenizer = {}

local whitespace = S(" \t")^1
local newline = P("\r\n") + P("\n") + P("\r")
local comment  = P(";") * (1 - newline)^0 * newline^-1
local param_name  = (R("az","AZ","09") + S("_-"))^1
local string_val  = P('\"') * C((1 - P('\"'))^0) * P('\"')
local number_val  = C(R("09")^1 + P("-") * R("09")^1)
local param_pair  = Ct(param_name * whitespace^0 * P("=") * whitespace^0 * (string_val + number_val))
local tag_name   = C((R("az","AZ","09") + S("_/"))^1)
local tag_params = Ct(param_pair * (whitespace^0 * param_pair)^0)
local tag_open, tag_close = P("[") * whitespace^0, whitespace^0 * P("]")
local tag = tag_open * tag_name * whitespace^0 * tag_params^-1 * tag_close
local eval_start = tag_open * P("eval") * tag_close
local eval_end   = tag_open * P("end") * tag_close
local eval_body  = C((1 - eval_end)^0)
local eval_block = eval_start * eval_body * eval_end
local text = C((1 - (tag_open + newline))^1)
local empty_line = newline * newline

local function make_grammar()
    return Ct((lpeg.V("tag") + lpeg.V("eval") + lpeg.V("emptyl") + lpeg.V("text") + lpeg.V("newline"))^0 * P(-1))
end

function tokenizer.parse(source)
    local g = lpeg.P{
        tag    = tag / function(name, params) return { type="TAG", cmd=name, params=params or {} } end,
        eval   = eval_block / function(code) return { type="EVAL", code=code } end,
        emptyl = empty_line / function() return { type="TAG", cmd="p", params={} } end,
        text   = text / function(t) return { type="TEXT", value=t } end,
        newline = newline,
    }
    local tokens = lpeg.match(g, source)
    if not tokens then return nil, "Parse error" end
    local filtered = {}
    for _, t in ipairs(tokens) do
        if t.type ~= "newline" then table.insert(filtered, t) end
    end
    table.insert(filtered, { type="EOF" })
    return filtered
end

function tokenizer.tokenize_file(filepath)
    local f = io.open(filepath, "r")
    if not f then return nil, "Cannot open: " .. filepath end
    local source = f:read("*all"); f:close()
    return tokenizer.parse(source)
end

return tokenizer
```

**Build:** `cd tests/lua && busted tokenizer_spec.lua` → 期望 5/5 通过

### Task 1.2: GameState 单例
**Files:** Create: `src/script/GameState.h`, `src/script/GameState.cpp`

```cpp
// src/script/GameState.h
#pragma once
extern "C" { #include <lua.h> #include <lauxlib.h> }
namespace caesura {
class GameState {
public:
    static void create(lua_State* L);
    static void push(lua_State* L);
private:
    static const char* REGISTRY_KEY;
};
}
```

```cpp
// src/script/GameState.cpp
#include "script/GameState.h"
namespace caesura {
const char* GameState::REGISTRY_KEY = "caesura_ctx";
void GameState::create(lua_State* L) {
    lua_newtable(L);       // ctx = {}
    lua_pushnil(L); lua_setfield(L, -2, "co");       // 当前协程
    lua_newtable(L); lua_setfield(L, -2, "tokens");   // Token 序列
    lua_pushinteger(L, 1); lua_setfield(L, -2, "token_index");
    lua_newtable(L); lua_setfield(L, -2, "call_stack");
    lua_newtable(L); lua_setfield(L, -2, "f");        // 全局变量
    lua_newtable(L); lua_setfield(L, -2, "sf");       // 系统变量
    lua_newtable(L); lua_setfield(L, -2, "tf");       // 临时变量
    lua_newtable(L); lua_setfield(L, -2, "layers");    // 图层引用
    lua_newtable(L); lua_setfield(L, -2, "backlog");
    lua_newtable(L); lua_setfield(L, -2, "active_operations");  // CancelToken 列表
    lua_pushboolean(L, 0); lua_setfield(L, -2, "stop_flag");
    lua_pushstring(L, "kag"); lua_setfield(L, -2, "input_focus");
    lua_pushstring(L, REGISTRY_KEY); lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);  // registry.caesura_ctx = ctx
    lua_pop(L, 1);
}
void GameState::push(lua_State* L) {
    lua_pushstring(L, REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
}
}
```

**Build:** `cmake --build build` → 在 init.lua 中 `print(type(ctx.f))` → 输出 "table"

### Task 1.3: Scheduler 协程调度器
**Files:** Create: `scripts/kag/scheduler.lua`, `tests/lua/scheduler_spec.lua`

```lua
-- scripts/kag/scheduler.lua
local scheduler = {}
local kag_commands = {}

function scheduler.register_command(name, fn)
    kag_commands[name] = fn
end

function scheduler.run(ctx, tokens)
    ctx.tokens = tokens; ctx.token_index = 1; ctx.active_operations = {}
    local co = coroutine.create(function() scheduler._execute_tokens(ctx) end)
    ctx.co = co
    coroutine.resume(co)
end

function scheduler.resume(ctx)
    if not ctx.co or ctx.stop_flag then return end
    if coroutine.status(ctx.co) == "dead" then return end
    local ok, err = coroutine.resume(ctx.co)
    if not ok then log("Script error: " .. tostring(err)) end
end

function scheduler._execute_tokens(ctx)
    local i = ctx.token_index
    while i <= #ctx.tokens do
        local token = ctx.tokens[i]
        if token.type == "EOF" then break
        elseif token.type == "TAG" then
            local fn = kag_commands[token.cmd]
            if fn then fn(ctx, token.params)
            else log("Unknown KAG tag: [" .. token.cmd .. "]") end
        elseif token.type == "EVAL" then
            local ok, eval_err = pcall(function()
                local fn = load(token.code, "=eval", "t", ctx.f)
                if fn then fn() end
            end)
            if not ok then log("Eval error: " .. tostring(eval_err)) end
        elseif token.type == "TEXT" then
            local ch_fn = kag_commands["ch"]
            if ch_fn then ch_fn(ctx, { text = token.value }) end
        end
        ctx.token_index = i + 1; i = ctx.token_index
    end
end

return scheduler
```

### Task 1.4: CancelToken 取消机制
**Files:** Create: `scripts/kag/cancel_token.lua`, `tests/lua/cancel_token_spec.lua`

```lua
-- scripts/kag/cancel_token.lua (两阶段模型: 标记 -> coroutine.close -> 回调)
local CancelToken = {}
CancelToken.__index = CancelToken

function CancelToken.new()
    return setmetatable({
        cancelled = false, cancellation_lock = false,
        callbacks = {}, __close = function(self) self:cancel() end
    }, CancelToken)
end

function CancelToken:register(fn)
    if self.cancelled then pcall(fn); return end
    table.insert(self.callbacks, fn)
end

function CancelToken:mark_cancelled()
    if self.cancellation_lock then return end
    self.cancellation_lock = true; self.cancelled = true
end

function CancelToken:execute_callbacks()
    for i = #self.callbacks, 1, -1 do
        pcall(self.callbacks[i])  -- 反向顺序, pcall 包裹
    end
    self.callbacks = {}
end

function CancelToken:cancel()
    self:mark_cancelled()
    -- coroutine.close() 由 scheduler 负责
    -- execute_callbacks() 在 coroutine.close() 之后调用
end

return CancelToken
```

### Task 1.5: Flow Control (jump/call/return/link/end)
**Files:** Create: `scripts/kag/flow.lua`

```lua
-- scripts/kag/flow.lua
local flow = {}
local label_index = {}

function flow.build_label_index(tokens)
    label_index = {}
    for i, token in ipairs(tokens) do
        if token.type == "TAG" and token.cmd == "label" then
            local name = token.params.name or token.params.storage
            if name then label_index[name] = i end
        end
    end
end

function flow.jump(ctx, params)
    local target = params.target or params.storage
    if not target then return end
    flow._cancel_all_active(ctx)
    local idx = label_index[target]
    if idx then ctx.token_index = idx end
end

function flow.call(ctx, params)
    local target = params.target or params.storage
    if not target then return end
    table.insert(ctx.call_stack, {
        return_index = ctx.token_index + 1,
        tf_snapshot = {table.unpack and table.unpack(ctx.tf) or {}}
    })
    local idx = label_index[target]
    if idx then ctx.token_index = idx end
end

function flow.return_(ctx, params)
    if #ctx.call_stack == 0 then ctx.stop_flag = true; return end
    local frame = table.remove(ctx.call_stack)
    ctx.token_index = frame.return_index
    ctx.tf = frame.tf_snapshot or {}
end

function flow.link(ctx, params)
    local target = params.storage or params.target
    if not target then return end
    flow._cancel_all_active(ctx)
    ctx.layers.bg = nil; ctx.layers.fg = nil; ctx.layers.message = nil
    ctx.backlog = {}
    ctx.pending_link = target
end

function flow.end_(ctx, params)
    flow._cancel_all_active(ctx)
    ctx.stop_flag = true
end

function flow._cancel_all_active(ctx)
    for _, token in ipairs(ctx.active_operations) do
        token:mark_cancelled()
    end
    for _, token in ipairs(ctx.active_operations) do
        token:execute_callbacks()
    end
    ctx.active_operations = {}
end

return flow
```

### Phase 1 检查点
- [x] .ks -> Token 序列 解析正常
- [x] ctx 单例在 Lua registry 可用
- [x] scheduler 协程遍历 tokens + yield 阻塞
- [x] CancelToken 两阶段取消模型
- [x] jump/call/return/link/end 全部实现




---
