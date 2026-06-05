-- ===========================================================================
--  Caesura (AmeKAG) — i18n.lua  [P1.3]
--  Multi-language internationalization.
--  Loads string tables from assets/lang/xx.lua.
--  KAG text parser replaces {key} tokens with translated strings.
-- ===========================================================================

local i18n = {}

-- Current language code
i18n.current = "zh"
-- Loaded string table (flat key->value)
i18n.strings = {}
-- Fallback string table (always English)
i18n.fallback = {}

-- ===========================================================================
-- i18n.load(langCode) — load language table from assets/lang/<code>.lua
-- ===========================================================================
function i18n.load(langCode)
    langCode = langCode or "zh"
    i18n.current = langCode

    -- Try loading the language file
    local path = "assets/lang/" .. langCode .. ".lua"
    local ok, data = pcall(function()
        local f = io.open(path, "r")
        if not f then return nil end
        local txt = f:read("*a")
        f:close()
        return load("return " .. txt, "i18n", "t", {})()
    end)

    if ok and type(data) == "table" then
        i18n.strings = data
        print("[i18n] Loaded " .. langCode .. " (" .. #data .. " keys)")
    else
        -- If file not found, try built-in fallback
        print("[i18n] Language file not found: " .. path .. ". Using built-in.")
        i18n._loadBuiltin(langCode)
    end

    -- Load English fallback if not already loaded and current is not English
    if langCode ~= "en" and next(i18n.fallback) == nil then
        local ok2, fb = pcall(function()
            local f = io.open("assets/lang/en.lua", "r")
            if not f then return nil end
            local txt = f:read("*a")
            f:close()
            return load("return " .. txt, "i18n", "t", {})()
        end)
        if ok2 and type(fb) == "table" then
            i18n.fallback = fb
        end
    end

    return i18n.strings
end

-- ===========================================================================
-- i18n.t(key) -> translated string
-- Looks up key in current strings, falls back to English, then returns key itself.
-- ===========================================================================
function i18n.t(key)
    if not key or #key == 0 then return key end
    -- Direct lookup
    local val = i18n.strings[key]
    if val ~= nil then return tostring(val) end
    -- Fallback to English
    val = i18n.fallback[key]
    if val ~= nil then return tostring(val) end
    -- Return key as-is
    return key
end

-- ===========================================================================
-- i18n.expand(text) -> string with {key} replaced by translations
-- Only replaces {key} tokens in text; non-token content passes through.
-- ===========================================================================
function i18n.expand(text)
    if not text or #text == 0 then return text or "" end
    return (text:gsub("{([%w_]+)}", function(key)
        return i18n.t(key)
    end))
end

-- ===========================================================================
-- i18n.available() -> { "zh", "en", "ja", ... }
-- Scans assets/lang/ for available .lua files.
-- ===========================================================================
function i18n.available()
    local langs = {"zh"}  -- built-in default
    local fileutil = require("fileutil")
    local files = fileutil.scan_dir("assets/lang", "%.lua$")
    for _, fname in ipairs(files) do
        local code = fname:match("^(.-)%.lua$")
        if code and code ~= "zh" then
            table.insert(langs, code)
        end
    end
    return langs
end

-- ===========================================================================
-- i18n._loadBuiltin(langCode) — minimal built-in tables for when no file exists
-- ===========================================================================
function i18n._loadBuiltin(langCode)
    local builtins = {
        zh = {
            title_screen = "标题画面",
            new_game = "新游戏",
            continue = "继续",
            load_game = "读取存档",
            settings = "设置",
            gallery = "CG鉴赏",
            music_room = "音乐室",
            back = "返回",
            yes = "是",
            no = "否",
            save = "保存",
            load = "读取",
            volume_bgm = "BGM音量",
            volume_se = "音效音量",
            volume_voice = "语音音量",
            text_speed = "文字速度",
            fullscreen = "全屏",
            language = "语言",
            skip_mode = "跳过模式",
            auto_mode = "自动模式",
        },
        en = {
            title_screen = "Title Screen",
            new_game = "New Game",
            continue = "Continue",
            load_game = "Load Game",
            settings = "Settings",
            gallery = "CG Gallery",
            music_room = "Music Room",
            back = "Back",
            yes = "Yes",
            no = "No",
            save = "Save",
            load = "Load",
            volume_bgm = "BGM Volume",
            volume_se = "SE Volume",
            volume_voice = "Voice Volume",
            text_speed = "Text Speed",
            fullscreen = "Fullscreen",
            language = "Language",
            skip_mode = "Skip Mode",
            auto_mode = "Auto Mode",
        },
        ja = {
            title_screen = "タイトル",
            new_game = "ニューゲーム",
            continue = "コンティニュー",
            load_game = "ロード",
            settings = "設定",
            gallery = "CG鑑賞",
            music_room = "音楽室",
            back = "戻る",
            yes = "はい",
            no = "いいえ",
            save = "セーブ",
            load = "ロード",
            volume_bgm = "BGM音量",
            volume_se = "SE音量",
            volume_voice = "ボイス音量",
            text_speed = "文字速度",
            fullscreen = "フルスクリーン",
            language = "言語",
            skip_mode = "スキップ",
            auto_mode = "オート",
        },
    }
    i18n.strings = builtins[langCode] or builtins.zh
end

-- Auto-load default language on module load
pcall(function() i18n.load("zh") end)

return i18n
