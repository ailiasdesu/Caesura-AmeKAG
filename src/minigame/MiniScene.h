#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "MiniGeometry.h"

namespace Caesura {
struct MiniObject {
    uint32_t id;
    float posX = 0, posY = 0, posZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    float rotX = 0, rotY = 0, rotZ = 0;
    MiniGeoType geoType = MiniGeoType::Cube;
    uint32_t materialId = 0;
    float r = 1, g = 1, b = 1;
    bool enableCollision = true;
    float velX = 0, velY = 0, velZ = 0;
    float accelX = 0, accelY = 0, accelZ = 0;
    bool useGravity = false;
};


// ===========================================================================
// MiniScene — parsed 3D scene descriptor for BgfxMiniGameBackend
// ===========================================================================
// A scene is a self-contained description loaded from a JSON file:
//   {
//     "name": "demo",
//     "camera": { "eye": [0,3,8], "at": [0,0,0] },
//     "lights": { "ambient": [0.3,0.3,0.3],
//                 "directional": { "dir": [0,-1,-0.5], "color": [1,1,1], "intensity": 0.8 } },
//     "objects": [ { "type": "cube", "pos": [0,0,0], "scale": 1,
//                    "color": [0.8,0.2,0.2], "gravity": false } ]
//   }
// loadScene() parses this file; enter(handle) spawns the objects and applies
// the camera/lighting setup.

struct MiniSceneLightSetup {
    float ambient[3] = { 0.25f, 0.25f, 0.3f };
    bool  hasDirectional = false;
    float dir[3] = { 0.0f, -1.0f, -0.5f };
    float dirColor[3] = { 1.0f, 1.0f, 1.0f };
    float dirIntensity = 0.8f;
};

struct MiniScene {
    uint32_t id = 0;
    std::string name;
    float eyeX = 0, eyeY = 3, eyeZ = 8;
    float atX = 0, atY = 0, atZ = 0;
    MiniSceneLightSetup lights;
    std::vector<MiniObject> objects;
};

} // namespace Caesura