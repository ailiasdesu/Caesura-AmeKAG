## Phase 0.5: Lua↔C++ 统一抽象接口层 (贯穿所有 Phase)

**目标:** 建立 Lua 层通过统一 backend 抽象接口调用 C++ 层的架构。所有引擎能力通过单一 `backend` 表暴露，资源创建走工厂模式，句柄有统一生命周期管理。

**这是整个引擎架构的脊梁**——每一个 Phase 的 Lua 代码都通过这层接口与 C++ 交互，而非直接调用 C 函数。

---

### 架构原则

```
┌──────────────────────────────────────────────┐
│  KAG 脚本 / Lua 库 / 游戏逻辑                 │
│  ↓ 只通过一个入口                             │
│  backend.lua   (Lua 端统一门面)               │
│  ↓ C API 绑定                                 │
│  BackendAPI.h  (C++ 端统一注册器)             │
│  ↓ 分发到子模块                               │
│  TextureManager / AudioBackend / LayerManager │
│  FontRenderer / VideoPlayer / InputManager    │
│  ↓                                            │
│  bgfx / SoLoud / SDL3 / FreeType              │
└──────────────────────────────────────────────┘
```

**硬约束:**
- Lua 代码**永远不**直接调用 C 函数 (如 `c_texture_load()`)
- Lua 代码**永远不**直接访问 bgfx/SoLoud/SDL C API
- 所有 C++ 能力通过 `backend.xxx()` 单一命名空间暴露
- 所有资源创建走工厂方法，返回统一句柄
- 所有句柄支持 `backend.is_valid(handle)` 检查

---

### Task 0.5.1: C++ BackendAPI 统一注册器

**Files:** Create: `src/script/BackendAPI.h`, `src/script/BackendAPI.cpp`
**Modify:** `src/script/LuaEngine.cpp` (在 registerCoreBindings 中调用 BackendAPI)

```cpp
// src/script/BackendAPI.h
#pragma once
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace caesura {

// 统一句柄类型 (Lua 侧为 lightuserdata + metatable)
enum class HandleType : uint8_t {
    TEXTURE   = 0,
    AUDIO     = 1,
    VIEWPORT  = 2,
    RTT       = 3,
    SHADER    = 4,
    TRANSITION = 5,
    FONT_ATLAS = 6,
};

// 统一资源句柄 (C++ 内部)
struct ResourceHandle {
    HandleType type;
    uint32_t id;
    uint32_t generation;  // 热重载时递增, is_valid 双重校验
    void* rawHandle;       // bgfx::TextureHandle* / SoLoud::handle* 等

    bool isValid() const { return id != 0; }
};

// BackendAPI: 将所有引擎能力注册到 Lua `backend` 表
class BackendAPI {
public:
    // 主入口: 在 Lua 全局创建 backend 表并注册所有子模块
    static void registerAll(lua_State* L);

private:
    // 各子系统注册函数
    static void registerTextureAPI(lua_State* L, lua_CFunction* methods);
    static void registerAudioAPI(lua_State* L, lua_CFunction* methods);
    static void registerLayerAPI(lua_State* L, lua_CFunction* methods);
    static void registerFontAPI(lua_State* L, lua_CFunction* methods);
    static void registerInputAPI(lua_State* L, lua_CFunction* methods);
    static void registerVideoAPI(lua_State* L, lua_CFunction* methods);
    static void registerViewportAPI(lua_State* L, lua_CFunction* methods);
    static void registerSystemAPI(lua_State* L, lua_CFunction* methods);
    static void registerDebugAPI(lua_State* L, lua_CFunction* methods);

    // 统一句柄管理
    static ResourceHandle* pushHandle(lua_State* L, HandleType type,
                                       uint32_t id, void* raw);
    static ResourceHandle* checkHandle(lua_State* L, int index, HandleType expected);
    static bool isValidHandle(lua_State* L, int index);
};

} // namespace caesura
```

```cpp
// src/script/BackendAPI.cpp
#include "script/BackendAPI.h"
#include "render/TextureManager.h"
#include "render/LayerManager.h"
#include "render/FontRenderer.h"
#include "audio/AudioBackend.h"
#include "input/InputManager.h"
#include "system/Logger.h"

namespace caesura {

void BackendAPI::registerAll(lua_State* L) {
    // 创建 backend 表
    lua_newtable(L);

    // === 纹理 API ===
    registerTextureAPI(L);

    // === 音频 API ===
    registerAudioAPI(L);

    // === 图层 API ===
    registerLayerAPI(L);

    // === 字体 API ===
    registerFontAPI(L);

    // === 输入 API ===
    registerInputAPI(L);

    // === 视频 API ===
    registerVideoAPI(L);

    // === 视口 API (3D) ===
    registerViewportAPI(L);

    // === 系统 API ===
    registerSystemAPI(L);

    // === 调试 API ===
    registerDebugAPI(L);

    // === 统一句柄 API ===
    // backend.is_valid(handle) -> bool
    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_pushboolean(L, isValidHandle(L, 1) ? 1 : 0);
        return 1;
    });
    lua_setfield(L, -2, "is_valid");

    // backend.destroy(handle)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        if (!isValidHandle(L, 1)) return 0;
        ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
        // 根据类型释放
        switch (h->type) {
            case HandleType::TEXTURE:
                TextureManager::instance().releaseTexture(
                    *static_cast<bgfx::TextureHandle*>(h->rawHandle));
                break;
            case HandleType::AUDIO:
                AudioBackend::instance().stop(h->id);
                break;
            // ... 其他类型
        }
        h->id = 0;  // 标记无效
        return 0;
    });
    lua_setfield(L, -2, "destroy");

    // 设置为全局变量
    lua_setglobal(L, "backend");
    LOG_INFO("Script", "BackendAPI registered (9 subsystems)");
}

// --- 纹理 API 注册示例 ---

void BackendAPI::registerTextureAPI(lua_State* L) {
    // backend.texture_load(path) -> TextureHandle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        bgfx::TextureHandle tex = TextureManager::instance().loadTexture(path);
        if (!bgfx::isValid(tex)) {
            lua_pushnil(L);
            return 1;
        }
        // 返回统一句柄 (工厂方法)
        static bgfx::TextureHandle s_handles[256];
        static int s_idx = 0;
        s_handles[s_idx] = tex;
        pushHandle(L, HandleType::TEXTURE, s_idx, &s_handles[s_idx]);
        s_idx = (s_idx + 1) % 256;
        return 1;
    });
    lua_setfield(L, -2, "texture_load");

    // backend.texture_get_size(handle) -> width, height
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::TEXTURE);
        if (!h) { lua_pushnil(L); lua_pushnil(L); return 2; }
        uint16_t w, hgt;
        TextureManager::instance().getTextureSize(
            *static_cast<bgfx::TextureHandle*>(h->rawHandle), w, hgt);
        lua_pushinteger(L, w);
        lua_pushinteger(L, hgt);
        return 2;
    });
    lua_setfield(L, -2, "texture_get_size");

    // backend.texture_create_solid(r, g, b, a) -> TextureHandle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        uint8_t r = (uint8_t)luaL_checkinteger(L, 1);
        uint8_t g = (uint8_t)luaL_checkinteger(L, 2);
        uint8_t b = (uint8_t)luaL_checkinteger(L, 3);
        uint8_t a = (uint8_t)luaL_optinteger(L, 4, 255);
        bgfx::TextureHandle tex = TextureManager::instance()
            .createSolidTexture(r, g, b, a);
        // ...
        return 1;
    });
    lua_setfield(L, -2, "texture_create_solid");
}

// --- 音频 API 注册示例 ---

void BackendAPI::registerAudioAPI(lua_State* L) {
    // backend.audio_play(path, bus, volume) -> AudioHandle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        int bus = (int)luaL_checkinteger(L, 2);
        float vol = (float)luaL_optnumber(L, 3, 1.0);
        unsigned int id = AudioBackend::instance().play(
            path, static_cast<AudioBus>(bus), vol);
        if (id == 0) { lua_pushnil(L); return 1; }
        pushHandle(L, HandleType::AUDIO, id, nullptr);
        return 1;
    });
    lua_setfield(L, -2, "audio_play");

    // backend.audio_stop(handle)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::AUDIO);
        if (h) AudioBackend::instance().stop(h->id);
        return 0;
    });
    lua_setfield(L, -2, "audio_stop");

    // backend.audio_fade(handle, to, duration)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::AUDIO);
        if (!h) return 0;
        float to = (float)luaL_checknumber(L, 2);
        float dur = (float)luaL_checknumber(L, 3);
        AudioBackend::instance().fade(h->id, to, dur);
        return 0;
    });
    lua_setfield(L, -2, "audio_fade");

    // backend.audio_is_playing(handle) -> bool
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::AUDIO);
        if (!h) { lua_pushboolean(L, 0); return 1; }
        lua_pushboolean(L, AudioBackend::instance().isPlaying(h->id) ? 1 : 0);
        return 1;
    });
    lua_setfield(L, -2, "audio_is_playing");
}

// --- 图层 API 注册示例 ---

void BackendAPI::registerLayerAPI(lua_State* L) {
    // backend.layer_set_texture(layer_id, path)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        int layerId = (int)luaL_checkinteger(L, 1);
        const char* path = luaL_checkstring(L, 2);
        bgfx::TextureHandle tex = TextureManager::instance().loadTexture(path);
        LayerManager::instance().setTex(
            static_cast<LayerManager::LayerType>(layerId), tex);
        return 0;
    });
    lua_setfield(L, -2, "layer_set_texture");

    // backend.layer_set_visible(layer_id, bool)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        int id = (int)luaL_checkinteger(L, 1);
        bool vis = lua_toboolean(L, 2);
        LayerManager::instance().get(
            static_cast<LayerManager::LayerType>(id)).visible = vis;
        return 0;
    });
    lua_setfield(L, -2, "layer_set_visible");

    // backend.layer_set_pos(layer_id, x, y)
    // backend.layer_clear(layer_id)
    // backend.layer_set_blend(layer_id, mode)
    // ...
}

// --- 统一句柄工厂方法 ---

ResourceHandle* BackendAPI::pushHandle(lua_State* L, HandleType type,
                                        uint32_t id, void* raw) {
    ResourceHandle* h = (ResourceHandle*)lua_newuserdata(L, sizeof(ResourceHandle));
    h->type = type;
    h->id = id;
    h->generation = 0;  // 初始 generation = 0
    h->rawHandle = raw;

    // 设置 metatable (支持 __gc, __close, __tostring)
    luaL_getmetatable(L, "caesura.ResourceHandle");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_newmetatable(L, "caesura.ResourceHandle");

        // __gc: 自动释放
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
            if (h && h->id != 0) {
                LOG_DEBUG("Backend", "GC handle type=%d id=%d", (int)h->type, h->id);
                h->id = 0;
            }
            return 0;
        });
        lua_setfield(L, -2, "__gc");

        // __close (Lua 5.4): 支持 local h <close> = backend.xxx()
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
            if (h && h->id != 0) {
                // 触发 destroy
                lua_getglobal(L, "backend");
                lua_getfield(L, -1, "destroy");
                lua_pushvalue(L, 1);
                lua_call(L, 1, 0);
                lua_pop(L, 1);
            }
            return 0;
        });
        lua_setfield(L, -2, "__close");

        // __tostring
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
            lua_pushfstring(L, "ResourceHandle{type=%d, id=%d, gen=%d}",
                           (int)h->type, h->id, h->generation);
            return 1;
        });
        lua_setfield(L, -2, "__tostring");
    }
    lua_setmetatable(L, -2);
    return h;
}

ResourceHandle* BackendAPI::checkHandle(lua_State* L, int index, HandleType expected) {
    ResourceHandle* h = (ResourceHandle*)luaL_checkudata(L, index, "caesura.ResourceHandle");
    if (!h || h->id == 0) {
        luaL_error(L, "Invalid or destroyed handle");
        return nullptr;
    }
    if (h->type != expected) {
        luaL_error(L, "Handle type mismatch: expected %d, got %d",
                  (int)expected, (int)h->type);
        return nullptr;
    }
    return h;
}

bool BackendAPI::isValidHandle(lua_State* L, int index) {
    if (!lua_isuserdata(L, index)) return false;
    ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, index);
    return h && h->id != 0;
}

// ... 其余子系统注册函数 (Font/Input/Video/Viewport/System/Debug) ...

} // namespace caesura
```

---

### Task 0.5.2: backend.lua — Lua 端统一门面

**Files:** Create: `scripts/backend.lua`

```lua
-- scripts/backend.lua
-- 统一后端抽象层 — Lua 代码唯一与 C++ 交互的入口
-- 架构约束: 任何 Lua 代码不得直接调用 C 函数或绕过此模块

local backend = {}

-- ══════════════════════════════════════════════════════
-- 纹理 API
-- ══════════════════════════════════════════════════════

--- 加载纹理 (同步)
--- @param path string 纹理文件路径 (通过 Provider Chain 解析)
--- @return TextureHandle|nil
function backend.load_texture(path)
    -- 直接透传 C++ 注册的函数 (已由 BackendAPI::registerTextureAPI 注入)
    return _G.backend.texture_load(path)
end

--- 创建纯色纹理
--- @param r,g,b,a number 0-255
--- @return TextureHandle
function backend.create_solid_texture(r, g, b, a)
    return _G.backend.texture_create_solid(r, g, b, a or 255)
end

--- 获取纹理尺寸
--- @param handle TextureHandle
--- @return number width, number height
function backend.texture_get_size(handle)
    return _G.backend.texture_get_size(handle)
end

-- ══════════════════════════════════════════════════════
-- 音频 API (全局 Direct 模式)
-- ══════════════════════════════════════════════════════

--- 播放音频
--- @param path string  音频文件路径
--- @param bus number   0=BGM, 1=VOICE, 2=SE
--- @param volume number 0.0-1.0
--- @return AudioHandle|nil
function backend.audio_play(path, bus, volume)
    return _G.backend.audio_play(path, bus, volume or 1.0)
end

--- 停止音频
--- @param handle AudioHandle
function backend.audio_stop(handle)
    _G.backend.audio_stop(handle)
end

--- 淡入淡出
--- @param handle AudioHandle
--- @param to number 目标音量
--- @param duration number 淡变时长 (ms)
function backend.audio_fade(handle, to, duration)
    _G.backend.audio_fade(handle, to, duration)
end

--- 检查是否正在播放
--- @param handle AudioHandle
--- @return boolean
function backend.audio_is_playing(handle)
    return _G.backend.audio_is_playing(handle)
end

-- ══════════════════════════════════════════════════════
-- 图层 API
-- ══════════════════════════════════════════════════════

local LAYER = { bg = 0, fg = 1, message = 2 }

--- 设置图层纹理
--- @param layer string "bg"|"fg"|"message"
--- @param path string 纹理路径
function backend.layer_set_texture(layer, path)
    local id = LAYER[layer]
    if not id then error("Unknown layer: " .. tostring(layer)) end
    _G.backend.layer_set_texture(id, path)
end

--- 设置图层可见性
function backend.layer_set_visible(layer, visible)
    local id = LAYER[layer]
    if id then _G.backend.layer_set_visible(id, visible) end
end

--- 设置图层位置
function backend.layer_set_pos(layer, x, y)
    local id = LAYER[layer]
    if id then _G.backend.layer_set_pos(id, x, y) end
end

--- 清除图层
function backend.layer_clear(layer)
    local id = LAYER[layer]
    if id then _G.backend.layer_clear(id) end
end

-- ══════════════════════════════════════════════════════
-- 字体/文本 API
-- ══════════════════════════════════════════════════════

--- 渲染文本到消息层
--- @param text string
--- @param size number 字号
--- @param x,y number 屏幕坐标
--- @param maxWidth number 最大宽度 (自动换行)
function backend.font_render_text(text, size, x, y, maxWidth)
    _G.backend.font_render_text(text, size or 48, x or 40, y or 500, maxWidth or 720)
end

--- 清除消息层文本
function backend.font_clear()
    _G.backend.font_clear()
end

--- 换行
function backend.font_newline()
    _G.backend.font_newline()
end

-- ══════════════════════════════════════════════════════
-- 输入 API
-- ══════════════════════════════════════════════════════

--- 检查是否有待处理的点击
--- @return boolean
function backend.input_has_click()
    return _G.backend.input_has_click()
end

--- 消费一个点击事件
function backend.input_consume_click()
    _G.backend.input_consume_click()
end

-- ══════════════════════════════════════════════════════
-- 视频 API
-- ══════════════════════════════════════════════════════

--- 加载并播放视频
--- @param path string 视频文件路径
--- @return VideoHandle|nil
function backend.video_play(path)
    return _G.backend.video_play(path)
end

--- 停止视频
function backend.video_stop(handle)
    _G.backend.video_stop(handle)
end

-- ══════════════════════════════════════════════════════
-- 系统 API
-- ══════════════════════════════════════════════════════

--- 存档
function backend.system_save(slot, jsonData)
    return _G.backend.system_save(slot, jsonData)
end

--- 读档
function backend.system_load(slot)
    return _G.backend.system_load(slot)
end

--- 获取引擎版本
function backend.system_version()
    return _G.backend.system_version()
end

-- ══════════════════════════════════════════════════════
-- 句柄管理 (统一)
-- ══════════════════════════════════════════════════════

--- 检查句柄是否有效
--- @param handle any
--- @return boolean
function backend.is_valid(handle)
    return _G.backend.is_valid(handle)
end

--- 销毁句柄 (释放底层资源)
--- @param handle any
function backend.destroy(handle)
    _G.backend.destroy(handle)
end

return backend
```

---

### 工厂模式调用示例

```lua
-- KAG 命令层调用示例 (scripts/kag/commands/layer.lua 中的 bg 命令)
local backend = require("scripts.backend")  -- 唯一入口

function kag_commands.bg(ctx, params)
    -- 工厂方法: 加载纹理, 返回统一句柄
    local tex_handle = backend.load_texture(params.storage)
    if not tex_handle then
        log("Texture not found: " .. params.storage)
        return
    end

    -- 设置到背景图层
    backend.layer_set_texture("bg", params.storage)
    ctx.layers.bg = tex_handle  -- 存储句柄供后续操作
end

-- 音频命令示例
function kag_commands.playvoice(ctx, params)
    local audio_handle = backend.audio_play(params.storage, 1, 1.0)
    if not audio_handle then return end

    -- 使用 CancelToken 管理生命周期
    local ct = kag.cancel_token.new()
    ct:register(function()
        backend.audio_stop(audio_handle)   -- 取消时停止
        backend.destroy(audio_handle)       -- 释放资源
    end)

    -- Lua 5.4 <close> 自动清理
    local h <close> = audio_handle

    table.insert(ctx.active_operations, ct)
    coroutine.yield()  -- 等待语音结束
end
```

---

### 统一抽象层的设计收益

| 收益 | 说明 |
|------|------|
| **单一切入点** | 所有 Lua 代码只 `require("scripts.backend")`，IDE 可以静态分析所有 C++ 调用 |
| **类型安全** | 工厂方法返回统一 ResourceHandle，含类型标记 + generation 计数 |
| **AI 友好** | AI Agent 只需了解 backend 模块的 API 签名，无需理解 bgfx/SoLoud 内部 |
| **热重载安全** | handle.is_valid() 双重校验 (id + generation)，热重载后自动失效 |
| **可测试性** | backend.lua 可被 mock 替换，支持 Lua 单元测试无需启动引擎 |
| **IDE 集成** | backend API 清单可直接生成 IDE 的自动补全 + 文档提示 |
| **向后兼容** | 新增 C++ 能力只需在 BackendAPI 中注册，Lua 侧添加 wrapper |

---

### 后续 Phase 如何使用此抽象层

```
Phase 1 (脚本引擎): scheduler.lua 中的 kag 命令调用 backend.xxx()
Phase 2 (渲染管线): layers.lua 调用 backend.layer_set_texture/backend.load_texture
Phase 3 (音频系统): audio.lua 调用 backend.audio_play/backend.audio_fade
Phase 4 (KAG标签): 每个标签实现只 import backend + kag 内部模块
Phase 5 (高级渲染): palette.lua/transition.lua 调用 backend 新注册的着色器/转场 API
Phase 6 (存档): save.lua 调用 backend.system_save/backend.system_load
Phase 7 (CARC): 对 Lua 层完全透明 — Provider Chain 在 C++ 侧自动选择来源
Phase 8 (热重载): backend.is_valid(handle) 检查所有旧句柄是否失效
Phase 9 (沙箱): [eval] 内仍然通过 backend 调用 (Render 白名单 = backend 子集)
```

