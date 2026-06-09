# MiniGame 3D API Reference

> Caesura (AmeKAG) 3D 小游戏 API。15 个 Lua 函数，PBR-lite 渲染管线。

## 初始化

```lua
mini_game.init({ width = 1280, height = 720 })
```
初始化 3D 渲染上下文。需在场景开始时调用。

## 几何体

### spawn_cube
```lua
mini_game.spawn_cube({
    size = { 1, 1, 1 },        -- x, y, z 尺寸
    pos  = { 0, 0.5, 0 },      -- 世界坐标位置
    color = { 1, 0.8, 0.2 }    -- RGB 颜色 (可选)
})
```

### spawn_sphere
```lua
mini_game.spawn_sphere({
    pos    = { 0, 1, 0 },
    radius = 0.5,              -- 半径 (默认 0.5)
    color  = { 0.2, 0.6, 1 }
})
```

### spawn_plane
```lua
mini_game.spawn_plane({
    size = { 10, 10 },         -- 宽, 高
    pos  = { 0, 0, 0 },
    color = { 0.8, 0.8, 0.8 }
})
```

### spawn_floor
```lua
mini_game.spawn_floor({
    size = { 20, 0.5, 20 },    -- x, y, z (y 为厚度)
    pos  = { 0, -1, 0 },
    color = { 0.2, 0.25, 0.3 }
})
```

## 摄像机

```lua
mini_game.set_camera({
    pos    = { 0, 5, 15 },      -- 摄像机位置
    lookAt = { 0, 2, 0 },       -- 看向目标点
    fov    = 60                  -- 视场角 (默认 60)
})
```

## 材质 (PBR-lite)

```lua
mini_game.set_material({
    roughness = 0.4,             -- 粗糙度 0-1
    metallic  = 0.1,             -- 金属度 0-1
    specStr   = 0.6              -- 镜面强度 0-1
})
```
使用 Cook-Torrance BRDF 模型。

## 光照

```lua
mini_game.set_lighting({
    dir = { 0.5, -1.0, 0.3, 0.8 },  -- 方向光 (xyz=方向, w=强度)
    points = {                        -- 点光源 (最多3个)
        {
            pos       = { 5, 3, 0 },
            color     = { 1, 0.8, 0.4 },
            intensity = 2.0,
            range     = 20
        },
        {
            pos       = { -5, 2, 3 },
            color     = { 0.3, 0.6, 1 },
            intensity = 1.5,
            range     = 15
        }
    }
})
```

## 碰撞检测

```lua
local hit = mini_game.check_collision({ 3, 0.5, 3 }, 1.5)
if hit then
    -- 在 (3, 0.5, 3) 半径 1.5 范围内有物体
end
```

## 清理

```lua
mini_game.cleanup()
```
释放所有 3D 资源。离开 MiniGame 场景时必须调用。

## 平台支持

| 平台 | 渲染 | 状态 |
|------|------|:---:|
| Windows | D3D11 PBR-lite | ✅ |
| Linux | OpenGL (待 shaderc 编译) | ⚠️ |
| macOS | Metal (待 shaderc 编译) | ⚠️ |
