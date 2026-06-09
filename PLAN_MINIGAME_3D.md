# MiniGame 3D 深化 — 实现计划

> 日期: 2026-06-09 | 引擎完成度: 87% → **93%** ✅
> 前置: nlohmann/json 集成 ✅, BgfxMiniGameBackend 基础框架 ✅
> 状态: **全部完成 ✅** (IU-1~7)

---

## 0. 现状分析

### 已有
- IMiniGameBackend 纯虚接口 — 生命周期/场景/渲染/输入/Lua 完整定义
- BgfxMiniGameBackend — bgfx view #10 独立渲染, cube 几何体, orbit camera
- NullMiniGameBackend — 无操作回退
- Lua API: mini_game.spawn_cube, 
emove_object, set_camera
- 嵌入 shader 基础设施: EmbeddedShaders.cpp (SPIR-V + DXBC)

### 缺失
- **无正式 3D shader** — 回退到 BGFX_DEBUG_WIREFRAME
- **无材质系统** — 颜色仅作 flat shading
- **无光照系统** — setAmbientLight 是空操作
- **无碰撞检测** — 物体可穿透
- **无比 cube 更复杂的几何体**
- **Shader 编译未接入 bgfx shaderc 管线**

---

## 1. 实现单元

### IU-1: 3D Shader 程序 (Phong 光照)

**范围**: 新增 3D vertex/fragment shader，替换 debug wireframe

**文件**:
- shaders/dx11/minigame_vs.hlsl — 新增: 3D 顶点着色器 (modelViewProj + worldNormal)
- shaders/dx11/minigame_fs.hlsl — 新增: Phong 片段着色器 (ambient + diffuse + specular)
- shaders/glsl/minigame_vs.sc — 新增: bgfx GLSL 变体
- shaders/glsl/minigame_fs.sc — 新增
- shaders/metal/minigame_vs.metal — 新增: Metal 变体
- shaders/metal/minigame_fs.metal — 新增
- shaders/compile_shaders.bat — 修改: 添加 minigame shader 编译
- src/Render/EmbeddedShaders_MiniGame.cpp — 新增: 嵌入编译后的 shader 二进制
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: init() 从 EmbededShaders 加载

**Shader 输入**:
- Vertex: Position (float3), Normal (float3)
- Uniform: u_modelViewProj (mat4), u_model (mat4), u_lightPos (vec4), u_lightColor (vec4), u_ambient (vec4), u_cameraPos (vec4)
- Fragment: u_albedo (vec4), u_roughness (float), u_metallic (float)

**Shader 输出**: 逐像素 Phong 光照 (ambient + diffuse N·L + Blinn-Phong specular)

**验收**: mini_game.spawn_cube() 不再显示 wireframe，而是一个有光照的彩色立方体。bgfx debug text 覆盖层消失。

---

### IU-2: 材质系统

**范围**: 将 MiniObject 的纯色替换为 PBR 材质参数

**文件**:
- src/MiniGame/MiniMaterial.h — 新增: 材质结构体
- src/MiniGame/BgfxMiniGameBackend.h — 修改: MiniObject 添加 material 引用
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: submitObject() 传递 material 参数

**MiniMaterial 结构**:
`cpp
struct MiniMaterial {
    uint32_t id;
    float albedoR = 1, albedoG = 1, albedoB = 1;
    float roughness = 0.5f;
    float metallic  = 0.0f;
    float emissiveR = 0, emissiveG = 0, emissiveB = 0;
};
`

**API 变更**:
- spawn_cube → 添加可选的 material params
- 新增 mini_game.create_material(r, g, b, rough, metal) → materialId
- 新增 mini_game.set_material(objId, matId)

**验收**: 不同 cube 可以有不同 roughness/metallic 外观

---

### IU-3: 几何体库

**范围**: 超出 cube 的基础几何体

**文件**:
- src/MiniGame/MiniGeometry.h — 新增: 几何体生成函数
- src/MiniGame/MiniGeometry.cpp — 新增: 实现
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: 按几何体类型创建 VB/IB

**首批几何体**:
1. Cube (已有) — 重构为 MiniGeometry::createCube()
2. Sphere (UV sphere, 16x16 细分) — 新增
3. Plane (带法线) — 新增
4. Quad (挡板/公告板, 面向相机) — 新增

**Lua API**:
- mini_game.spawn_sphere(x, y, z, radius, r, g, b) → objId
- mini_game.spawn_plane(x, y, z, w, h, r, g, b) → objId

**验收**: 场景中同时存在 cube、sphere、plane，各有正确几何体

---

### IU-4: 光照系统

**范围**: setAmbientLight 不再空操作，加入点光源/方向光

**文件**:
- src/MiniGame/MiniLight.h — 新增: 光源类型枚举 + 结构体
- src/MiniGame/BgfxMiniGameBackend.h — 修改: 存储光源列表
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: 
ender() 遍历光源提交

**光源类型**:
- Directional (方向光: dir + color + intensity)
- Point (点光源: pos + color + range + intensity)
- Ambient (环境光: color)

**最大光源数**: 4 (限制 uniform 开销，适合视觉小说小场景)

**Lua API**:
- mini_game.set_ambient(r, g, b) — 环境光
- mini_game.add_directional_light(dirX, dirY, dirZ, r, g, b, intensity) → lightId
- mini_game.add_point_light(x, y, z, range, r, g, b, intensity) → lightId
- mini_game.remove_light(lightId)

**验收**: 多光源场景，不同位置的 cube 受不同光源影响

---

### IU-5: AABB 碰撞检测

**范围**: 基础 AABB 碰撞检测 + 碰撞回调

**文件**:
- src/MiniGame/MiniCollision.h — 新增: AABB 结构体 + 碰撞函数
- src/MiniGame/MiniCollision.cpp — 新增: 实现
- src/MiniGame/BgfxMiniGameBackend.h — 修改: 添加碰撞对检测
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: update() 中执行碰撞检测

**AABB 计算**: 从 MiniObject 的 pos/scale 实时计算

**Lua 回调**: 
- 碰撞发生时调用 Lua: :mini_game("on_collision", objA, objB)
- mini_game.check_collision(objA, objB) → bool (手动查询)

**验收**: 移动两个 cube 相互靠近时触发 on_collision 回调

---

### IU-6: 简单物理运动

**范围**: 给物体添加速度/加速度，在 update() 中积分

**文件**:
- src/MiniGame/BgfxMiniGameBackend.h — 修改: MiniObject 添加 velocity/acceleration
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: update() 中 Euler 积分

**Lua API**:
- mini_game.set_velocity(objId, vx, vy, vz)
- mini_game.set_gravity(objId, enabled) — 预设重力 (-9.8 m/s²)

**注意**: 这不是完整物理引擎，只是满足"推方块、重力下落"等基础 galgame 交互场景。完整物理（Bullet/PhysX）不在当前范围。

**验收**: 脚本可以设置 cube 下落，碰撞地面 plane 时停止

---

### IU-7: Lua API 统一 + 文档

**范围**: 补齐所有 Lua API，写脚本示例

**文件**:
- src/MiniGame/BgfxMiniGameBackend.cpp — 修改: luaCall() 补齐所有分发
- scripts/demo_minigame.lua — 新增: 完整 mini-game demo 脚本
- ENGINE_ANALYSIS.md — 更新 MiniGame 模块描述
- CONCEPTS.md — 更新 MiniGame 架构说明

---

## 2. 依赖关系

`
IU-1 (Shader) ──→ IU-2 (Material) ──→ IU-3 (Geometry)
                                          ↓
IU-1 (Shader) ──→ IU-4 (Lighting)  ←─────┘
                                          ↓
              IU-5 (Collision) ←──────────┘
                   ↓
              IU-6 (Physics) ←── IU-5
                   ↓
              IU-7 (Docs)
`

**关键路径**: IU-1 → IU-2 → IU-5 → IU-6（7 步顺序）
**可并行**: IU-3 和 IU-4 可同时开发（互相不依赖）

---

## 3. 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| bgfx shaderc 工具链配置复杂 | 中 | 延迟 1-2h | 先用 HLSL 手写 + dxc 编译，回退到现有 compile_shaders.bat |
| Phong 光照在 bgfx view 叠加时与 2D 层冲突 | 低 | 视觉异常 | 已用独立 view #10，确认 view 顺序 |
| 碰撞检测在大场景性能不足 | 低 | 掉帧 | 视觉小说小场景 (<100 物体)，暴力 O(n²) 够用 |
| 多光源 uniform 超限 | 低 | 编译失败 | 限制 4 光源，足够 galgame 需求 |

---

## 4. 非目标 (明确不做)

- ❌ 完整物理引擎 (Bullet/PhysX) — 超出视觉小说需求
- ❌ glTF/OBJ 模型加载 — 交由共同开发者
- ❌ 骨骼动画 — 当前无需求
- ❌ 阴影贴图 — 显存开销大，galgame 不需要
- ❌ PBR IBL (基于图像的照明) — 留待后续
- ❌ 粒子系统 3D 集成 — ParticleSystem 已独立

---

## 5. 预估

| 维度 | 估值 |
|------|------|
| 实现单元 | 7 |
| 新增文件 | ~8 (.h/.cpp/.hlsl/.lua) |
| 修改文件 | ~5 |
| 新增 Lua API | ~15 函数 |
| 引擎完成度 | 87% → ~93% |
| C++ 代码增量 | ~600 行 |
| Shader 代码 | ~120 行 HLSL + GLSL + Metal 变体 |

---

## 6. 验证标准

每个 IU 完成后独立验证：

1. **IU-1**: 启动引擎 → 进入 mini-game → cube 有光照，无 wireframe
2. **IU-2**: 两个不同 roughness 的 cube 并排，视觉可区分
3. **IU-3**: spawn_sphere + spawn_plane 可见
4. **IU-4**: 添加点光源后 cube 单侧变亮
5. **IU-5**: Lua on_collision 回调被调用
6. **IU-6**: set_gravity(true) 后 cube 下落
7. **IU-7**: demo_minigame.lua 可独立运行
