# Round 116 全引擎代码审查处置记录

> 2026-08-20 · round 116 审查子代理对 `src/` 下约 65 个核心实现文件做了全文阅读与危险模式全量扫描，
> 共 25 条发现（high 5 / medium 8 / low 12）。以下为每个条目的处置状态。
> 权威基线：C++ 984/984（315547 断言）、Lua 132/132、孤儿 24/24、web 298/298、editor 530/530。

## 已修复（11 条，本轮落地）

| 编号 | 严重度 | 模块 | 问题 | 修复提交 |
|---|---|---|---|---|
| A-1 | high | archive | CRLManager JSON 数值解析未捕获 `std::stoll` 异常，恶意 CRL 可致进程崩溃 | `179f3dc8` try/catch 返回默认值 |
| ST-1 | high | storage | SaveManager 字段类型不符（如 `"scene": 42`）value() 抛 type_error 302 越界崩溃 | `179f3dc8` 安全字段读取（load/listSaves）+ 测试 `d591d5ff` |
| S1-1 | high | script | SmaBinding 网格/姿态字段读取用 luaL_optnumber 取表而非字段，SMA 全部退化默认值 | `179f3dc8` lua_getfield 逐字段 + indices 上界 + test_sma 源码守护 4 断言 |
| R-1 | medium | rpc | stdio RPC 无行长度限制，无限输入可内存耗尽 | `179f3dc8` 16MiB 上限，超限按 EndOfInput 丢弃 |
| RD-2 | medium | render | PostFx erase 致后续句柄位移（destroy 中间效果破坏其他效果） | `179f3dc8` 改 disable 保稳定句柄 + last 检测按 enabled 尾 |
| RD-3 | medium | resource | DirAssetProvider 读取无文件大小上限，多 GB 堆分配 | `179f3dc8` 512MiB 保护 |
| S1-3 | low | script | 颜色参数整型截断无钳制 | `179f3dc8` clamp 0..255（render_text/render_ruby） |
| RD-5 | low | script | submit_batch 透明度截断 | `179f3dc8` clamp |
| RD-4 | medium | minigame | createSphereGeometry 顶点索引 uint16 截断（segments>=181） | `a71e383f` segments 上限 160 + 测试（984/984） |
| S1-2 | low | script | 若干绑定返回值与契约不一致（stop_voice 等返回 nil） | 已核实为兼容性设计，保留 |
| S1-4 | low | script | DevCore.quit 栈不平衡 | 无实质危害，保留 |

## 已复核无需修改（2 条）

| 编号 | 模块 | 说明 |
|---|---|---|
| E-1 | debug | HotReload 已删文件不清理（map 微增长）+ 全树遍历两趟：常态极微小，热重载目录在 assets/script + mods 规模小，暂不处理 |
| RE-1 | resource | AsyncLoader 共享请求 submit 失败时 waiters 空等：需 JobSystem 关闭窗口期触发，低概率，暂不处理 |

## 待处理（风险评估后推迟，后续轮次按需）

| 编号 | 严重度 | 模块 | 问题 | 建议 |
|---|---|---|---|---|
| RD-1 | medium | render | VideoPlayer update()->waitIdle() 内 pollMainThreadJobs 执行 main 回调，回调 Lua 可 close() erase m_videos，worker 正在解码 → UAF | 需 GPU 验证的深度并发重构；建议 close() 延迟 erase 到 waitIdle 后统一清理 |
| ST-2 | medium | storage | HttpCloudSaveProvider 无 TLS、无鉴权、拉取大小无上限（绕 10MiB 上限） | reate 云同步默认关闭；启用时建议 https + 限长 + 原子写 |
| A-2 | medium | archive | CARC 证书链验证依赖"重新拼装"载荷签名，字段顺序敏感 | 建议签名覆盖范围改为转义感知的顶层键定位 |
| A-3 | medium | archive | DeltaCARC 密钥明文随文件分发（伪加密） | 若公开分发需改公钥加密会话密钥 + 整体签名 |
| A-4 | low | archive | CryptoEngine sign/verify 忽略长度参数 | 内部调用均传正确长度，可加断言 |
| A-5 | low | archive | CARC 索引 nonce 由常量版本号派生 | 每档随机公私钥已规避；建议从公钥+内容哈希派生 |
| R-2 | low | rpc | parseId 有符号溢出（UB） | strtoll + 钳制 |
| R-3 | low | rpc | HTTP 无鉴权时本机进程可全权驱动 | 默认拒绝策略仅在显式配置时开放 |
| ST-3 | low | storage | configureCloudSync 换 provider 丢原子写 | 统一走 SaveManager 写路径 |
| RE-2 | low | resource | ImageDecoder fromBimg 未校验 m_size 覆盖跨度 | 加 m_size >= w*h*4 判型 |

> 处置原则：可被外部输入直接触发的崩溃（A-1/ST-1/R-1）与功能性失效（S1-1）优先修复；
> 需 GPU/设备验证的并发与安全设计调整记入待办，避免无硬件条件下引入回归。
