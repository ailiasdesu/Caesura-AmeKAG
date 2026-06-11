#pragma once
#include <bgfx/bgfx.h>
#include <cstdint>

namespace Caesura {

struct Emitter;

// ============================================================================
// IParticleSystem — pure virtual interface for particle effects
// ============================================================================
// ParticleSystem implements this interface. BackendRegistry stores IParticleSystem*.

class IParticleSystem {
public:
    virtual ~IParticleSystem() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual int  createEmitter(const Emitter& cfg) = 0;
    virtual void destroyEmitter(int id) = 0;
    virtual void emit(int emitterId, int count) = 0;

    virtual void update(float dt, uint32_t screenW, uint32_t screenH) = 0;
    virtual void render(uint16_t viewId) = 0;

    virtual int aliveCount() const = 0;
};

} // namespace Caesura
