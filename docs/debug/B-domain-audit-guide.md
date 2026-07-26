# B 域排查执行指南

> 负责人：B（渲染） | 32 项 | 覆盖 D2+D3+D7+D8+D10 | 基于代码审查完成

## 前置条件

- 引擎可正常构建 (`cmake --build build --config Debug --parallel`)
- 测试全绿 (`cd build/tests/Debug && ./CaesuraTests.exe`)
- 飞书 Base 已打开: https://mcn95ia2oj1a.feishu.cn/base/SKH2bMea7aF0TlsykBkcRdM1nWH
- 了解 bgfx 渲染管线基本结构

## 各域当前状态

### D2 背景与图层 (7/8 通过)

| ID | 验证项 | 状态 | 说明 |
|----|--------|------|------|
| D2.1 | PNG/JPG纹理加载 | ✅ | TextureManager::loadTexture |
| D2.2 | 纹理正确显示 | ✅ | BgfxDraw::blitTexture |
| D2.3 | BG图层设置背景 | ✅ | LayerManager::setTexture(BG) |
| D2.4 | FG图层设置立绘 | ✅ | LayerManager::setTexture(FG) |
| D2.5 | z-order正确 | ✅ | BG/FG/MSG枚举顺序渲染 |
| D2.6 | 图层透明度渐变 | P2 | 待Lua层或transition实现 |
| D2.7 | 图层缩放定位 | ✅ | setPosition/setScale |
| D2.8 | dirty-rect优化 | ✅ | markDirty/updateDirtyRegions |

**待验证的运行时项（需要启动引擎观察）**：
- D2.2: 启动引擎 → demo → 观察背景和立绘是否正常显示（非纯色、非拉伸）
- D2.5: 同时显示BG+FG → 确认立绘在背景上方，无遮挡错误
- D2.7: 立绘缩放/位移后位置正确
- D2.8: 快速切换多个背景/立绘 → 无残留闪烁

### D3 文字渲染 (6/8 通过)

| ID | 验证项 | 状态 | 说明 |
|----|--------|------|------|
| D3.1 | 中文字符 | ✅ | FreeType + CJK (已修复) |
| D3.2 | 日文字符 | ✅ | FreeType 同字体 |
| D3.3 | 文字颜色自定义 | ✅ | renderText(r,g,b,a) |
| D3.4 | Ruby注音 | ✅ | renderRuby |
| D3.5 | 文字换行 | P2 | 待新功能开发 |
| D3.6 | 字体切换 | ✅ | FontId::Small/Large/TTF |
| D3.7 | 文字图层上方 | ✅ | VIEW_MAIN渲染顺序 (已修复) |
| D3.8 | 文字淡入淡出 | P2 | 待特效支持 |

**待验证的运行时项**：
- D3.1/D3.2: 运行demo → 观察中文和日文是否正确显示（非乱码）
- D3.3: 修改demo中文本颜色 → 确认颜色变化生效
- D3.4: 在demo中加入 [ruby text="漢字" ruby="かんじ"] → 确认注音显示
- D3.7: 背景+文字同时显示 → 确认文字在背景上方可见

### D7 转场与特效 (5/6 通过)

| ID | 验证项 | 状态 | 说明 |
|----|--------|------|------|
| D7.1 | crossfade | ✅ | Transition.crossfade |
| D7.2 | wipe各方向 | ✅ | LEFT/RIGHT/TOP/BOTTOM |
| D7.3 | rule-based | ✅ | Transition.rule + preload_rule |
| D7.4 | 转场时长 | ✅ | duration参数 |
| D7.5 | 转场可取消 | ✅ | Transition.cancel |
| D7.6 | VFX GPU特效 | P2 | 待验证GPU特效路径 |

**待验证的运行时项**：
- D7.1: 运行demo → 场景切换时观察crossfade效果
- D7.2: 修改demo加入 [trans method="wipe" dur="1000"] → 观察wipe效果
- D7.3: 准备一张灰度规则图 → [trans method="rule" rule="rule.png"] → 观察效果
- D7.6: 运行demo → [quake time="500"] → 观察屏幕震动

### D8 资源加载管线 (6/6 通过)

| ID | 验证项 | 状态 | 说明 |
|----|--------|------|------|
| D8.1 | 同步纹理加载 | ✅ | loadTexture |
| D8.2 | 异步纹理加载 | ✅ | AsyncLoader::enqueue/poll |
| D8.3 | 失败占位纹理 | ✅ | buildCheckerboardPlaceholder |
| D8.4 | 大纹理>4096px | ✅ | bimg无尺寸限制 |
| D8.5 | 纹理LRU驱逐 | ✅ | checkBudget + LRU |
| D8.6 | 多资源并发 | ✅ | AsyncLoader 16并发 |

**待验证的运行时项**：
- D8.1/D8.2: 加载多个纹理 → 确认无崩溃
- D8.3: 加载不存在的图片(try to load "nonexistent.png") → 确认显示占位纹理
- D8.4: 准备一张大尺寸图片(>4K) → 确认加载正常

### D10 Live2D/立绘 (3/4 通过)

| ID | 验证项 | 状态 | 说明 |
|----|--------|------|------|
| D10.1 | PNG立绘显示 | P2 | NullAniBackend render需GPU context |
| D10.2 | 立绘表情切换 | ✅ | setExpression |
| D10.3 | 立绘透明度 | ✅ | setOpacity |
| D10.4 | 立绘显示/隐藏 | ✅ | showModel/hideModel |

**待验证的运行时项**：
- D10.1: 引擎启动 → 加载PNG立绘 → 确认显示

## 排查 SOP

1. 打开飞书Base → 筛选负责人=B → 筛选状态≠通过
2. 从上到下逐项排查
3. 对于"待验证运行时"的项：启动引擎，手动验证
4. 通过 → 在飞书记为 ✅ 通过
5. 失败 → 记录现象和根因 → 记入 audit-tracker.md
6. P2标记的项：确认当前状态 → 不需要现在修

## 关键源文件

```
src/render/BgfxRenderDevice.cpp    - GPU渲染设备
src/render/LayerManager.cpp         - 3层合成
src/render/TextureManager.cpp       - 纹理管理
src/render/TextRenderer.cpp         - 文字渲染
src/render/TextRenderer.cpp         - FreeType字体生命周期与文字渲染
src/render/BgfxDraw_Blit.cpp       - 纹理blit
src/render/BgfxQuadBatch.cpp       - 批量提交
src/render/BgfxShaderManager.cpp   - 着色器
src/render/VideoPlayer.cpp         - 视频播放
src/render/api/IRenderDevice.h     - 渲染接口
src/render/api/ILayerManager.h     - 图层接口
src/render/api/ITextureManager.h   - 纹理接口
src/live2d/NullAnimationBackend.cpp - 立绘后端
scripts/transition.lua              - 转场效果
scripts/layers.lua                  - Lua图层API
```

## 已知P1修复 (本次已完成)

- D9.3: 右键隐藏 (Engine.cpp ac32780f)
- D9.6: Ctrl快进 (Engine.cpp ac32780f)
- D9.7: 回溯backlog (Engine.cpp ac32780f)
