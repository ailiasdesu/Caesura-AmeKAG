## Phase 6: 存档与国际化 (P1)

**目标:** JSON 序列化存档/读档、缩略图、跨版本迁移、i18n 框架。

**依赖:** Phase 1+4 | **验收:** 存档->退出->读档->状态恢复一致, _T("msg_hello") 翻译生效

### Task 6.1: SaveManager (JSON 存档)
**Files:** Create: `src/system/SaveManager.h`, `src/system/SaveManager.cpp`, `scripts/kag/commands/save.lua`

```cpp
// src/system/SaveManager.h
#pragma once
#include <string>
#include <vector>

namespace caesura {
struct SaveMeta {
    int slot; std::string timestamp, sceneName, thumbnail;
    int tokenIndex, schemaVersion;
};

class SaveManager {
public:
    static SaveManager& instance();
    void init(const std::string& saveDir);
    bool save(int slot, const std::string& jsonData, const std::string& thumbnailPng = "");
    std::string load(int slot);
    std::vector<SaveMeta> listSaves();
    bool migrate(const std::string& jsonData, int fromVersion, int toVersion);
private:
    std::string m_saveDir;
    int m_currentSchemaVersion = 1;
};
}
```

```cpp
// save() - JSON 序列化
bool SaveManager::save(int slot, const std::string& jsonData, const std::string& thumb) {
    // 构建存档 JSON
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("schema_version", m_currentSchemaVersion, doc.GetAllocator());
    doc.AddMember("timestamp", rapidjson::Value(os.date).Move(), doc.GetAllocator());
    doc.AddMember("data", rapidjson::Value(jsonData).Move(), doc.GetAllocator());

    if (!thumb.empty()) {
        // thumbnail: Base64 PNG, quality=80, 限制 64KB
        doc.AddMember("thumbnail", rapidjson::Value(thumb).Move(), doc.GetAllocator());
    }

    // 写入文件
    std::string path = m_saveDir + "/save_" + std::to_string(slot) + ".json";
    return writeJsonFile(path, doc);
}
```

```lua
-- scripts/kag/commands/save.lua
return {
    save = function(ctx, p)
        local slot = tonumber(p.slot) or 0
        -- 序列化 f 变量空间 (过滤不可序列化类型)
        local saveData = kag.variables.serialize_f(ctx)
        saveData.token_index = ctx.token_index
        saveData.scene_path = ctx.current_scene
        local json = require("json")
        local ok = backend.save(slot, json.encode(saveData))
    end,
    load = function(ctx, p)
        local slot = tonumber(p.slot) or 0
        local jsonStr = backend.load(slot)
        if not jsonStr then return end
        local data = json.decode(jsonStr)
        -- 恢复变量
        for k, v in pairs(data) do ctx.f[k] = v end
        ctx.token_index = data.token_index
        -- 重新加载场景并从存档位置继续
        scheduler.run(ctx, loadScene(data.scene_path))
    end,
}
```

### Task 6.2: I18nManager
**Files:** Create: `src/system/I18nManager.h`, `scripts/i18n.lua`

```lua
-- scripts/i18n.lua
local i18n = {}
local translations = {}
local current_lang = "zh"

function i18n.load(lang, path)
    local f = io.open(path)
    if not f then return end
    translations[lang] = json.decode(f:read("*all")); f:close()
end

function i18n.set_language(lang)
    current_lang = lang
end

-- _T 翻译函数: %{key} 插值, \_ 原样下划线, \% 原样百分号
function _T(key, params)
    local dict = translations[current_lang] or {}
    local text = dict[key] or key
    if params then
        text = text:gsub("%%{(%w+)}", function(k) return tostring(params[k] or "") end)
    end
    text = text:gsub("\_", "_"):gsub("\%%", "%%")
    return text
end

_G._T = _T
return i18n
```

### Phase 6 检查点
- [x] JSON 存档/读档 (schema_version + 迁移)
- [x] 存档缩略图 (PNG base64, quality=80, max 64KB)
- [x] _T() 翻译 + %{key} 插值 + 转义

---
