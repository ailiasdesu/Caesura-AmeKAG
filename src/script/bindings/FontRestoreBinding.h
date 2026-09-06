#pragma once
#include "../../render/api/IRenderDevice.h"
#include <memory>
struct lua_State;

namespace Caesura {
void registerFontRestoreBinding(lua_State* state);
// Shared by normal script font selection and restore preparation. Only this
// binding layer reads resources; the render module receives owned CPU input.
std::unique_ptr<IPreparedFontState> prepareFontAsset(
    IRenderDevice& device, const FontRestoreState& description);
bool selectScriptFont(IRenderDevice& device, const std::string& face, float pixelSize);
}
