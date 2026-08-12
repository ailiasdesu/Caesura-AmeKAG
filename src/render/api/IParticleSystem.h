#pragma once
#include <cstdint>

namespace Caesura {

struct ParticleEmitterConfig {
    float x = 0.0f;
    float y = 0.0f;
    float rate = 10.0f;
    float lifeMin = 0.5f;
    float lifeMax = 2.0f;
    float speedMin = 10.0f;
    float speedMax = 50.0f;
    float angleMin = 0.0f;
    float angleMax = 6.283f;
    float sizeMin = 2.0f;
    float sizeMax = 8.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    float gravityX = 0.0f;
    float gravityY = 0.0f;
};

// ============================================================================
// IParticleSystem — pure virtual interface for particle effects
// ============================================================================
// ParticleSystem implements this interface. BackendRegistry stores IParticleSystem*.

class IParticleSystem {
public:
    virtual ~IParticleSystem() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual int  createEmitter(const ParticleEmitterConfig& cfg) = 0;
    virtual bool destroyEmitter(int id) = 0;
    virtual void emit(int emitterId, int count) = 0;

    virtual void update(float dt, uint32_t screenW, uint32_t screenH) = 0;
    virtual void render(uint16_t viewId) = 0;

    virtual int aliveCount() const = 0;
    virtual bool isInitialized() const = 0;
};

} // namespace Caesura
