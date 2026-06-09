# Sprint Plan — MiniGame 3D + Electron 编辑器

> 更新: 2026-06-09

## MiniGame 3D (✅ 完成)

| API | 状态 |
|-----|:---:|
| `spawn_cube(x,y,z,size?,r?,g?,b?,mat?)` | ✅ |
| `spawn_sphere(x,y,z,radius?,r?,g?,b?,mat?)` | ✅ |
| `spawn_plane(x,y,z,w?,h?,r?,g?,b?,mat?)` | ✅ |
| `remove_object(id)` | ✅ |
| `set_camera(eyeX,eyeY,eyeZ, atX,atY,atZ)` | ✅ |
| `create_material(r,g,b,roughness,metallic,specular,name?)` | ✅ |
| `set_material(objId, matId)` | ✅ |
| `set_ambient(r,g,b)` | ✅ |
| `set_directional(dirX,dirY,dirZ, r?,g?,b?,intensity?)` | ✅ |
| `add_point_light(x,y,z, r?,g?,b?,intensity?,range?,name?)` | ✅ |
| `remove_light(id)` | ✅ |
| `check_collision(objA, objB)` | ✅ |
| `set_collision(bool)` | ✅ |
| `set_velocity(objId, vx,vy,vz)` | ✅ |
| `set_gravity(objId, bool)` | ✅ |

渲染: PBR-lite (roughness, metallic, specular) + 3 光源类型 + 碰撞 + 物理
着色器: debug wireframe 回退（待编译真实 shader）

## Electron 编辑器 (✅ 完成)

| 组件 | 状态 |
|------|:---:|
| StageView (Canvas 2D + 拖入素材) | ✅ |
| AssetPanel (素材浏览 + 拖拽源) | ✅ |
| Timeline (事件可视化 + 时间轴) | ✅ |
| PropertyPanel (属性编辑 + 代码同步) | ✅ |
| AIPanel (@generate/@fix + 多后端) | ✅ |
| SettingsDialog (AI 后端设置) | ✅ |
| RPC 集成 (7 个方法) | ✅ |
| AI Providers (OpenAI/Codex/Custom) | ✅ |
| 打包配置 (electron-builder) | ✅ |

## 待完成

- [ ] MiniGame shader 编译（替换 wireframe）
- [ ] FFmpeg 默认启用
- [ ] Demo 场景
- [ ] RPC 帧预览（编辑器截图回传）
- [ ] 用户文档
## Related Knowledge

- docs/solutions/best-practices/alpha-to-beta-sprint.md — 从本 Sprint 提炼的 Alpha→Beta 升级模式