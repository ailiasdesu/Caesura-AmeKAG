# 存档系统认证与兼容边界

> **2026-09-05 限定源码同步**：仅核对 SaveManager 的加密策略、provider 边界、显式旧明文导入、云同步暂存校验与加载失败语义。本文不代表重新完成全仓安全审计，也不宣称最新回归已经通过。
> 历史文档曾将信封元数据误写为明文，并将 provider 绕过加密列为当时的现状。本次按当前工作树修正这些表述；测试结果应查对应运行日志，不能由本文推导。

## 1. 加密实际覆盖哪些数据

[SaveManager::save](../../src/storage/SaveManager.cpp) 先构造以下完整 JSON 信封：

```text
schema_version, timestamp, scene, token_index,
thumbnail, engine_version, data
            ↓ envelope.dump()
            ↓ encodeSave()，设置 key 时执行 AES-256-GCM
CAES (4B) | nonce (12B) | tag (16B) | ciphertext（整个 JSON 信封）
```

`scene`、`timestamp`、`schema_version` 等元数据与游戏 `data` 一起处于密文中，随整段密文接受 GCM 认证。它们不是位于 ciphertext 外的明文信封；缺少 AAD 不意味着这些已加密字段未被认证。修改密文字节或使用不匹配的 nonce/key 会使认证失败，不能把未经认证的解密内容交给 JSON 解析器。

CAES 外层没有独立格式版本字段；`schema_version` 是 JSON 中的数据迁移版本。当前 [ICryptoEngine](../../src/archive/api/ICryptoEngine.h) 没有 AAD 参数，存档槽位文件名、文件路径也未加入认证上下文。外部 `CAES` 魔数是格式识别标记，不等于槽位身份或版本的新认证字段。

## 2. 加密策略由调用方决定

策略定义在 [ISaveManager.h](../../src/storage/api/ISaveManager.h)。[EngineConfig](../../src/entry/EngineConfig.h) 的 `saveEncryptionPolicy` 默认 `Compatible`，组合根在初始化存档管理器时设置策略；策略不能由待验文件自报。

| 策略 | 读取 | 保存 |
|---|---|---|
| `Compatible` | 接受合法旧明文；识别 CAES 后必须有正确 key 并通过认证 | 有 key 一律编码为 CAES，无 key 写明文 |
| `RequireEncrypted` | 普通 `load()` 拒绝非 CAES；CAES 必须有正确 key 并通过认证 | 未设 key 或 clear key 后直接失败，不进入 provider/磁盘写入 |

两种策略都会在识别到 CAES 后对缺 key、截断、错误 tag 或解密失败直接返回失败，绝不回退 JSON。设置 key 不会扫描或改写已有文件；清除 key 不会清除严格策略。

兼容模式仍接受“把整个密文文件替换为一个合法明文 JSON 信封”，这是保留旧明文读取的边界；读取此类内容不能声称获得了真实性保证。严格模式拒绝这类替换，但仍不防重放已有的合法密文。

### 显式旧明文导入

`loadLegacyPlaintext(slot, outMeta)` 是 C++ 的显式只读入口。它可以由严格模式的调用方主动选择使用，但具有以下固定行为：

- 拒绝 CAES；有效或损坏的 CAES 都必须走普通认证读取，不能借导入入口退回明文。
- 不改变管理器策略，不自动创建备份，不重写源文件。
- 复用 JSON 信封和内存迁移检查，成功后返回数据及可选元数据。
- 调用方提供 key 并随后显式 `save()` 才会写出密文；保留原槽位还是写入新槽位由调用方决定。

导入是调用方对旧明文数据的显式选择，不将该明文补成“已认证来源”。

## 3. Provider 与云同步的职责

[ISaveProvider](../../src/storage/api/ISaveProvider.h) 是原始字节存储接口，包括 NUL 在内的内容应保持不变。SaveManager 在 provider 写入前编码、读取后解码；默认 LocalFile、自定义内存 provider、HTTP 和 Steam 的 SaveManager 路径不会因安装 provider 而绕过加密。原先 `readFile/writeFile` 首行直接转发造成的旁路已在当前源码中移除，不再需要额外的加密 provider 包装器。

云同步使用可选接口 [ICloudSaveTransport](../../src/storage/api/ICloudSaveTransport.h)：

| 操作 | 方向与职责 |
|---|---|
| `readLocalFile(slotPath)` | 只取得本地上传源的字节 |
| `writeCloudFile(slotPath, bytes)` | 上传给定字节，不重新读取上传源 |
| `readCloudFile(slotPath)` | 只取得远端下载源的字节，不先覆盖本地 |
| `writeLocalFile(slotPath, bytes)` | 将给定字节提交到本地 |

`SaveManager::pushSlotToCloud/pullSlotFromCloud` 的顺序为：**读取一次 → 暂存 bytes → 按当前策略/key 解码与认证 → 检查可加载信封及迁移结果 → 提交同一份 bytes**。明文、坏 tag、错 key、缺 key 或不可加载数据被拒绝时，不调用目标写入。校验成功后也不重新读取源文件；验证期间的内存迁移不会让传输内容被重新序列化或再次加密。

[HttpCloudSaveProvider](../../src/storage/HttpCloudSaveProvider.cpp) 的普通存储仍在本地，显式同步才访问远端。[CloudSaveProvider](../../src/storage/CloudSaveProvider.cpp) 的普通 `readFile/writeFile` 指向 Steam 云存储，因此必须额外区分本地/云端；push 是本地→云端，pull 是云端→本地。

provider 的直接 `pushToCloud/pullFromCloud` 保留原始字节传输行为，不执行 SaveManager 的策略。游戏应通过 SaveManager 的槽位 API 同步；声明支持云同步但未实现暂存 transport 的自定义 provider 会被该路径拒绝。

这里的“失败不覆盖”指**验证未通过时尚未提交**。它不扩大底层文件替换、远端写入失败或断电场景的原子性/持久性保证；也没有新增云冲突合并或时间戳仲裁。

## 4. 加载失败与迁移

`load()` 和 `loadLegacyPlaintext()` 以 null JSON 表示失败，只有成功出口才更新 `SaveMeta`。以下失败不会改动调用方原有的 slot、timestamp、sceneName、thumbnail、tokenIndex 或 schemaVersion：

- CAES 认证失败或当前策略拒绝输入；
- JSON 解析失败、信封不是对象、缺少 `data` 或 `data` 为 null；
- 已注册迁移返回 null 或抛异常。

合法空对象 `{}` 与空数组 `[]` 是可以成功加载的数据，不能用 `json.empty()` 代替 `is_null()` 判定失败。读取和迁移在内存中完成，不自动覆盖旧档或生成 `.bak`。

内置迁移链仍为 v1→v2(playtime)→v3(minigame)→v4(live2d)→v5(editor)，查找已注册迁移，最多执行 64 步。当前源码保留未来 `schema_version` 不迁移透传的行为；这不是所有跨版本组合已经验证的声明。迁移函数由引擎/调用方注册，文件本身不能提供可执行迁移代码。

槽位范围保留普通槽位 0..99、quicksave -1 和 autosave -2，路径由 SaveManager 生成；这里没有扩大任意路径读写能力。`slotExists()` 不是完整真实性检查：兼容模式无 key 时保留原始字节存在探测，实际读取是否成功应检查 `load()`。

## 5. 仍然存在的认证边界

### 槽位文件名未绑定

SaveManager 的 JSON 信封没有由引擎加入并核验的槽位身份，CryptoEngine 接口也未传入槽位 AAD。成功加载时，`SaveMeta.slot` 来自请求参数。使用相同 key 时，把 slot 1 的完整有效密文复制为 slot 2 的文件仍可通过认证；严格模式不会阻止这类槽位交换。

未来若要求防槽位交换，需要定义受认证的槽位身份及旧格式兼容行为。本文没有新增实现或排期承诺，也不再把“元数据位于明文”作为引入 AAD 的理由。

### 时间戳认证不等于防重放

`timestamp` 在密文内受认证，但没有可信的单调状态或旧档拒绝机制。恢复同一 key 下更早的完整密文仍可能成功；云同步的策略校验也不比较哪端更新，不自动解决版本冲突。

### 密钥由调用方管理

`setEncryptionKey()` 接收调用方的 32 字节 key；`clearEncryptionKey()` 和析构会清除管理器持有的 key。引擎没有因此自动生成每会话新 key，也没有实现 key 持久化。重启后要读取旧密文，调用方必须重新提供相同 key；是否跨会话复用由调用方决定，不能据“内存中保存 key”推断重放窗口只限单次进程。

## 6. 回归入口与结果范围

以下文件包含相关回归入口；本节只登记验证面，不替代实际运行日志：

- [test_storage.cpp](../../tests/cpp/test_storage.cpp)：既有加密、槽位、迁移和 provider 行为。
- [test_save_roundtrip.cpp](../../tests/cpp/test_save_roundtrip.cpp)：Engine 默认 provider 加密、重建 Engine 读取、单层 CAES、兼容/严格策略、显式导入、失败元数据及空容器。
- [test_cloud_save.cpp](../../tests/cpp/test_cloud_save.cpp)：HTTP/Steam 原始密文字节、严格模式拒绝非法同步、合法同步及同一暂存 buffer 提交。

槽位绑定、防重放、完整云冲突处理和断电持久性未因这些测试入口而获得已实现或已通过结论。
