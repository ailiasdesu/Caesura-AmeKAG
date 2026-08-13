# E-mote 替代方案设计 — 骨骼网格动画子系统（Battle 4d / P2-9）

> 2026-08-12 · 设计文档。E-mote（Live2D 同类商业中间件）不可移植，需
> 自研等价物。本设计定义 **Caesura Skeletal Mesh Animation（SMA）**：
> 轻量骨骼 + 网格变形动画子系统，作为精灵家族（刚性）与 Live2D
> （Cubism 全功能）之间的中间层。
> 状态：**设计定稿，实现待排期**（标注为后续迭代项）。

## 1. 定位与范围

| 层级 | 现有能力 | SMA 填补 |
|---|---|---|
| 刚性 | `[sprite_move/scale/fade/swap]` + AffineBlt | — |
| **骨骼网格** | **无** | **本设计：骨骼驱动顶点变形** |
| 全功能 | Live2D（Cubism 5，D3D11 已验证） | — |

**目标场景**：低成本角色动画——头发/裙摆摆动、表情牵动、呼吸起伏，
不需要 Live2D 的整套参数系统。数据用 JSON 声明（对标 Live2D 模型
文件但极简），渲染走引擎自身网格路径。

**非目标**：物理模拟、IK、表情参数系统、Live2D 兼容导入。

## 2. 架构

```
scripts/kag/sma.lua            ← 数据加载/动画驱动（纯 Lua）
src/render/...MeshRender       ← 网格渲染原语（C++，IRenderDevice 扩展）
├── SMAMesh: 顶点数组 + 索引 + 骨骼权重
├── SMAAnimation: 骨骼关键帧（时间轴）
└── SMARenderer: CPU 软变形 → 顶点缓冲 → bgfx 绘制
```

### 2.1 模块边界（AGENTS.md 合规）

- `render` 模块新增 `IMeshRenderer`（`src/render/api/IMeshRenderer.h`）：
  纯虚接口，无数据成员
- 网格数据模型（SMA 顶点/骨骼/动画）放接口头（按值传参的类型）
- `script` 绑定层经 `BackendRegistry` 访问（不直接 include 实现）
- 数据加载（JSON 解析）在 `scripts/kag/sma.lua`（复用存档 JSON 技能）
  ——**渲染热路径零 Lua**（上传顶点缓冲后由 C++ 驱动）

### 2.2 数据格式（JSON，assets/sma/<name>.json）

```json
{
  "texture": "chara/hero_body.png",
  "atlas": { "w": 512, "h": 512 },
  "bones": [
    { "id": 0, "parent": -1, "pivot": [0.5, 0.9] },
    { "id": 1, "parent": 0,  "pivot": [0.5, 0.5] }
  ],
  "mesh": {
    "positions": [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]],
    "uvs":      [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]],
    "indices":  [0, 1, 2, 0, 2, 3],
    "weights":  [
      { "bone": 0, "w": 1.0 },
      { "bone": 1, "w": 1.0 }
    ]
  },
  "animations": {
    "idle": {
      "duration": 2.0,
      "tracks": [
        { "bone": 0, "frames": [
            { "t": 0.0, "rot": 0.0, "scale": 1.0, "offset": [0, 0] },
            { "t": 1.0, "rot": 0.1, "scale": 1.0, "offset": [0, 0] } ] }
      ]
    }
  }
}
```

设计要点：
- **线性骨骼链**（parent 树），每骨骼一个 2D 枢轴点 + 旋转/缩放/平移
- 顶点权重：每顶点最多 2 根骨骼（`{bone, w}` 列表，w 归一化）
- 动画 = 时间轴关键帧（rot/scale/offset），线性插值（LERP）
- 纹理来自现有纹理管线（`ITextureManager`，预算/LRU 复用）

## 3. 渲染路径

### 3.1 CPU 软变形（首版，推荐）

```
每帧（C++）:
  1. 按骨骼层级计算世界变换（父→子，矩阵链）
  2. 每顶点按权重混合骨骼变换 → 新位置
  3. 上传顶点缓冲（bgfx::update）→ 绘制
```

- 顶点数目标 <4k/角色（2D VN 规模绰绰有余）
- CPU 变形 4k 顶点 <0.1ms（现代桌面），移动端可接受
- **零新 shader**：用现有 AffineBlt 程序（UV 不变，只变位置）
- 无需 bgfx 新后端能力

### 3.2 GPU 骨骼蒙皮（后续，可选）

骨骼矩阵 uniform 数组 + 蒙皮 shader（`boneMatrices[N]`，顶点 shader
`pos = Σ w_i * boneMat_i * pos`）。仅在 CPU 路径成为瓶颈时引入
（>10k 顶点或大量同屏角色）。

## 4. IRenderDevice 扩展（接口草案）

```cpp
// src/render/api/IMeshRenderer.h（新增，render 模块）
class IMeshRenderer {
public:
    virtual ~IMeshRenderer() = default;

    // 上传网格（顶点/索引/权重），返回句柄
    virtual MeshHandle createMesh(const SMAMesh& mesh) = 0;
    virtual void destroyMesh(MeshHandle h) = 0;

    // 上传动画骨骼姿态（CPU 软变形在引擎侧完成）
    virtual void updateMesh(MeshHandle h,
                            const std::vector<BonePose>& poses) = 0;

    // 绘制到视图（targetView 与图层系统一致）
    virtual void drawMesh(uint16_t targetView, MeshHandle h,
                          uint32_t dstTexId, float x, float y,
                          float scale, float opacity) = 0;
};
```

- `SMAMesh`/`BonePose` 为接口头内 POD 结构（符合 §2.1）
- `BackendRegistry` 增加 `setMeshRenderer/getMeshRenderer`
- 引擎组合根（`src/entry/Engine.cpp`）创建实现

## 5. KAG 命令集

```kag
; 加载模型 + 纹理（阻塞：等待资源就绪）
[sma_load name="hero" model="sma/hero_body.json"]

; 播放动画（loop 可选，阻塞等待完成或非阻塞）
[sma_play name="hero" anim="idle" loop=true]

; 设置骨骼姿态（直接控制，如表情）
[sma_bone name="hero" bone=1 rot=0.3 scale=1.0]

; 放置/变换角色
[sma_move name="hero" x=0.5 y=0.3 scale=1.0 time=500]

; 卸载
[sma_unload name="hero"]
```

契约（schema.define，与 78 命令同规范）：
- `sma_load`：`name`（required）、`model`（file 类型，路径交叉验证）
- `sma_play`：`name`、`anim`（required）、`loop`（boolean，default true）
- `sma_bone`：`name`、`bone`（number）、`rot`（number，clamp ±π）、
  `scale`（number，min 0.1 max 4）、`time`（number，default 0）
- `sma_move`：`name`、`x`/`y`（0..1 归一化）、`scale`、`time`
- 未知参数警告、范围钳制——与现有契约体系一致

## 6. 与 Live2D / 精灵家族分层

```
[sprite_move]  → AffineBlt 刚性     （已有，零成本）
[sma_*]        → 骨骼网格变形        （本设计，CPU 软变形）
[live2d]       → Cubism 全功能      （已有，D3D11 验证）
```

- 同一角色可用三层混合（刚性整体移动 + SMA 局部动画 + Live2D 表情）
- 渲染排序：`sma_*` 输出到与 sprite 相同的图层系统（layer z 序）

## 7. 性能目标与验证

| 指标 | 目标 |
|---|---|
| 4k 顶点 CPU 软变形 | <0.1ms/角色（桌面）/ <0.5ms（移动） |
| 骨骼链深度 | ≤8（父→子线性链） |
| 同屏 SMA 角色 | ≥16 @ 60fps（桌面） |
| 动画 LERP | 每帧 <0.01ms（<32 骨骼） |

验证：`test_sma.lua`（确定性：骨骼变换矩阵数学、权重混合、LERP、
JSON 加载）；渲染测试走 deferred-gpu 模式（无 GPU 环境仅构造+访问器，
见 `docs/solutions/deferred-gpu-tests.md`）。

## 8. 实现排期（后续迭代）

| 步骤 | 内容 | 依赖 |
|---|---|---|
| S1 | `IMeshRenderer` 接口 + `SMAMesh`/`BonePose` POD + BackendRegistry 接入 | 本设计 |
| S2 | CPU 软变形实现 + 顶点缓冲上传 + 绘制（复用 AffineBlt shader） | S1 |
| S3 | `scripts/kag/sma.lua` 数据加载/动画驱动 + KAG 命令契约 | S1 |
| S4 | 测试（确定性矩阵/权重/LERP + deferred-gpu 渲染） | S2/S3 |
| S5 | GPU 蒙皮（bgfx compute）——**已交付（round 18）**，见 §10 | S2 |

## 9. S5 GPU 蒙皮（bgfx compute，round 18）

**管线**：每个网格在 `createMesh` 时上传**静态 compute 输入顶点缓冲**（
pos+uv+bone0/bone1+w0/w1，32B/顶点；D3D11 禁止 DYNAMIC 用法带 SRV 绑定，
故输入必须是静态）与**动态输出缓冲**（pos+uv，16B/顶点，COMPUTE_WRITE）；
共享骨骼缓冲（64 骨骼 × vec4 + 槽 64/65 携带**绘制变换** (x,y,scale) 与视口
尺寸 (sw,sh)）。`updateMesh` 只存姿势；`drawMesh` 时打包骨骼 → 上传 →
**同 view dispatch**（compute 排序先于 draw，提交序保证）→ 计算着色器完成
蒙皮 + NDC 变换 → 绘制复用引擎已验证的直通程序（vs_sprite + fs_texture）。

**关键实现细节**：
- D3D11 compute 缓冲是 **typed float4 视图**（非 structured）——HLSL 用
  `Buffer<float4>`/（输出）`RWBuffer<float4> : register(u2)`（**寄存器必须
  与绑定 stage 一致**；GLSL 用 std430 binding=0/1/2）。
- 骨骼打包 `packBonePose`：vec4=(cos·scale, sin·scale, ox, oy)，与 CPU
  `applyBonePose` 严格等价（shader 数学复刻单测 <1e-4）。
- 内存生命周期：`bgfx::update`/create 的 makeRef 内存必须存活到
  `bgfx::frame()`——一律用 `bgfx::copy` 移交所有权。
- 回退：无 BGFX_CAPS_COMPUTE / Metal / SPIR-V → CPU 软变形（SkinMode::Auto）。
- **验证**：D3D11 GPU 子测试（隐藏窗 + framebuffer 读回）——同一网格同一
  姿势 GPU/CPU 两帧逐像素比对（容差 ±1 + 边缘预算）通过（14 断言）。

## 9b. 播放控制与高级动画（round 18）

- **播放控制**：`loop`（设计文档 §4 承诺，默认 true，t 按 duration 取模）、
  `sma.duration`、`rate` 倍速、`pause`/`resume`/`seek`/`set_rate`、
  `play_anim`（切换动画不重建网格）、非 loop 播完 `on_done_anim` 回退。
- **crossfade 混合**：`play_anim(blend_time)` 双动画采样逐骨 LERP，完成后清除。
- **2 骨 IK**：cosine-law 纯函数 `ik2bones`（可达/不可达/退化全分支），
  `set_ik`/`clear_ik` 每帧覆盖链骨骼世界旋转使链条到达目标。
- **部件/表情变体**（E-mote 风格）：资产 `parts` 多网格（每部件独立
  变体几何），`set_variant` 销毁重建该部件网格；单 mesh 路径完全兼容。

## 10. 风险

| 风险 | 对策 |
|---|---|
| 软变形 CPU 成本随角色数线性增长 | 顶点预算 + 同屏上限；GPU 蒙皮为后备 |
| 数据格式与 Live2D 生态不兼容 | 定位为"轻量替代"，非 Live2D 导入器 |
| 网格渲染与图层系统集成复杂度 | 复用 sprite 图层路径（z 序/透明度） |
| 无美术工具产出 SMA JSON | 提供手写 JSON 模板 + 后续编辑器可视化（Battle 4b 场景树扩展） |
