#include "FontRestoreBinding.h"
#include "BindingAssetPath.h"
#include "../../di/BackendRegistry.h"
#include "../../resource/api/IAssetReader.h"
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
constexpr char kFontType[] = "Caesura.PreparedFont";
constexpr size_t kMaxFontBytes = 32 * 1024 * 1024;
struct FontBox { IPreparedFontState* prepared; };
class InactiveFont final : public IPreparedFontState {
public:
    const FontRestoreState& description() const override { return value; }
private:
    FontRestoreState value;
};
struct FontInput { bool active=false; int font=0; const char* path=""; size_t pathSize=0; float size=0; };

int failure(lua_State* state, const char* message, bool boolean=false) {
    if (boolean) lua_pushboolean(state,false); else lua_pushnil(state);
    lua_pushstring(state,message && message[0] ? message : "Font restoration failed");
    return 2;
}

void rawField(lua_State* state, int index, const char* key) {
    index=lua_absindex(state,index);
    lua_pushstring(state,key);
    lua_rawget(state,index);
}

bool readInput(lua_State* state, FontInput& input) {
    if (lua_type(state,1)!=LUA_TTABLE) return false;
    rawField(state,1,"version");
    if (lua_type(state,-1)!=LUA_TNUMBER || lua_tonumber(state,-1)!=1) return false;
    rawField(state,1,"active");
    if (!lua_isboolean(state,-1)) return false;
    input.active=lua_toboolean(state,-1)!=0;
    if (!input.active) return true;
    rawField(state,1,"font");
    int integer=0;
    const auto font=lua_tointegerx(state,-1,&integer);
    if (lua_type(state,-1)!=LUA_TNUMBER || !integer || font<0 || font>2) return false;
    input.font=static_cast<int>(font);
    rawField(state,1,"path");
    if (lua_type(state,-1)!=LUA_TSTRING) return false;
    input.path=lua_tolstring(state,-1,&input.pathSize);
    rawField(state,1,"size");
    const double size=lua_tonumber(state,-1);
    if (lua_type(state,-1)!=LUA_TNUMBER || !std::isfinite(size) || std::floor(size)!=size
        || size<1 || size>256) return false;
    input.size=static_cast<float>(size);
    if (input.font==2) return validBindingAssetPath(input.path,input.pathSize);
    return input.pathSize==0 && size==(input.font==0 ? 16 : 32);
}

int pushState(lua_State* state) {
    const auto* value=static_cast<const FontRestoreState*>(lua_touserdata(state,1));
    lua_createtable(state,0,5);
    lua_pushinteger(state,1); lua_setfield(state,-2,"version");
    lua_pushboolean(state,value->active); lua_setfield(state,-2,"active");
    if (value->active) {
        lua_pushinteger(state,static_cast<int>(value->font)); lua_setfield(state,-2,"font");
        lua_pushlstring(state,value->assetPath.data(),value->assetPath.size()); lua_setfield(state,-2,"path");
        lua_pushnumber(state,value->pixelSize); lua_setfield(state,-2,"size");
    }
    return 1;
}

int capture(lua_State* state, bool defaults) {
    if (!lua_checkstack(state,8)) return failure(state,"Cannot grow font capture stack");
    char error[256]={};
    try {
        auto* device=BackendRegistry::instance().getRenderDevice();
        const auto value=device ? (defaults ? device->defaultFontState() : device->captureFontState())
                                : FontRestoreState{};
        if (value.active && value.font==FontId::TTF
            && !validBindingAssetPath(value.assetPath.data(),value.assetPath.size()))
            throw std::runtime_error("Active font has no portable asset path");
        lua_pushcfunction(state,pushState);
        lua_pushlightuserdata(state,const_cast<FontRestoreState*>(&value));
        if (lua_pcall(state,1,1,0)==LUA_OK) return 1;
        std::snprintf(error,sizeof(error),"%s",lua_tostring(state,-1) ? lua_tostring(state,-1) : "Font capture allocation failed");
        lua_pop(state,1);
    } catch (const std::exception& cause) { std::snprintf(error,sizeof(error),"%s",cause.what()); }
      catch (...) { std::snprintf(error,sizeof(error),"Font capture failed"); }
    return failure(state,error);
}
int captureFont(lua_State* state) { return capture(state,false); }
int defaultFont(lua_State* state) { return capture(state,true); }

int pushBox(lua_State* state) {
    auto* prepared=static_cast<std::unique_ptr<IPreparedFontState>*>(lua_touserdata(state,1));
    auto* box=static_cast<FontBox*>(lua_newuserdatauv(state,sizeof(FontBox),0));
    box->prepared=nullptr;
    luaL_setmetatable(state,kFontType);
    box->prepared=prepared->release();
    return 1;
}

int prepareFont(lua_State* state) {
    if (!lua_checkstack(state,12)) return failure(state,"Cannot grow font preparation stack");
    FontInput input;
    if (!readInput(state,input)) return failure(state,"Invalid saved font state");
    char error[256]={};
    try {
        auto* device=BackendRegistry::instance().getRenderDevice();
        if (!device && input.active) throw std::runtime_error("Font renderer unavailable");
        const FontRestoreState description{input.active,static_cast<FontId>(input.font),
            std::string(input.path,input.pathSize),input.size};
        std::unique_ptr<IPreparedFontState> prepared=input.active
            ? prepareFontAsset(*device,description) : std::make_unique<InactiveFont>();
        if (!prepared) throw std::runtime_error("Cannot prepare required font");
        lua_pushcfunction(state,pushBox);
        lua_pushlightuserdata(state,&prepared);
        if (lua_pcall(state,1,1,0)==LUA_OK) return 1;
        std::snprintf(error,sizeof(error),"%s",lua_tostring(state,-1) ? lua_tostring(state,-1) : "Font preparation allocation failed");
        lua_pop(state,1);
    } catch (const std::exception& cause) { std::snprintf(error,sizeof(error),"%s",cause.what()); }
      catch (...) { std::snprintf(error,sizeof(error),"Font preparation failed"); }
    return failure(state,error);
}

FontBox* getBox(lua_State* state) {
    auto* box=static_cast<FontBox*>(luaL_testudata(state,1,kFontType));
    return box && lua_rawlen(state,1)==sizeof(FontBox) ? box : nullptr;
}
int discardFont(lua_State* state) {
    if (auto* box=getBox(state)) delete std::exchange(box->prepared,nullptr);
    return 0;
}

int applyFont(lua_State* state) {
    auto* box=getBox(state);
    if (!box || !box->prepared) return failure(state,"Prepared font already consumed",true);
    char error[256]={};
    bool applied=false;
    try {
        std::unique_ptr<IPreparedFontState> prepared(std::exchange(box->prepared,nullptr));
        auto* device=BackendRegistry::instance().getRenderDevice();
        if (!prepared->description().active) {
            if (device) device->clearFontState();
        } else if (!device || !device->applyFontState(std::move(prepared))) {
            throw std::runtime_error("Required font upload failed");
        }
        applied=true;
    } catch (const std::exception& cause) { std::snprintf(error,sizeof(error),"%s",cause.what()); }
      catch (...) { std::snprintf(error,sizeof(error),"Font upload failed"); }
    if (!applied) return failure(state,error,true);
    lua_pushboolean(state,true);
    return 1;
}

int clearFont(lua_State* state) {
    char error[256]={};
    bool cleared=false;
    try {
        if (auto* device=BackendRegistry::instance().getRenderDevice()) device->clearFontState();
        cleared=true;
    } catch (const std::exception& cause) { std::snprintf(error,sizeof(error),"%s",cause.what()); }
      catch (...) { std::snprintf(error,sizeof(error),"Font cleanup failed"); }
    if (!cleared) return failure(state,error,true);
    lua_pushboolean(state,true);
    return 1;
}
}

std::unique_ptr<IPreparedFontState> prepareFontAsset(
    IRenderDevice& device, const FontRestoreState& description) {
    std::vector<uint8_t> bytes;
    if (description.active && description.font==FontId::TTF) {
        if (!validBindingAssetPath(description.assetPath.data(),description.assetPath.size())) return {};
        auto* reader=BackendRegistry::instance().getAssetReader();
        if (!reader) return {};
        bytes=reader->readAsset(description.assetPath,kMaxFontBytes);
        if (bytes.empty() || bytes.size()>kMaxFontBytes) return {};
    }
    return device.prepareFontState(description,bytes.data(),bytes.size());
}

bool selectScriptFont(IRenderDevice& device, const std::string& name, float pixelSize) {
    try {
        if (name=="bitmap" || name=="builtin") {
            auto prepared=prepareFontAsset(device,{true,FontId::Small,"",16});
            return prepared && device.applyFontState(std::move(prepared));
        }
        if (!std::isfinite(pixelSize) || pixelSize<1 || pixelSize>256) return false;
        const std::string path=(name.empty() || name=="default")
            ? "assets/fonts/NotoSansCJKsc-Regular.otf" : name;
        auto prepared=prepareFontAsset(device,{true,FontId::TTF,path,std::floor(pixelSize)});
        return prepared && device.applyFontState(std::move(prepared));
    } catch (...) { return false; }
}

void registerFontRestoreBinding(lua_State* state) {
    luaL_newmetatable(state,kFontType);
    lua_pushcfunction(state,discardFont); lua_setfield(state,-2,"__gc");
    lua_pushboolean(state,false); lua_setfield(state,-2,"__metatable");
    lua_pop(state,1);
    lua_getglobal(state,"Restore");
    static const luaL_Reg functions[]={
        {"capture_font",captureFont},{"default_font",defaultFont},{"prepare_font",prepareFont},
        {"apply_font",applyFont},{"discard_font",discardFont},{"clear_font",clearFont},
        {nullptr,nullptr}
    };
    luaL_setfuncs(state,functions,0);
    lua_pop(state,1);
}
}
