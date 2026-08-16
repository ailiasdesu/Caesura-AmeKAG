# 存档系统防篡改安全审计（SaveSystem Tamper-Resistance Audit）

> 审计对象：`src/storage/`（SaveManager / ISaveProvider / 加密路径）+ `src/archive/`（CryptoEngine）。
> 审计方式：只读代码分析 + 现有测试用例核对（`tests/cpp/test_storage.cpp`、`tests/cpp/test_save_roundtrip.cpp`）。
> 结论性质：**设计观察与缓解建议，未改动任何实现，未 git 提交**（受命不改实现）。
> 相关既有记录：round 85「自定义 provider 绕过 AES 加密（设计观察）」、round 86「存档大小上限对称修复」。

---

## 0. 交互面与信任边界速览

| 路径 | 位置 | 输入来源 | 信任边界 |
|------|------|----------|----------|
| `KAG.save_game(slot,data,scene,token)` | `SaveBinding.cpp:186` | Lua 游戏脚本 | 槽位受限 0..99 |
| `KAG.load_game(slot)` | `SaveBinding.cpp:203` | Lua 游戏脚本 | 槽位受限 0..99 |
| `KAG.delete_save/save_exists/cloud_*/set_encryption_key` | `SaveBinding.cpp` | Lua 游戏脚本 | 槽位受限 / 云端点由脚本提供 |
| 存档文件内容 | 磁盘 `saves/save_<slot>.json` | **外部攻击者可篡改**（本审计核心场景） | 明文无签名 / 加密时 GCM 认证 |
| 云沙箱（CloudSaveProvider/Steam） | `CloudSaveProvider.cpp` | Steam Remote Storage（攻击者可伪冒） | 无端到端签名 |

---

## 1. 审计矩阵（核心结论表）

| # | 审计维度 | 现状 | 认证/防护覆盖 | 风险评级 |
|---|----------|------|----------------|:--------:|
| a | **AES-GCM 认证覆盖** | 密文与 tag 由 GCM 认证；**nonce、魔数 `CAES`、明文信封元数据、槽位文件路径均不在 GCM AAD 内** | 篡改密文/nonce → 解密失败（有测试覆盖）；篡改信封元数据字段（`scene`/`timestamp`/`schema_version`）→ **无法检测**（见 2a） | **中** |
| b | **槽位回滚 / 重放** | 信封含 `timestamp`（`time(nullptr)`）但**无单调计数器，写入端不校验旧于现有则拒绝** | 攻击者用旧存档覆盖新存档 → 加载成功，时间戳回退无告警 | **中** |
| c | **槽位交换 / 复制** | 槽位仅体现在**磁盘文件名**，密文内部**无槽位绑定** | `save_1.json` 密文整体拷到 `save_2.json` → 加载为有效存档，且 `SaveMeta.slot` 由调用方填充（与文件不绑定） | **中** |
| d | **自定义 provider 绕过 AES** | 安装 `ISaveProvider` 后（含 LocalFile/Cloud/InMemory）加密**完全被旁路**：`SaveManager::writeFile/readFile` 首行即转发原始明文给 provider（`SaveManager.cpp:130,170`） | InMemoryProvider 仅内存无持久化 → **本地低风险**；Cloud/云轮询 provider 若持续 → 明文云落盘隐患 | **低–中** |
| e | **路径注入** | 槽位 0..99 强校验（save/load/delete 均拦）；`slotPath` = `m_saveDir + "save_" + to_string(slot) + ".json"`；`m_saveDir` 固定 `"saves/"`（Engine.cpp:347），无 `../` 逃逸 | **已防御**：槽位越权 + 目录来自组合根硬编码，无外部注入面 | **低** |
| f | **schema 迁移注入** | 迁移仅当 `schemaVer < current` 触发；`migrate()` 非对象返回原样、链上限 64 步；迁移实现为固定闭包（仅条件加字段） | 恶意 `schema_version`（如 v0/超大 v99）不会执行任意代码；v99(>current) 原样透传（有测试覆盖） | **低** |

---

## 2. 逐项分析与证据

### 2a. AES-GCM 认证覆盖（#a）— 关键发现

**加密/解密实现**：`CryptoEngine::encrypt/decrypt`（`src/archive/CryptoEngine.cpp`）。

- Windows BCrypt：`BCRYPT_INIT_AUTH_MODE_INFO(auth)` 将结构清零后仅设置 `pbNonce/cbNonce` 与 `pbTag/cbTag`，**未设置 `pbAuthData/cbAuthData`** → 无 AAD。
- OpenSSL EVP：初始化后**没有对 AAD 的 `EVP_EncryptUpdate`（flag 0）调用** → 无 AAD。

**结果**：GCM tag 只认证：
- 密文体（任何密文字节翻转 → tag 失配拒绝）；
- 隐式认证 nonce 与 key（换成错误的 nonce/key → tag 失配，测试 wrong-key 与 tampered nonce 均覆盖）；
- **不认证**：`CAES` 魔数、明文信封的 `scene/timestamp/schema_version/engine_version` 等、**槽位标识 / 文件路径**。

**后果**：外部攻击者（能改磁盘文件、无法解密）可以对加密存档做以下**仍会被接受的篡改**：
1. 把 slot 1 的整个密文文件复制为 slot 2（解密成功，内容合法，见 2c）。
2. 若游戏读取 `data` 时信任信封外的自定义字段，或依赖 `meta.timestamp/scene` 做逻辑判断，则可被伪造。

> 缓解后文见 §3-A（AAD 绑定槽位/版本）与 §4（成本评估）。

> **测试佐证**：`test_storage.cpp` tampered ciphertext & nonce rejected（770-818）、wrong key rejects（632-655）、forged CAES-magic decoy rejected（691-717）均**只验证密文/nonce 篡改被拒**，**没有**任何元数据字段篡改被拒或槽位交换被拒的用例——与上述审计结论一致（当前设计确实不防御这两类）。

### 2b. 槽位回滚 / 重放（#b）

- 写入端 `save()` 填充 `envelope["timestamp"] = time(nullptr)`（`SaveManager.cpp:274`）；**无进程持久化单调计数器**，每次全新取自系统时钟。
- 加载端 `load()` 读取时间戳仅填 `SaveMeta.timestamp`，**无新存档不得被旧存档覆盖的写入/加载校验**。
- 攻击者保存旧版（更早时间戳）的合法密文为同名文件 → 重载 = 时间回退，无法检测。
- 说明：若加密键每会话变化，跨会话的旧密文本就走不通（见 2g 密钥瞬态），所以回滚威胁主要在**同一密钥生命周期内**。

### 2c. 槽位交换 / 复制（#c）

- 槽位只体现在**文件名**（`slotPath`），密文载荷内**无槽位字段**，tag 也未绑定槽位。
- `SaveMeta.slot` 在 `load()` 内直接 `outMeta->slot = slot`（来自调用参数，`SaveManager.cpp:335`），不读取文件内容——文件被换名后加载的 `meta.slot` 显示的是**新文件名对应的槽位**，与密文来源无关。
- slot 1 密文拷到 slot 2：解密成功、加载成功、`meta.slot==2`。**槽位语义由文件名而非内容决定** → 单槽位内拷入他槽密文不可被内容层识别。

### 2d. 自定义 provider 绕过 AES（#d）

- `SaveManager::writeFile/readFile`（`SaveManager.cpp:169-170 / 129-130`）：`if (m_saveProvider) return m_saveProvider->writeFile(path, content);`——**安装 provider 即完全跳过加密/解密**，payload 以明文进出 provider。
- provider 不含加密意识：`ISaveProvider` 注释与 `test_storage.cpp:505-509` ARCHITECTURE NOTE 均承认 ISaveProvider 是原始字节存储、无加密意识，加密由 SaveManager 的 m_keySet + CAES 魔数决定。
- **威胁模型评级**：
  - **InMemorySaveProvider（测试用）**：仅进程内存、无持久化，进程退出即消失 → **低风险**（设计观察，round 85 已记录）。
  - **CloudSaveProvider / HttpCloudSaveProvider**：若在加密密钥已启用的产品中把它设为 `m_saveProvider`（`configureCloudSync(endpoint)`），则云端/HTTP 落盘的是**明文 JSON** → 云侧敏感存档泄露，评级**中**（取决于是否真实启用 + 密钥是否启用）。
- **并发观察**（round 94 已记录）：SaveManager 无主线程守卫、无内部互斥；并发安全性完全委托给 provider（线程安全）——与加密/篡改无直接关系，但提示引擎层不保证 I/O 原子性，仅单个文件原子重命名。

### 2e. 路径注入 / 越权（#e）— 已防御

- `save/load/slotExists/deleteSlot` 全部先做 `slot < 0 || slot > 99` 拦截（`SaveManager.cpp:258,299,406,411`），杜绝用越权槽位拼接 `../` 或逃逸存档目录。
- `slotPath` 完全由 `slot`（受限整数）+ `m_saveDir`（组合根硬编码 `"saves/"`）拼接，**无来自用户/Lua 的路径成分**。
- `LocalFileSaveProvider::listFiles` 将 pattern 拆目录+glob 后遍历目录（`ISaveProvider.cpp:37-57`），pattern 由 SaveManager 内部构造，不暴露给 Lua。
- **结论**：未发现可被外部攻击者触发的 `../` 逃逸。唯一理论面是组合根把任意目录传给 `init()`，但那是调用方责任，非存档层漏洞。
- 云侧：`HttpCloudSaveProvider::safeName` 剥离目录成分（:15-19）防云键路径注入；`CloudSaveProvider` 的 `.meta/.chunkNNN` 键由内部路径拼接，槽位受限。

### 2f. schema 迁移注入（#f）— 已防御

- `load()` 仅当 `schemaVer < m_currentSchemaVersion` 才调用 `migrate()`（`SaveManager.cpp:345`）；未来版本（> current）**原样透传不迁移**（测试 unknown future schema version 覆盖）。
- `migrate()`：非对象直接返回；链查找 `m_migrations`（仅有 registerMigration 注册的内置闭包）；步数上限 64 防环。
- **无任意函数执行**：迁移是固定 lambda（v1→v5 仅 if(!contains) 补字段）。恶意 payload 无法把迁移转成代码执行，最多通过构造 `schema_version` 触发已有的内置迁移，行为确定且无副作用。
- **剩余面**：`envelope.value("schema_version", 1)` 对缺失/非数值的默认；极端值 0 触发从 v0 链查找（无注册 → 直接 break）→ 安全。
- 结论：**低风险**，无需改动。

### 2g. 加密密钥管理（审计附加观察）

- 唯一生产调用方是 Lua `KAG.set_encryption_key`（`SaveBinding.cpp:294-301`，接受 32 字节 Lua 字符串），无进程内密钥派生/持久化存储。
- 密钥仅存于 `SaveManager::m_encryptKey[32]`，析构时 secureErase（`SaveManager.cpp:52-55`）。跨会话不持久 → **每个会话密钥不同，历史密文在会话重启后不可读**（除非脚本每次重建同一密钥）。
- 这本身不是漏洞，但：既然密钥是瞬态/每会话，**同一密钥生命周期很短**，降低了回滚/交换的现实可利用时长（必须在一次密钥有效的窗口内篡改）。这缓解了 2b/2c 的实际风险，但**不消除**。文档化即可。

---

## 3. 缓解建议（按优先级，均为可选、不强推）

### A. GCM AAD 绑定槽位 + 版本（削弱 #a/#c）— 成本 M，收益高
- 在 `CryptoEngine::encrypt/decrypt` 增加 AAD 入参（如 `const uint8_t* aad, size_t aadLen`），调用方把**槽位标识 + schema_version（+可选魔数版本）**作为 AAD。
- 效果：槽位 A → 槽位 B 的密文复制后，因 AAD 不匹配被 GCM 拒（解出 #a 元数据篡改 + #c 槽位交换一大半）。
- 增量成本：约改 1 接口 + 2 后端路径（BCrypt/OpenSSL）+ SaveManager 两处调用 + 回归测试；工作量约 **M（半轮）**。需注意向后兼容（旧存档无 AAD → 升级需迁移或双路径）。

### B. 槽位绑定 HMAC / 单调计数器防回滚（削弱 #b）— 成本 M–L，收益中
- 方案 1（轻）：在信封加入**单调计数**（进程内 `std::atomic<uint64_t>` 按槽位维护或自文件读取最大值+1），加载时若加载计数值 < 现有计数值则拒绝/告警。成本 **S–M**。
- 方案 2（重）：槽位专属 HMAC（独立密钥对 `{slot, schema_version, timestamp, data}` 做 MAC 附在信封），加载时验签。成本 **M**，且需导出/持久化第二把 MAC 密钥（与 2g 的瞬态密钥模型冲突，需一并设计）。
- 现实权衡：攻击场景是同一密钥生命周期内被本地攻击者回滚——较低威胁；建议以**文档承认** + 方案 1（进程内计数）为准，方案 2 仅在需要对抗存档工具外篡改时上。

### C. Provider 加密强制（制约 #d）— 成本 M，收益中
- 把加密从 SaveManager 明文→provider 下沉为**在调用 provider 前后强制加/解密**（即 provider 边界必须过密），或在 `setSaveProvider` 安装自定义/云 provider 时保持密钥加密语义（提供加密代理 provider 包装任意后端）。
- 对 InMemory（测试）可豁免；对 Cloud/HTTP 若产品启用加密则必须强制。成本 **M**（需新增加密 provider 适配器或重构 SaveManager I/O 顺序）。
- 建议：先文档化云启用加密时 provider 路径必须经加密适配器，避免未来误用。

### D. 保持现状（明确接受的）：
- #e 路径注入已防御，无需改。
- #f 迁移注入已防御，无需改。
- 2g 密钥瞬态：文档化行为即可，不改。
- InMemoryProvider 明文：检测用，不改（round 85 已接受）。

---

## 4. 增量成本评估汇总

| 建议 | 工作量 | 破坏/兼容性 | 是否推荐 |
|------|:------:|-------------|:--------:|
| A. GCM AAD 槽位/版本绑定 | M | 旧密文需迁移/双路径 | 推荐（若在意存档槽位完整性） |
| B. 单调计数器（轻） | S–M | 新字段，旧存档默认 0 兼容 | 推荐做轻量版 |
| C. provider 加密强制 | M | 需新增适配器/改 I/O 顺序 | 有条件做（云启用加密时） |

> 全部为建议存量，**不影响当前基线构建/测试**；如需实施，按 AGENTS.md §10 走接口变更流程（新增 AAD 入参属 ICryptoEngine 接口变更）。

---

## 5. 结论与优先级

| 优先级 | 项 | 处置 |
|:------:|----|------|
| P1 | 无**高危且修复极小**的利用面（无路径逃逸、无迁移代码注入、无任意文件写） | —— |
| P2 | AES-GCM 不认证元数据/槽位（#a/#c），提供 AAD 绑定为最有效加固 | 建议 A（M） |
| P2 | 槽位回滚无单调计数（#b） | 建议 B 轻量版（S–M） |
| P3 | 自定义/云 provider 绕过加密（#d） | 文档化 + 条件做建议 C（M） |
| P3 | 密钥瞬态（2g）、路径注入（#e）、迁移注入（#f） | 文档化 / 已防御，不改 |

**总结**：存档系统基础是健壮的——密文/nonce/key 篡改均被 GCM 拒绝且有测试覆盖，槽位边界与路径拼接已防注入。**主要缺口在认证不覆盖元数据与槽位绑定、无回滚防护**，威胁等级为本地攻击者 + 同密钥窗口内篡改，属**中等级**；建议优先做 GCM AAD 槽位绑定（成本低、直接消除槽位交换 + 元数据伪造两类篡改）。

---

## 附：审计核对过的测试佐证索引

- `tests/cpp/test_storage.cpp`：加密往返（512）、磁盘非明文（561）、无密钥拒载（599）、错钥+forged 魔数（632/691）、密文/nonce 篡改拒（770）、槽位边界（325/719）、迁移链（413/974）、自定义 InMemoryProvider（1070/1109）、并发 provider（1234+）。
- `tests/cpp/test_save_roundtrip.cpp`：密钥场景 + transientKey 瞬态密钥（263）。
- 未覆盖（与本审计结论一致的空白）：**无元数据字段篡改被拒、槽位交换被拒、回滚被拒用例**——当前设计确实不防御。

