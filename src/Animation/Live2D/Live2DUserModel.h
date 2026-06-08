#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include <Model/CubismUserModel.hpp>

namespace Caesura {

// Bridge: expose protected _motionManager / _expressionManager
struct Live2DUserModel : public Live2D::Cubism::Framework::CubismUserModel {
    auto* motionManager()     { return _motionManager; }
    auto* expressionManager() { return _expressionManager; }
};

} // namespace Caesura

#endif
