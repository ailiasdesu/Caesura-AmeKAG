# CI 三端调试提示词

## 通用规则

```
你是 Caesura (AmeKAG) 引擎的 CI 工程师。先读 AGENTS.md。
目标：Windows/macOS/Linux 三端 CI 全部绿色通过。

工作方式：
- git push origin master → 触发 GitHub Actions
- 观察 Actions 日志 → 定位错误 → 本地或远程修复 → 提交 → 再次推送
- 一步一提交，每次推送后等待 CI 结果
- 不要猜测，根据 CI 错误日志实际内容修复

构建：cd build && cmake --build . --config Debug --parallel
测试：cd build\tests\Debug && .\CaesuraTests.exe
```

---

## Step 1: 触发首次 CI

```
推送当前 master 到 GitHub：
git push origin master

等待 GitHub Actions 完成（约 10-20 分钟）。
记录三端结果：Windows(✓/✗), macOS(✓/✗), Linux(✓/✗)
```

---

## Step 2: 根据失败日志逐端修复

### Windows 修复参考

```
预期：绿（本地 MSVC 构建已通过）

如果失败，常见原因：
- vcpkg SDL3 安装超时 → 重试机制或缓存
- 测试 DLL 缺失 → 确认 SDL3.dll 复制路径正确
- Release 构建链接错误 → 检查优化标志
```

### macOS 修复参考

```
已知：之前有 || true 跳过测试，已移除

构建失败常见原因：
- brew 包名不对 → brew search 确认包名
- SDL3 安装路径 CMake 找不到 → 设置 SDL3_DIR
- bgfx Metal 后端编译失败 → 检查 BX_PLATFORM_OSX 条件编译
- 缺少 Metal 框架 → -framework Metal -framework QuartzCore

测试失败常见原因：
- 测试找不到 SDL3.dylib → 设置 DYLD_LIBRARY_PATH
- 测试二进制路径不对 → 确认 CMake 输出目录
- 测试 CWD 不对 → 从 build/tests 目录运行
- FreeType 找不到字体 → macOS 系统字体路径

修复顺序：
1. 先让构建通过（cmake --build）
2. 再让测试通过
```

### Linux 修复参考

```
构建失败常见原因：
- SDL3 源码构建依赖缺失 → apt install 补充
  可能缺失：libasound2-dev libpulse-dev libwayland-dev 
           libxkbcommon-dev libdrm-dev libgbm-dev
- OpenSSL 版本不兼容 → 检查 libssl-dev 版本
- bgfx OpenGL 后端需要 → apt install libegl1-mesa-dev libgles2-mesa-dev

测试失败常见原因：
- 测试二进制路径 →./build/tests/CaesuraTests
- SDL3 动态库找不到 → sudo ldconfig 或 LD_LIBRARY_PATH
- headless 环境无 X11 display → 设置 SDL_VIDEODRIVER=dummy
- 耦合计数 → python3 可用性

修复顺序：
1. 先让构建通过
2. 再让测试通过
3. 最后让耦合计数通过
```

---

## Step 3: 逐次迭代

```
每次迭代：
1. 读 CI 失败日志 → 定位根因
2. 修改代码/配置 → 提交
3. git push origin master
4. 等待 CI → 确认修复生效或定位下一个错误

重复直到三端全绿。
```

---

## Step 4: 全绿后

```
- 确认 GitHub Actions badge 显示 "passing"
- 确认 README.md 中的 badge 链接指向正确的 workflow
- 提交最终状态
```

---

## 验收

- [ ] Windows CI 绿（Debug + Release 构建 + 测试）
- [ ] macOS CI 绿（构建 + 测试，无 || true）
- [ ] Linux CI 绿（构建 + 测试 + 耦合计数）
- [ ] 295 测试在三端全部通过
