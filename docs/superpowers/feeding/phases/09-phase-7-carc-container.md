## Phase 7: CARC 现代化容器 (P0+P2)

**目标:** AES-256-GCM 加密、Ed25519 签名、Provider Chain、打包工具 carc_pack.exe。

**依赖:** Phase 0 | **验收:** carc_pack 打包目录->CARC, 引擎读取 CARC 验证签名+解密+显示资源

### Task 7.1: CryptoEngine
**Files:** Create: `src/carc/CryptoEngine.h`, `src/carc/CryptoEngine.cpp`

```cpp
// src/carc/CryptoEngine.h
#pragma once
#include <vector>
#include <cstdint>

namespace caesura::carc {

struct CARCHeader {
    char magic[4] = {'C','A','R','C'};  // 魔数
    uint32_t version = 1;
    uint64_t contentOffset;   // Content 区块偏移
    uint64_t contentSize;     // 加密后大小
    uint64_t indexOffset;     // Index 区块偏移
    uint64_t indexSize;       // Index 加密后大小
    uint32_t numFiles;
    uint8_t reserved[28];     // 对齐 64B
};
static_assert(sizeof(CARCHeader) == 64);

struct FileEntry {
    uint8_t pathHash[32];    // SHA-256(relative_path)
    uint64_t offset, size;
    uint8_t aesKey[32], nonce[12], tag[16];
};

class CryptoEngine {
public:
    // AES-256-GCM 加密/解密
    static std::vector<uint8_t> encrypt(const uint8_t* data, size_t len,
        const uint8_t key[32], uint8_t nonce[12], uint8_t tag[16]);
    static std::vector<uint8_t> decrypt(const uint8_t* data, size_t len,
        const uint8_t key[32], const uint8_t nonce[12], const uint8_t tag[16]);

    // Ed25519 签名/验签
    static bool sign(const uint8_t* data, size_t len,
        const std::string& privateKeyPem, uint8_t sig[64]);
    static bool verify(const uint8_t* data, size_t len,
        const std::string& publicKeyPem, const uint8_t sig[64]);

    // 随机密钥生成
    static void generateKey(uint8_t key[32]);
    static void generateNonce(uint8_t nonce[12]);

    // SHA-256
    static void sha256(const uint8_t* data, size_t len, uint8_t hash[32]);
};

} // namespace caesura::carc
```

### Task 7.2: CARCReader (读取+验证+解密)
**Files:** Create: `src/carc/CARCReader.h`

```cpp
// src/carc/CARCReader.h
#pragma once
#include "CryptoEngine.h"
#include <memory>
#include <unordered_map>

namespace caesura::carc {

struct CarcFileInfo {
    uint64_t offset, size;
    uint8_t aesKey[32], nonce[12], tag[16];
};

class CARCReader {
public:
    // 打开 CARC 文件: Header->验签->解密 Index->构建文件索引
    bool open(const std::string& path, const std::string& pubKeyPath = "");

    // 读取单个文件 (解密+解压)
    std::vector<uint8_t> readFile(const std::string& relativePath);
    std::vector<uint8_t> readFileByHash(const uint8_t pathHash[32]);

    // 索引信息
    const std::vector<std::string>& fileList() { return m_fileList; }
    uint32_t version() { return m_header.version; }

private:
    CARCHeader m_header;
    std::unordered_map<std::string, CarcFileInfo> m_index;  // pathHash hex -> info
    std::vector<std::string> m_fileList;
    std::ifstream m_stream;

    bool verifySignature(const std::string& pubKeyPem);
    bool decryptIndex();
};

} // namespace
```

**加载流程:**
```
1. 读取 Header (64B) -> 验证 magic="CARC"
2. 从 CARC 末尾读取 PublicKey (32B) 或从编译时嵌入处读取
3. 读取 Signature (末尾 64B) -> Ed25519 验签 (覆盖 Header+Content+Index)
4. 解密 Index Block (AES-256-GCM, 密钥派生自主密钥)
5. 构建文件索引: pathHash -> {offset, size, aesKey, nonce, tag}
6. 读取文件: 定位 Content[offset:offset+size] -> AES-GCM 解密 -> zstd 解压
```

### Task 7.3: CarcAssetProvider (IAssetProvider 实现)
**Files:** Create: `src/carc/CarcAssetProvider.h`

```cpp
// src/carc/CarcAssetProvider.h
#pragma once
#include "resource/IAssetProvider.h"
#include "CARCReader.h"

namespace caesura::carc {

class CarcAssetProvider : public IAssetProvider {
public:
    CarcAssetProvider(std::unique_ptr<CARCReader> reader);

    // IAssetProvider 接口
    std::vector<uint8_t> read(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::string getSource() override { return "CARC"; }
    int priority() override { return 10; }  // 最高优先级

private:
    std::unique_ptr<CARCReader> m_reader;
};

} // namespace
```

### Task 7.4: Provider Chain
**Files:** Modify: `src/resource/ProviderChain.h`

```cpp
// src/resource/ProviderChain.h
#pragma once
#include "resource/IAssetProvider.h"
#include "resource/DirAssetProvider.h"
#include "carc/CarcAssetProvider.h"
#include <vector>
#include <memory>

namespace caesura {

class ProviderChain {
public:
    void addProvider(std::unique_ptr<IAssetProvider> provider);
    std::vector<uint8_t> read(const std::string& path);
    bool exists(const std::string& path);

private:
    std::vector<std::unique_ptr<IAssetProvider>> m_providers;  // 按 priority 排序
    void sortByPriority();  // 插入后自动按 priority 降序排列
};

} // namespace
```

### Task 7.5: carc_pack.exe 打包工具
**Files:** Create: `tools/carc_pack/main.cpp`

```cpp
// tools/carc_pack/main.cpp
int main(int argc, char* argv[]) {
    // 用法: carc_pack.exe <input_dir> <output.carc> <private.key>
    // 流程:
    // 1. 遍历 input_dir 所有文件 -> 构建文件列表
    // 2. 每个文件: zstd 压缩 -> 随机 AES-256 密钥 -> AES-GCM 加密 -> 写入 Content 区块
    // 3. 构建 Index 区块: 文件路径 SHA-256 + offset/size + AES 密钥(公钥加密) + GCM tag
    // 4. Index 区块 AES-GCM 加密 (密钥派生自主密钥)
    // 5. 写入 Header (64B)
    // 6. Ed25519 签名 (Header+Content+Index) -> 写入 Signature (64B)
    // 7. 附加 PublicKey (32B) 到 CARC 末尾
}
```

### Phase 7 检查点
- [x] AES-256-GCM 加密/解密
- [x] Ed25519 签名/验签
- [x] CARCReader 读取+验签+解密+解压
- [x] Provider Chain 责任链: CARC(10) > patch(8) > dir(5)
- [x] carc_pack.exe 打包工具



---
