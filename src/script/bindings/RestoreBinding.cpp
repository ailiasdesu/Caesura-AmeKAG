#include "RestoreBinding.h"
#include "BindingAssetPath.h"
#include "../../di/BackendRegistry.h"
#include "../../resource/api/IAssetReader.h"
#include "../../resource/api/IImageDecoder.h"
#include "../../render/api/ITextureManager.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace Caesura {
namespace {
constexpr char kImageType[] = "Caesura.RestoreImage";
constexpr size_t kMaxImageBytes = 64 * 1024 * 1024;
struct ImageTicket { uint16_t width, height; bool consumed; };
struct ImageView {
    const uint8_t* pixels;
    size_t size;
    uint16_t width, height;
    const char* path;
    size_t pathSize;
};

int failure(lua_State* state, const char* message) {
    lua_pushnil(state);
    lua_pushstring(state, message);
    return 2;
}

// This trampoline owns no C++ resources. Its Lua allocations are protected by
// pcall while the caller still owns the decoded vector. Pixels live in a Lua
// string, so discarded tickets and VM shutdown need no backend or native pool.
int pushImage(lua_State* state) {
    const auto* view = static_cast<const ImageView*>(lua_touserdata(state, 1));
    auto* ticket = static_cast<ImageTicket*>(lua_newuserdatauv(state, sizeof(ImageTicket), 2));
    *ticket = {view->width, view->height, false};
    luaL_setmetatable(state, kImageType);
    lua_pushlstring(state, reinterpret_cast<const char*>(view->pixels), view->size);
    lua_setiuservalue(state, -2, 1);
    lua_pushlstring(state, view->path, view->pathSize);
    lua_setiuservalue(state, -2, 2);
    return 1;
}

bool boxImage(lua_State* state, const ImageView& view, char (&error)[256]) {
    lua_pushcfunction(state, pushImage);
    lua_pushlightuserdata(state, const_cast<ImageView*>(&view));
    if (lua_pcall(state, 1, 1, 0) == LUA_OK) return true;
    std::snprintf(error, sizeof(error), "%s", lua_tostring(state, -1)
        ? lua_tostring(state, -1) : "Cannot allocate prepared image");
    lua_pop(state, 1);
    return false;
}

bool validPath(const char* path, size_t size) {
    return validBindingAssetPath(path, size);
}

int prepareImage(lua_State* state) {
    if (lua_type(state, 1) != LUA_TSTRING) return failure(state, "Image path must be a string");
    size_t pathSize = 0;
    const char* path = lua_tolstring(state, 1, &pathSize);
    if (!validPath(path, pathSize)) return failure(state, "Unsafe image path");
    if (!lua_checkstack(state, 8)) return failure(state, "Cannot grow image preparation stack");
    char error[256] = {};
    try {
        auto& registry = BackendRegistry::instance();
        auto* reader = registry.getAssetReader();
        auto* decoder = registry.getImageDecoder();
        if (!reader || !decoder) throw std::runtime_error("Image preparation backend unavailable");
        const auto bytes = reader->readAsset(std::string(path, pathSize), kMaxImageBytes);
        if (bytes.empty() || bytes.size() > kMaxImageBytes)
            throw std::runtime_error("Cannot read required image within the size limit");
        const auto image = decoder->decode(bytes.data(), bytes.size(), kMaxImageBytes);
        if (!image.ok || !image.width || !image.height || image.rgba.size() > kMaxImageBytes
            || image.rgba.size() != size_t(image.width) * image.height * 4)
            throw std::runtime_error("Cannot decode required image within the size limit");
        const ImageView view{image.rgba.data(), image.rgba.size(), image.width, image.height, path, pathSize};
        if (boxImage(state, view, error)) return 1;
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Image preparation failed");
    }
    return failure(state, error);
}

int prepareColor(lua_State* state) {
    uint8_t pixel[4] = {};
    for (int i = 0; i < 4; ++i) {
        int valid = 0;
        const auto value = lua_tointegerx(state, i + 1, &valid);
        if (lua_type(state, i + 1) != LUA_TNUMBER || !valid || value < 0 || value > 255)
            return failure(state, "Color components must be integers from 0 to 255");
        pixel[i] = static_cast<uint8_t>(value);
    }
    if (!lua_checkstack(state, 8)) return failure(state, "Cannot grow color preparation stack");
    const ImageView view{pixel, sizeof(pixel), 1, 1, "", 0};
    char error[256] = {};
    if (boxImage(state, view, error)) return 1;
    return failure(state, error);
}

ImageTicket* getTicket(lua_State* state) {
    auto* ticket = static_cast<ImageTicket*>(luaL_testudata(state, 1, kImageType));
    return ticket && lua_rawlen(state, 1) == sizeof(ImageTicket) ? ticket : nullptr;
}

void consume(lua_State* state, ImageTicket* ticket) {
    ticket->consumed = true;
    lua_pushnil(state);
    lua_setiuservalue(state, 1, 1);
    lua_pushnil(state);
    lua_setiuservalue(state, 1, 2);
}

int discardImage(lua_State* state) {
    if (auto* ticket = getTicket(state)) consume(state, ticket);
    return 0;
}

int imageInfo(lua_State* state) {
    auto* ticket = getTicket(state);
    if (!ticket || ticket->consumed) return failure(state, "Prepared image is unavailable");
    lua_pushinteger(state, ticket->width);
    lua_pushinteger(state, ticket->height);
    return 2;
}

int materializeImage(lua_State* state) {
    auto* ticket = getTicket(state);
    if (!ticket || ticket->consumed) return failure(state, "Prepared image is unavailable");
    lua_getiuservalue(state, 1, 1);
    lua_getiuservalue(state, 1, 2);
    size_t size = 0, pathSize = 0;
    const char* pixels = lua_tolstring(state, -2, &size);
    const char* path = lua_tolstring(state, -1, &pathSize);
    // Stack references keep strings alive while consuming the ticket. Every
    // attempt releases its preparation, including backend rejection/exception.
    consume(state, ticket);
    if (!pixels || !path || size != size_t(ticket->width) * ticket->height * 4)
        return failure(state, "Invalid prepared image storage");
    char error[256] = {};
    uint32_t id = 0;
    try {
        auto* textures = BackendRegistry::instance().getTextureManager();
        if (!textures) throw std::runtime_error("Texture backend unavailable");
        id = textures->loadTextureFromRGBA(reinterpret_cast<const uint8_t*>(pixels),
            ticket->width, ticket->height, std::string(path, pathSize));
        if (!id) throw std::runtime_error("Cannot materialize required image");
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Image materialization failed");
    }
    if (!id) return failure(state, error);
    lua_pushinteger(state, id);
    return 1;
}

int pushSource(lua_State* state) {
    const auto* source = static_cast<const TextureSourceInfo*>(lua_touserdata(state, 1));
    lua_createtable(state, 0, 5);
    lua_pushstring(state, source->kind == TextureSourceKind::Asset ? "asset" : "color");
    lua_setfield(state, -2, "kind");
    if (source->kind == TextureSourceKind::Asset) {
        lua_pushlstring(state, source->path.data(), source->path.size());
        lua_setfield(state, -2, "path");
    } else {
        const char* keys[] = {"r", "g", "b", "a"};
        for (size_t i = 0; i < 4; ++i) {
            lua_pushinteger(state, source->color[i]);
            lua_setfield(state, -2, keys[i]);
        }
    }
    return 1;
}

int describeTexture(lua_State* state) {
    int valid = 0;
    const auto id = lua_tointegerx(state, 1, &valid);
    if (lua_type(state, 1) != LUA_TNUMBER || !valid || id <= 0 || id > UINT32_MAX)
        return failure(state, "Invalid texture ID");
    if (!lua_checkstack(state, 8)) return failure(state, "Cannot grow texture description stack");
    char error[256] = {};
    try {
        auto* textures = BackendRegistry::instance().getTextureManager();
        TextureSourceInfo source;
        if (!textures || !textures->describeTexture(static_cast<uint32_t>(id), source))
            throw std::runtime_error("Texture has no restorable origin");
        if (source.kind == TextureSourceKind::Asset && !validPath(source.path.data(), source.path.size()))
            throw std::runtime_error("Texture origin is not a portable asset path");
        lua_pushcfunction(state, pushSource);
        lua_pushlightuserdata(state, &source);
        if (lua_pcall(state, 1, 1, 0) == LUA_OK) return 1;
        std::snprintf(error, sizeof(error), "%s", lua_tostring(state, -1)
            ? lua_tostring(state, -1) : "Cannot allocate texture description");
        lua_pop(state, 1);
    } catch (const std::exception& cause) {
        std::snprintf(error, sizeof(error), "%s", cause.what());
    } catch (...) {
        std::snprintf(error, sizeof(error), "Cannot describe texture");
    }
    return failure(state, error);
}
}

void registerRestoreBinding(lua_State* state) {
    luaL_newmetatable(state, kImageType);
    lua_pushboolean(state, false);
    lua_setfield(state, -2, "__metatable");
    lua_pop(state, 1);
    static const luaL_Reg functions[] = {
        {"prepare_image", prepareImage}, {"prepare_color", prepareColor},
        {"image_info", imageInfo}, {"materialize_image", materializeImage},
        {"discard_image", discardImage}, {"describe_texture", describeTexture},
        {nullptr, nullptr}
    };
    luaL_newlib(state, functions);
    lua_setglobal(state, "Restore");
}
}
