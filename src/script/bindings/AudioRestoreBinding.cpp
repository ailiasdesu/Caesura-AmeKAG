#include "AudioRestoreBinding.h"
#include "BindingAssetPath.h"
#include "../../audio/api/IAudioRestore.h"
#include "../../resource/api/IAssetReader.h"
#include "../../di/BackendRegistry.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {
namespace {
constexpr char kAudioType[] = "Caesura.PreparedAudio";
constexpr size_t kMaxAudioBytes = 64 * 1024 * 1024;
// A trivial Lua-owned box: GC and explicit discard may both run safely. The
// pointed-to preparation only owns CPU data and needs no live backend to die.
struct AudioBox { IPreparedAudioState* prepared; };
struct AudioInput {
    const char* path = "";
    size_t pathSize = 0;
    double position = 0;
    float gain = 1;
    bool looping = false;
    uint64_t framePosition = 0;
    uint32_t frameFraction = 0;
    uint32_t sourceRate = 0;
    uint32_t outputRate = 0;
};

int failure(lua_State* state, const char* error, bool boolean = false) {
    if (boolean) lua_pushboolean(state, false);
    else lua_pushnil(state);
    lua_pushstring(state, error && error[0] ? error : "Audio restore operation failed");
    return 2;
}

void rawField(lua_State* state, int index, const char* key) {
    index = lua_absindex(state, index);
    lua_pushstring(state, key);
    lua_rawget(state, index);
}

bool readUnsigned(lua_State* state, int table, const char* name, uint64_t limit, uint64_t& value) {
    rawField(state, table, name);
    int valid = 0;
    const auto number = lua_tointegerx(state, -1, &valid);
    const bool accepted = lua_type(state, -1) == LUA_TNUMBER && valid && number >= 0
        && static_cast<uint64_t>(number) <= limit;
    lua_pop(state, 1);
    if (accepted) value = static_cast<uint64_t>(number);
    return accepted;
}

bool readInput(lua_State* state, AudioInput& input) {
    if (lua_type(state, 1) != LUA_TTABLE) return false;
    rawField(state, 1, "version");
    const bool version = lua_type(state, -1) == LUA_TNUMBER && lua_tonumber(state, -1) == 1;
    lua_pop(state, 1);
    if (!version) return false;
    rawField(state, 1, "bgm");
    if (lua_isboolean(state, -1) && !lua_toboolean(state, -1)) return true;
    if (lua_type(state, -1) != LUA_TTABLE) return false;
    const int bgm = lua_gettop(state);
    rawField(state, bgm, "path");
    if (lua_type(state, -1) != LUA_TSTRING) return false;
    input.path = lua_tolstring(state, -1, &input.pathSize);
    if (!validBindingAssetPath(input.path, input.pathSize)) return false;
    rawField(state, bgm, "position");
    if (lua_type(state, -1) != LUA_TNUMBER) return false;
    input.position = lua_tonumber(state, -1);
    rawField(state, bgm, "gain");
    if (lua_type(state, -1) != LUA_TNUMBER) return false;
    const double gain = lua_tonumber(state, -1);
    rawField(state, bgm, "looping");
    if (!lua_isboolean(state, -1)) return false;
    input.looping = lua_toboolean(state, -1) != 0;
    if (!std::isfinite(input.position) || input.position < 0 || input.position > 86400
        || !std::isfinite(gain) || gain < 0 || gain > 16) return false;
    input.gain = static_cast<float>(gain);
    rawField(state, bgm, "cursor");
    if (!lua_isnil(state, -1)) {
        if (!lua_istable(state, -1)) return false;
        const int cursor = lua_gettop(state);
        uint64_t frame = 0, fraction = 0, source = 0, output = 0;
        if (!readUnsigned(state, cursor, "frame", UINT64_MAX >> 20, frame)
            || !readUnsigned(state, cursor, "fraction", (1u << 20) - 1, fraction)
            || !readUnsigned(state, cursor, "source_rate", UINT32_MAX, source)
            || !readUnsigned(state, cursor, "output_rate", UINT32_MAX, output) || !source || !output) return false;
        input.framePosition = frame;
        input.frameFraction = static_cast<uint32_t>(fraction);
        input.sourceRate = static_cast<uint32_t>(source);
        input.outputRate = static_cast<uint32_t>(output);
    }
    return true;
}

int pushAudioState(lua_State* state) {
    const auto* value = static_cast<const AudioRestoreState*>(lua_touserdata(state, 1));
    lua_createtable(state, 0, 2);
    lua_pushinteger(state, 1);
    lua_setfield(state, -2, "version");
    if (value->bgmPath.empty()) lua_pushboolean(state, false);
    else {
        lua_createtable(state, 0, 4);
        lua_pushlstring(state, value->bgmPath.data(), value->bgmPath.size());
        lua_setfield(state, -2, "path");
        lua_pushnumber(state, value->position);
        lua_setfield(state, -2, "position");
        lua_pushnumber(state, value->gain);
        lua_setfield(state, -2, "gain");
        lua_pushboolean(state, value->looping);
        lua_setfield(state, -2, "looping");
        if (value->sourceRate) {
            lua_createtable(state, 0, 4);
            lua_pushinteger(state, static_cast<lua_Integer>(value->framePosition)); lua_setfield(state, -2, "frame");
            lua_pushinteger(state, value->frameFraction); lua_setfield(state, -2, "fraction");
            lua_pushinteger(state, value->sourceRate); lua_setfield(state, -2, "source_rate");
            lua_pushinteger(state, value->outputRate); lua_setfield(state, -2, "output_rate");
            lua_setfield(state, -2, "cursor");
        }
    }
    lua_setfield(state, -2, "bgm");
    return 1;
}

int captureAudio(lua_State* state) {
    if (!lua_checkstack(state, 8)) return failure(state, "Cannot grow audio capture stack");
    char error[256] = {};
    try {
        auto* restore = BackendRegistry::instance().getAudioRestore();
        if (!restore) throw std::runtime_error("Audio restoration unavailable");
        const auto value = restore->captureAudioState();
        if (!value.bgmPath.empty() && !validBindingAssetPath(value.bgmPath.data(), value.bgmPath.size()))
            throw std::runtime_error("Audio source has no portable asset path");
        lua_pushcfunction(state, pushAudioState);
        lua_pushlightuserdata(state, const_cast<AudioRestoreState*>(&value));
        if (lua_pcall(state, 1, 1, 0) == LUA_OK) return 1;
        std::snprintf(error, sizeof(error), "%s", lua_tostring(state, -1) ? lua_tostring(state, -1) : "Audio capture allocation failed");
        lua_pop(state, 1);
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Audio capture failed");
    }
    return failure(state, error);
}

int boxAudio(lua_State* state) {
    auto* value = static_cast<std::unique_ptr<IPreparedAudioState>*>(lua_touserdata(state, 1));
    auto* box = static_cast<AudioBox*>(lua_newuserdatauv(state, sizeof(AudioBox), 0));
    box->prepared = nullptr;
    luaL_setmetatable(state, kAudioType);
    box->prepared = value->release();
    return 1;
}

int prepareAudio(lua_State* state) {
    if (!lua_checkstack(state, 12)) return failure(state, "Cannot grow audio preparation stack");
    AudioInput input;
    if (!readInput(state, input)) return failure(state, "Invalid saved audio state");
    char error[256] = {};
    try {
        auto& registry = BackendRegistry::instance();
        auto* restore = registry.getAudioRestore();
        if (!restore) throw std::runtime_error("Audio restoration unavailable");
        const AudioRestoreState value{std::string(input.path, input.pathSize),
            input.position, input.gain, input.looping, input.framePosition,
            input.frameFraction, input.sourceRate, input.outputRate};
        std::vector<uint8_t> bytes;
        if (!value.bgmPath.empty()) {
            auto* reader = registry.getAssetReader();
            if (!reader) throw std::runtime_error("Audio asset reader unavailable");
            bytes = reader->readAsset(value.bgmPath, kMaxAudioBytes);
            if (bytes.empty() || bytes.size() > kMaxAudioBytes)
                throw std::runtime_error("Cannot read required audio within the size limit");
        }
        auto prepared = restore->prepareAudioState(value, bytes.data(), bytes.size());
        if (!prepared) throw std::runtime_error("Cannot prepare required audio state");
        lua_pushcfunction(state, boxAudio);
        lua_pushlightuserdata(state, &prepared);
        if (lua_pcall(state, 1, 1, 0) == LUA_OK) return 1;
        std::snprintf(error, sizeof(error), "%s", lua_tostring(state, -1) ? lua_tostring(state, -1) : "Audio preparation allocation failed");
        lua_pop(state, 1);
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Audio preparation failed");
    }
    return failure(state, error);
}

AudioBox* getBox(lua_State* state) {
    auto* box = static_cast<AudioBox*>(luaL_testudata(state, 1, kAudioType));
    return box && lua_rawlen(state, 1) == sizeof(AudioBox) ? box : nullptr;
}

int discardAudio(lua_State* state) {
    if (auto* box = getBox(state)) delete std::exchange(box->prepared, nullptr);
    return 0;
}

int applyAudio(lua_State* state) {
    auto* box = getBox(state);
    if (!box || !box->prepared) return failure(state, "Prepared audio already consumed", true);
    char error[256] = {};
    bool applied = false;
    try {
        std::unique_ptr<IPreparedAudioState> prepared(std::exchange(box->prepared, nullptr));
        auto* restore = BackendRegistry::instance().getAudioRestore();
        if (!restore || !restore->applyAudioState(std::move(prepared)))
            throw std::runtime_error("Required audio restoration failed");
        applied = true;
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Audio restoration failed");
    }
    if (!applied) return failure(state, error, true);
    lua_pushboolean(state, true);
    return 1;
}

int stopAudio(lua_State* state) {
    char error[256] = {};
    bool stopped = false;
    try {
        auto& registry = BackendRegistry::instance();
        auto* restore = registry.getAudioRestore();
        // Standalone Lua VMs and already-detached hosts own no audio service.
        // Their lifecycle cleanup is complete without a restoration backend.
        if (!restore && !registry.getAudioBackend()) {
            lua_pushboolean(state, true);
            return 1;
        }
        if (!restore) throw std::runtime_error("Audio restoration unavailable");
        restore->stopSessionAudio();
        stopped = true;
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Audio stop failed");
    }
    if (!stopped) return failure(state, error, true);
    lua_pushboolean(state, true);
    return 1;
}
}

void registerAudioRestoreBinding(lua_State* state) {
    luaL_newmetatable(state, kAudioType);
    lua_pushcfunction(state, discardAudio);
    lua_setfield(state, -2, "__gc");
    lua_pushboolean(state, false);
    lua_setfield(state, -2, "__metatable");
    lua_pop(state, 1);
    lua_getglobal(state, "Restore");
    static const luaL_Reg functions[] = {
        {"capture_audio", captureAudio}, {"prepare_audio", prepareAudio},
        {"apply_audio", applyAudio}, {"discard_audio", discardAudio}, {"stop_audio", stopAudio},
        {nullptr, nullptr}
    };
    luaL_setfuncs(state, functions, 0);
    lua_pop(state, 1);
}
}
