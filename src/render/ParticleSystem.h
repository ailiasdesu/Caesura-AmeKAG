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
    bool  alive = false;
};

struct Emitter {
    float x = 0, y = 0;
    float rate = 10.0f;
    float lifeMin = 0.5f, lifeMax = 2.0f;
    float speedMin = 10.0f, speedMax = 50.0f;
    float angleMin = 0.0f, angleMax = 6.283f;
    float sizeMin = 2.0f, sizeMax = 8.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    float gravityX = 0.0f, gravityY = 0.0f;
    bool  active = true;
    float timer = 0.0f;
};

struct SimBatch {
    Particle* particles = nullptr;
    uint32_t  startIdx  = 0;
    uint32_t  endIdx    = 0;
    float     dt        = 0.0f;
    float     gravityX  = 0.0f;
    float     gravityY  = 0.0f;
    int       deadCount = 0;
};

void processSimBatch(SimBatch& batch);

// ============================================================================
// ParticleSystem -- implements IParticleSystem
// ============================================================================

class ParticleSystem : public IParticleSystem {
public:
    static constexpr int MAX_PARTICLES = 1024;
    static constexpr int SIM_BATCH_SIZE = 256;

    ParticleSystem() = default;
    ~ParticleSystem() override;

    bool init() override;
    void shutdown() override;

    int  createEmitter(const Emitter& cfg) override;
    void destroyEmitter(int id) override;
    void emit(int emitterId, int count) override;

    void update(float dt, uint32_t screenW, uint32_t screenH) override;
    void render(uint16_t viewId) override;

    int aliveCount() const override { return m_aliveCount; }

private:
    std::vector<Particle> m_particles;
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