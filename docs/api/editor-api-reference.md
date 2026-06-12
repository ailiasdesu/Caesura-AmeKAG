# Caesura (AmeKAG) — Editor Developer API Reference

> **面向 web-editor 前端开发者的完整接口文档**
> 最后更新: 2026-06-12

---

## 文档地图

编辑器前端通过三层接口与引擎通信：

| 层 | 协议 | 用途 | 详细文档 |
|---|------|------|---------|
| **RPC** | HTTP (localhost:9876) | 编辑器控制：状态查询、资源列表、脚本执行、打包 | [§1 HTTP RPC 端点](#1-http-rpc-api) |
| **Lua 绑定** | Lua C API | KAG 脚本调用的引擎能力（音频、渲染、存档等） | [§2 Lua 绑定模块](#2-lua-binding-modules) |
| **KAG 脚本** | `.ks` 文本 | galgame 场景描述语言 | [§3 KAG 命令参考](#3-kag-command-reference) |
| **C++ 接口** | C++ 虚函数 | 引擎内部架构（供引擎开发者参考） | [cpp-interfaces.md](cpp-interfaces.md) |

---

## 1. HTTP RPC API

**Base URL**: `http://localhost:9876`
**Content-Type**: `application/json`
**CORS**: 已启用 (`Access-Control-Allow-Origin: *`)

### 1.1 健康检查

**`GET /api/ping`**

```
→ (no body)
← {"status":"ok","engine":"CaesuraAmeKAG"}
```

---

### 1.2 引擎状态

**`GET /api/status`**

```
→ (no body)
← {"status":"ok","engine":"CaesuraAmeKAG","lua":true,"port":9876}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `status` | string | `"ok"` |
| `engine` | string | `"CaesuraAmeKAG"` |
| `lua` | bool | Lua VM 是否已初始化 |
| `port` | int | 服务器端口 |

---

### 1.3 资源列表

**`GET /api/assets?type={image|audio|script}`**

`type` 参数可选，省略则返回全部。

```
→ (no body)
← [
    {"path":"assets/bg/scene01.png","name":"scene01.png","type":"image"},
    {"path":"assets/bgm/theme.ogg","name":"theme.ogg","type":"audio"},
    {"path":"assets/scripts/chapter1.ks","name":"chapter1.ks","type":"script"}
  ]
```

扫描的目录映射：

| type | 扫描目录 |
|------|---------|
| `image` | `assets/bg/`, `assets/char/`, `assets/ui/` |
| `audio` | `assets/bgm/`, `assets/voice/`, `assets/se/` |
| `script` | `assets/scripts/` |

---

### 1.4 执行 Lua 脚本

**`POST /api/run`**

将脚本内容写入临时文件并执行。

```
→ "playbgm('theme.ogg')\nbg('scene01.png')\nch('Hero', 'Hello world!')\np()"
← {"status":"ok"}
```

错误响应 (4xx/5xx)：
```
← {"error":"Lua not initialized"}
← {"error":"Empty script"}
← {"status":"error","message":"<lua error message>"}
```

---

### 1.5 停止执行

**`POST /api/stop`**

设置全局标志 `_CAESURA_QUIT = true`，脚本在下一个 yield 点退出。

```
→ (no body)
← {"status":"ok"}
```

---

### 1.6 查看日志

**`GET /api/logs`**

返回最近 500 条日志。

```
→ (no body)
← [
    {"level":"info","message":"Running scene script...","time":"14:32:05"},
    {"level":"error","message":"playbgm: file not found","time":"14:32:06"}
  ]
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `level` | string | `"info"` / `"warn"` / `"error"` |
| `message` | string | 日志内容 |
| `time` | string | `HH:MM:SS` 格式时间戳 |

---

### 1.7 Live2D 模型列表

**`GET /api/live2d/models`**

扫描 `models/`, `assets/models/`, `assets/live2d/` 目录中的模型文件。

```
→ (no body)
← [
    {"name":"haru.moc","path":"models/haru.moc"},
    {"name":"shizuku.model.json","path":"assets/models/shizuku.model.json"}
  ]
```

文件过滤：`.moc`, `.json`, 或文件名含 `.model`。

---

### 1.8 加载 Live2D 模型

**`POST /api/live2d/load`**

```
→ {"modelPath":"models/haru.moc"}
← {"status":"ok","modelId":1,"name":"models/haru.moc"}
```

错误响应：
```
← {"error":"modelPath required"}
← {"error":"Animation backend not available"}
← {"error":"Failed to load model"}
```

---

### 1.9 一键打包 (CARC)

**`POST /api/build`**

将 `scripts/` 和 `assets/` 下所有文件打包为加密 `.carc` 归档。

```
→ (可选) {"outputPath":"build/game.carc","keyPath":"build/game.key"}
← {"status":"ok","path":"build/game.carc","size":1048576,"files":42}
```

| 请求参数 | 必填 | 默认值 | 说明 |
|---------|------|--------|------|
| `outputPath` | 否 | `build/game.carc` | 输出归档路径 |
| `keyPath` | 否 | `build/game.key` | Ed25519 密钥基础路径 |

| 响应字段 | 说明 |
|---------|------|
| `status` | `"ok"` |
| `path` | 生成的 .carc 文件路径 |
| `size` | 文件大小（字节） |
| `files` | 打包的文件数量 |

---

## 2. Lua Binding Modules

编辑器通过注入 Lua 脚本来驱动引擎。以下是所有可用的 Lua 绑定模块。

### 2.1 Render 模块

```lua
-- 全局变量: Render
-- 访问: BackendRegistry::instance().getRenderDevice()
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `load_texture` | `(path: string) → id: int` | 从文件加载纹理，返回纹理 ID（0=失败） |
| `destroy_texture` | `(id: int)` | 释放纹理 |
| `create_solid_texture` | `(r, g, b, a: int) → id: int` | 创建 1×1 纯色纹理 |
| `render_text` | `(text, x, y, scale, r, g, b, a)` | 渲染文字到消息图层 |
| `render_ruby` | `(text, ruby, x, y)` | 渲染带注音的文字 |
| `clear_text` | `()` | 清除消息图层文字 |
| `set_font` | `(face, size, color)` | 设置字体 |
| `line_height` | `() → number` | 获取当前行高 |
| `get_resolution` | `() → width, height` | 获取 backbuffer 分辨率 |
| `set_view_name` | `(viewId, name)` | 设置 bgfx 视图调试名称 |
| `submit_batch` | `(table)` | 批量提交绘制四边形（见下方） |
| `submit_blend` | `(baseTexId, blendTexId, mode, baseAlpha, blendAlpha, globalAlpha)` | 提交混合特效 |
| `submit_transition` | `(fromTexId, toTexId, ruleTexId, method, progress)` | 提交转场特效 |
| `submit_vfx` | `(srcTexId, effect, fadeAlpha, r, g, b, blur, quakeX, quakeY)` | 提交 VFX |
| `stretch_blt` | `(dstTexId, dx, dy, dw, dh, srcTexId, sx, sy, sw, sh, filter)` | 缩放 Blit |
| `affine_blt` | `(dstTexId, dx, dy, dw, dh, srcTexId, sx, sy, sw, sh, m0..m5)` | 仿射变换 Blit |
| `fill_viewport` | `(vpId, r, g, b, a)` | 纯色填充 RTT |
| `resize` | `(width, height)` | 通知引擎窗口大小变化 |
| `is_valid_handle` | `(type: int, id: int) → bool` | 验证资源句柄（type: 0=Texture,1=Sound,...） |
| `cancel_async_loads` | `()` | 取消所有异步加载 |
| **视频** | | |
| `video_play` | `(path) → handle` | 播放视频 |
| `video_stop` | `(handle)` | 停止视频 |
| `video_update` | `(handle) → bool` | 解码下一帧 |
| `video_get_texture` | `(handle) → texId` | 获取当前帧纹理 ID |
| `video_is_playing` | `(handle) → bool` | 是否播放中 |
| `video_has_ended` | `(handle) → bool` | 是否播放完毕 |
| `video_get_size` | `(handle) → width, height` | 视频尺寸 |
| `video_pause` | `(handle)` | 暂停 |
| `video_resume` | `(handle)` | 继续 |

**submit_batch 格式**：
```lua
Render.submit_batch({
    { tex = textureId, x = 0, y = 0, w = 1280, h = 720, opacity = 255, view = 1 },
    { tex = charTexId,  x = 400, y = 100, w = 480, h = 620, opacity = 255, view = 1 },
})
```

### 2.2 VFX 模块

```lua
-- 全局变量: VFX
-- 访问: BackendRegistry::instance().getParticleSystem()
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `particles_init` | `() → bool` | 初始化粒子系统 |
| `particles_shutdown` | `()` | 关闭粒子系统 |
| `particles_create_emitter` | `(cfg: table) → id: int` | 创建发射器，返回 -1=参数无效 |
| `particles_destroy_emitter` | `(id)` | 销毁发射器 |
| `particles_emit` | `(id, count)` | 发射 N 个粒子 |
| `particles_update` | `(dt)` | 更新粒子物理 |
| `particles_render` | `()` | 渲染所有粒子 |
| `particles_alive_count` | `() → int` | 活跃粒子数 |
| `particles_is_initialized` | `() → bool` | 系统是否已初始化 |
| `particles_clear` | `()` | 清除所有发射器和粒子 |

**Emitter 配置表**：
```lua
{
    x = 640, y = 360,          -- 发射位置
    rate = 10.0,               -- 发射速率（粒/秒，0=手动 emit）
    lifeMin = 0.5, lifeMax = 2.0,   -- 生命期范围（秒）
    speedMin = 10, speedMax = 50,    -- 速度范围
    angleMin = 0, angleMax = 6.283,  -- 角度范围（弧度）
    sizeMin = 2, sizeMax = 8,        -- 尺寸范围
    r = 1, g = 1, b = 1, a = 1,    -- 颜色
    gravityX = 0, gravityY = 0,     -- 重力
}
```

### 2.3 KAG 模块 (C++ 绑定)

```lua
-- 全局变量: KAG
-- 访问: BackendRegistry::instance() + C++ binding layer
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `play_bgm` | `(file, volume?, loop?) → bool` | 播放 BGM |
| `stop_bgm` | `(fadeTime?)` | 停止 BGM |
| `play_se` | `(file, volume?) → bool` | 播放音效 |
| `stop_se` | `()` | 停止所有音效 |
| `play_voice` | `(file, volume?) → bool` | 播放语音 |
| `stop_voice` | `()` | 停止语音 |
| `set_global_volume` | `(vol: 0.0–1.0)` | 设置主音量 |
| `get_global_volume` | `() → number` | 获取主音量 |
| `set_bus_volume` | `(bus, vol)` | 设置总线音量（"bgm"/"voice"/"se"） |
| `get_bus_volume` | `(bus) → number` | 获取总线音量 |
| `render_text` | `(text, x, y, scale, r, g, b, a)` | 渲染文字 |
| `clear_text` | `()` | 清除文字 |
| `line_height` | `() → number` | 行高 |
| `is_bgm_playing` | `() → bool` | BGM 是否播放中 |
| `is_voice_playing` | `() → bool` | 语音是否播放中 |
| `get_active_voices` | `() → int` | 活跃语音数 |
| `log` | `(message)` | 日志 |

### 2.4 Debug 模块

```lua
-- 全局变量: Debug
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `log` | `(level, message)` | 写入引擎日志 |
| `assert` | `(condition, message)` | 调试断言 |
| `traceback` | `() → string` | Lua 堆栈回溯 |
| `get_fps` | `() → number` | 当前帧率 |
| `get_memory` | `() → number` | Lua 内存使用 (KB) |
| `profile_start` | `(name)` | 开始性能采样 |
| `profile_end` | `(name)` | 结束性能采样 |

### 2.5 DevCore 模块

```lua
-- 全局变量: DevCore
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `get_version` | `() → string` | 引擎版本 |
| `get_backend_name` | `(subsystem) → string` | 后端名称（"render"/"audio"/"platform"） |
| `set_dev_mode` | `(bool)` | 开发模式开关 |
| `reload_scripts` | `()` | 热重载脚本 |

### 2.6 Save 模块

```lua
-- 全局变量: Save
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `save` | `(slot: int, data: table)` | 保存到槽位 |
| `load` | `(slot: int) → table` | 从槽位加载 |
| `list_saves` | `() → table` | 列出所有存档 |
| `delete_save` | `(slot: int)` | 删除存档 |

### 2.7 Steam 模块（条件编译）

```lua
-- 全局变量: Steam（仅当 CAESURA_HAS_STEAM 定义时可用）
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `is_available` | `() → bool` | Steam 是否可用 |
| `get_user_name` | `() → string` | Steam 昵称 |
| `unlock_achievement` | `(id)` | 解锁成就 |
| `set_rich_presence` | `(key, value)` | 设置 Rich Presence |

---

## 3. KAG Command Reference

KAG 脚本语法：`[command param="value"]`，写在 `.ks` 文件中。

### 3.1 音频命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `playbgm` | `storage` (路径), `volume` (0–1, 默认1), `loop` (bool) | 播放 BGM |
| `stopbgm` | `time` (毫秒, 淡出) | 停止 BGM |
| `fadebgm` | `volume` (目标音量), `time` (毫秒) | 淡入/淡出 BGM |
| `xfadebgm` | `storage`, `time` (毫秒) | 交叉淡入淡出新 BGM |
| `playse` | `storage`, `volume` | 播放音效 |
| `stopse` | — | 停止所有音效 |
| `playvoice` | `storage`, `volume` | 播放语音 |
| `stopvoice` | — | 停止语音 |
| `waitsound` | — | 等待当前 SE 播放完毕 |
| `waitbgm` | — | 等待 BGM 淡入淡出完毕 |
| `setbgmvolume` | `volume` | 设置 BGM 总线音量 |
| `setsevolume` | `volume` | 设置 SE 总线音量 |
| `setvoicevolume` | `volume` | 设置 Voice 总线音量 |

### 3.2 图层命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `bg` | `storage` (路径), `time` (毫秒) | 设置背景图 |
| `fg` | `storage` (路径), `layer` (层号), `clear` (bool) | 设置前景立绘 |
| `cl` | `layer` ("bg"/"fg"/"msg") | 清除指定图层 |
| `image` | `storage`, `layer`, `x`, `y`, `w`, `h` | 在指定位置放置图片 |
| `position` | `layer`, `x`, `y`, `scale` | 定位图层 |
| `layopt` | `layer`, `opacity` (0–1) | 设置图层渲染选项 |

### 3.3 文本命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `ch` | `name` (说话人), `text` | 角色对话 |
| `text` | `text` | 旁白/叙述 |
| `l` | — | 换行 |
| `r` | — | 回车 |
| `er` | — | 清除消息图层所有文字 |
| `p` | — | 分页/等待点击 |
| `ruby` | `text`, `ruby` | 带注音的文字 |
| `font` | `face`, `size`, `color` | 设置字体 |
| `skip` | — | 切换 Skip 模式 |
| `reset` | — | 重置文字状态 |
| `pt` | `speed` (毫秒/字) | 打字机速度 |
| `button` | `text`, `target` (`*label`) | 选项按钮 |
| `endbutton` | — | 确认选项集，等待选择 |

### 3.4 系统命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `wait` | `time` (毫秒) | 等待 N 毫秒 |
| `eval` | `exp` (Lua 表达式) | 执行 Lua，结果存入 ctx |
| `emb` | `exp` (Lua 代码) | 沙箱内执行 Lua |
| `history` | — | 打开 Backlog 界面 |

### 3.5 流程控制

| 命令 | 参数 | 说明 |
|------|------|------|
| `if` | `exp` (Lua 表达式) | 条件分支 |
| `else` | — | 否则分支 |
| `endif` | — | 结束条件块 |
| `jump` | `target` (`*label`) | 跳转到当前场景标签 |
| `call` | `target` (场景路径) | 调用子场景（可 return） |
| `return` | — | 从子场景返回 |
| `link` | `target` (场景路径) | 跨场景跳转 |
| `label` | (内联 `*name`) | 定义跳转目标 |
| `macro` | `name` | 定义宏 |
| `endmacro` | — | 结束宏定义 |
| `end` | — | 结束脚本 |

### 3.6 过渡效果

| 命令 | 参数 | 说明 |
|------|------|------|
| `trans` | `type` (效果名), `time` (毫秒) | 应用命名过渡效果 |
| `move` | `layer`, `x`, `y`, `time` | 动画移动图层 |
| `quake` | `time`, `amplitude` | 屏幕震动 |
| `fade` | `type` ("in"/"out"), `time` | 淡入/淡出 |

### 3.7 特效 / 视频 / 资源 / 存档

| 命令 | 参数 | 说明 |
|------|------|------|
| `vfx` | `type` (shake/flash/blur/sepia), `time`, ... | 视觉特效 |
| `video` | `storage`, `loop`, `volume` | 播放视频 |
| `stopvideo` | — | 停止视频 |
| `preload` | `storage`, `type` | 预载资源 |
| `get_texture` | `storage` | 按路径获取纹理句柄 |
| `is_loaded` | `storage` | 资源是否已载入 |
| `is_pending` | `storage` | 资源是否载入中 |
| `flush_cache` | — | 清空资源缓存 |
| `save` | `slot` (整数) | 存档 |
| `load` | `slot` (整数) | 读档 |
| `listsaves` | — | 列出所有存档 |

---

## 4. C++ 接口

引擎内部架构文档，供需要修改引擎核心的合作开发者参考。

→ [cpp-interfaces.md](cpp-interfaces.md) — 30 个 `I*` 纯虚接口，16 模块，BackendRegistry 完整 getter 列表。

---

## 附录 A: 快速开始 — 编辑器工作流

```
1. 启动引擎（含 --editor 标志）
   → 引擎启动 EditorServer，监听 http://localhost:9876

2. 浏览器打开 web-editor（静态文件由引擎托管）
   → GET /api/ping 确认连接

3. 浏览资源
   → GET /api/assets?type=image  列出所有图片
   → GET /api/assets?type=audio  列出所有音频
   → GET /api/assets?type=script 列出所有脚本

4. 编写 KAG 场景
   → POST /api/run 发送脚本内容，引擎即时执行预览

5. 调试
   → GET /api/logs 查看执行日志
   → POST /api/stop 停止预览

6. 发布
   → POST /api/build 一键打包为 .carc
```

## 附录 B: 通用约定

- **命名空间**: 所有 C++ 公共类型在 `Caesura::` 下（archive 在 `Caesura::carc::`）
- **Lua 全局变量**: 绑定模块注册为 Lua 全局变量（`KAG`, `Render`, `VFX`, `Debug`, `DevCore`, `Save`, `Steam`）
- **BackendRegistry**: 所有后端访问的唯一入口点（参见 [cpp-interfaces.md §A](cpp-interfaces.md#附录-a-backendregistry-完整-getter-列表)）
- **接口文件**: `src/<module>/api/I<Name>.h`，纯虚类，不包含实现
- **构建**: `cmake --build build --config Debug`
- **测试**: `build/tests/Debug/CaesuraTests.exe`（当前 324/324 passed）