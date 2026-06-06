## Phase 9: 安全与平台 (P0+P2)

**目标:** [eval]/[emb] 沙箱白名单、CARC 链式信任+CRL、移动平台预留接口。

**依赖:** Phase 1 (沙箱), Phase 7 (CARC) | **验收:** [eval] 无法调用 os.execute(), 子 CARC 验签+证书

### Task 9.1: Lua Sandbox ([eval]/[emb] _ENV 白名单)
**Files:** Modify: `scripts/kag/commands/system.lua` (eval/emb 实现), Create: `scripts/sandbox.lua`

```lua
-- scripts/sandbox.lua
local sandbox = {}

-- 白名单: KAG, Render, math, string, table
-- 黑名单: os, io, package, debug, require, dofile, loadfile, _G
local whitelist = {
    KAG = true, Render = true, math = true, string = true, table = true,
    pairs = pairs, ipairs = ipairs, next = next, type = type,
    tostring = tostring, tonumber = tonumber, pcall = pcall,
    error = error, assert = assert, select = select,
    print = log,  -- 重定向到引擎日志
}

function sandbox.create(env_table)
    return setmetatable({}, {
        __index = function(t, k)
            if whitelist[k] then return _G[k] end
            if env_table[k] ~= nil then return env_table[k] end
            return nil  -- undefined -> nil (不访问 _G)
        end,
        __newindex = function(t, k, v)
            env_table[k] = v
        end,
    })
end

-- 禁止控制流命令: [jump]/[link]/[call]/[return_]/[end_]/[trans]/[move]/playvoice
local forbidden_cmds = {
    jump=true, link=true, call=true, ["return_"]=true, ["end_"]=true,
    trans=true, move=true, quake=true, fade=true, playvoice=true, wait=true
}

function sandbox.execute(code, env, cancelToken)
    local sandbox_env = sandbox.create(env)
    -- 注入 CancelToken 引用 (用于取消检测)
    sandbox_env._cancel_token = cancelToken
    local fn, err = load(code, "=eval", "t", sandbox_env)
    if not fn then return nil, err end
    return pcall(fn)
end

-- Dev/Release 开关
function sandbox.is_strict()
    return not config.dev_mode  -- Release 强制沙箱
end
```

### Task 9.2: CARC 链式信任 + CRL
**Files:** Modify: `src/carc/CARCReader.cpp` (证书验证), Create: `src/carc/CRLManager.h`

```cpp
// 链式信任验证流程 (与 [10.2.63] 一致):
// 1. 读取子 CARC 末尾 32B 公钥
// 2. 从主 CARC chain/ 虚拟路径读取授权证书 (JSON, 含子公钥哈希+有效期+权限)
// 3. 主密钥 Ed25519 验证证书签名
// 4. 比对证书内公钥哈希与子 CARC 末尾公钥的 SHA-256
// 5. 子公钥验签子 CARC (Ed25519)
// 6. 检查证书有效期 (expiry: "none" 或指定日期)
// 7. 检查 CRL 吊销列表 (离线: 本地缓存, 在线: HTTPS carc_crl_url, 混合: 在线->超时5s->缓存)

// CRL 格式:
{
    "version": 3,
    "timestamp": "2026-06-06T12:00:00Z",
    "revoked_fingerprints": ["sha256_of_public_key1", "sha256_of_public_key2"],
    "signature": "Ed25519_signature_by_root_key"
}
```

```cpp
class CRLManager {
public:
    enum class CRLMode { Offline, Online, Hybrid };

    bool verify(const std::string& pubKeyFingerprint, CRLMode mode);
    bool fetchOnline(const std::string& url);   // HTTPS -> 本地缓存
    bool loadLocalCache();                       // saves/crl_cache.json
    bool isRevoked(const std::string& fingerprint);

private:
    std::unordered_set<std::string> m_revoked;
    uint32_t m_localVersion = 0;
    std::string m_localTimestamp;
};
```

### Task 9.3: 移动平台预留 (P2)
**Files:** Create: `src/platform/MobileAdapter.h`

```cpp
// src/platform/MobileAdapter.h
#pragma once

namespace caesura::platform {

class MobileAdapter {
public:
    // 生命周期钩子 (P2)
    // SDL_EVENT_DID_ENTER_BACKGROUND -> onPause()
    // SDL_EVENT_DID_ENTER_FOREGROUND -> onResume(savedData)
    static void onPause(lua_State* L);    // 暂停 SoLoud + Lua 钩子 _G.onPause()
    static void onResume(lua_State* L);   // 恢复 + Lua 钩子 _G.onResume(savedData)

    // 触控手势 (P2)
    // 单指->鼠标左键, 双指缩放->预留, 长按>500ms->右键菜单
    static void onFingerDown(float x, float y);
    static void onFingerMotion(float x, float y);
    static void onFingerUp(float x, float y);

    // 字体缩放 (移动端 DPI)
    static float getDisplayScale();
};

} // namespace
```

### Phase 9 检查点
- [x] [eval] 沙箱: 无法调用 os.execute() / io.open()
- [x] 禁止控制流命令在 [eval] 内执行
- [x] 子 CARC 链式验证 (证书+CRL)
- [x] CRL 三种模式: offline/online/hybrid
- [x] 移动端生命周期钩子 (P2 预留)

---
