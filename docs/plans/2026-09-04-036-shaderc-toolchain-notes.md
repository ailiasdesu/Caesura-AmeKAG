# 036 · Shaderc 工具链勘察笔记（035 后置项）

> 状态：GL 已激活（shaderc 工具链构建成功 → GL profile 130 数组入库，Lut3D 在 GL/GLES 真生效）；Metal 轮=**取消**（Metal 后端属 macOS/iOS=用户明令排除项，勿再生成）
> 日期：2026-09-04
> 背景：t214 palette 3D-LUT 的 GL/Metal 着色器数组待 shaderc 生成（D3D DXBC 已 fxc 本地闭环）。

## 1. 结论

**GL/Metal 的 fs_postfx_lut3d 数组生成 = deprioritized**（本批止损）：bgfx shaderc 完整构建需
glsl-optimizer/glslang/spirv-*/tint 巨库（3rdparty 全在 vendored，但编译量小时级/次），而
D3D11 全链已闭环（t214：program READY + 冒烟零错误）。GL/Metal 维持 t214 的 identity 回落
（无 LUT 效果、不崩、契约测试锁定）。激活前置=构建 shaderc 工具链（本方第 2 节）。

## 2. 本机零 shaderc 情形下的编译路径（本次实测全链）

### 2.1 前置
- MSVC 2022 Community（cl 不在 PATH，用全路径）
- bx/bimg/bgfx 库：build/{bx,bimg,bgfx_lib}/Release/*.lib（CMake vendored 构建产物）

### 2.2 已知坑（全部实测踩到）
1. **MSYS 路径转换**：git-bash 把 /I 参数转成 C:/Program Files/Git/I... → cl 永远收不到 include。
   解法：MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' + -I（连字符）传路径。
2. cl 需要 VC/SDK 头：-I$VCINC -I$SDKU -I$SDK/shared -I$SDK/um（VCINC/SDKU 用 glob 直取，
   不要 dirname 嵌套——git-bash 包装层会错层）。
3. bx 要求：/std:c++20（platform.h static_assert）+ /Zc:__cplusplus + /Zc:preprocessor
   + /DBX_CONFIG_DEBUG=0。
4. fcpp（预处理器）：3rdparty/fcpp 是 **C 源**（cpp1.c..cpp6.c + cpp.h），需一并编译/链接
   （fppPreProcess 符号）。
5. **链接库路径**：/LIBPATH:$VCLIB /LIBPATH:$UMLIB /LIBPATH:$UCRTLIB（msvcprt/uuid 从这里来）。
6. **后端符号强制全链**：shaderc.cpp 无条件引用全部 8 后端（GLSL/HLSL/Metal/DXIL/PSSL/WGSL/SPIRV）
   ——只编部分会 LNK2019；SPIRV（glslang 系）与 GLSL 的 compileSPIRVShader 双桩互斥（去 spirv 源即可）。

### 2.3 最小可行编译（GL/Metal/HLSL 三后端，无优化器）
（待 3rdparty glsl-optimizer 编库后补充——当前停在这里；完整构建另计时。）

## 3. 激活路径（未来）
1. 编译 glsl-optimizer + glslang + spirv-*（或接受 GLSL 无优化器路径）
2. shaders/compile_shaders.bat（SHADERC env 指向本地 shaderc.exe）→ shaders/compiled/*.bin
3. shaders/embed_to_c.py → 更新 src/render/EmbeddedShaders_GL.cpp / EmbeddedShaders_Metal.cpp
4. BgfxShaderManager case 4 从 identity 换真实 program（t214 ShaderManager 预留）

## 4. 现状（诚实标记）
- D3D11：lut3d 全链绿色（DXBC 嵌入）
- GL/Metal/Vulkan：identity 回落（不崩），数组未生成——platform_tested 仍 '-'；矩阵注记已写明
