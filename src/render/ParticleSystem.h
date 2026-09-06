#pragma once
#include "api/IParticleSystem.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <cstdint>

namespace Caesura {

struct Particle {
    float x, y;
    float vx, vy;
    float life, maxLife;
    float size;
    float r, g, b, a;
    int   emitterId = -1;  // owning emitter for per-emitter gravity
    bool  alive = false;
};

struct Emitter : ParticleEmitterConfig {
    Emitter() = default;
    explicit Emitter(const ParticleEmitterConfig& config)
        : ParticleEmitterConfig(config) {}

    bool  active = true;
    float timer = 0.0f;
};

// ============================================================================
// ParticleSystem -- implements IParticleSystem
// ============================================================================

class ParticleSystem : public IParticleSystem {
public:
    static constexpr int MAX_PARTICLES = 1024;

    ParticleSystem() = default;
    ~ParticleSystem() override;

    bool init() override;
    void shutdown() override;

    int  createEmitter(const ParticleEmitterConfig& cfg) override;
    bool destroyEmitter(int id) override;
    void emit(int emitterId, int count) override;

    void update(float dt, uint32_t screenW, uint32_t screenH) override;
    void render(uint16_t viewId) override;

    int aliveCount() const override { return m_aliveCount; }
    int activeEmitterCount() const override;
    bool isInitialized() const override { return m_initialized; }

public:
    // -- Pure particle visual math (headless-testable) ---------------------
    // Pixel-space quad + life-faded color/alpha for one particle. Extracted
    // from render() so the decay curve and quad math can be pinned without a
    // GPU (G8). lifeFade = life/maxLife in [0,1]; a zero/negative maxLife
    // degenerates to 1.0 (full size/alpha) instead of NaN.
    struct ParticleQuad {
        float x0, y0, x1, y1;   // pixel-space corners (half-extent box)
        uint8_t r, g, b, a;     // color with life-faded alpha
    };
    static float lifeFade(const Particle& p);
    static ParticleQuad buildParticleVisual(const Particle& p);

private:
    std::vector<Particle> m_particles;
    // Free-slot stack: O(1) slot acquisition instead of a linear scan.
    std::vector<int> m_freeSlots;
    std::vector<Emitter>  m_emitters;
    int m_aliveCount = 0;

    bgfx::VertexLayout   m_layout;
    bgfx::ProgramHandle  m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle  m_texSampler = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle  m_particleTex = BGFX_INVALID_HANDLE;
    uint32_t m_screenW = 1280;
    uint32_t m_screenH = 720;
    bool m_initialized = false;
};

} // namespace Caesura
