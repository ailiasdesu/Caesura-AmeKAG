#pragma once
#include <bgfx/bgfx.h>
#include <vector>
#include <cstdint>

namespace Caesura {

struct Particle {
    float x, y;          // position
    float vx, vy;        // velocity
    float life, maxLife; // lifetime (seconds)
    float size;
    float r, g, b, a;    // color
    bool  alive = false;
};

struct Emitter {
    float x = 0, y = 0;
    float rate = 10.0f;        // particles per second
    float lifeMin = 0.5f, lifeMax = 2.0f;
    float speedMin = 10.0f, speedMax = 50.0f;
    float angleMin = 0.0f, angleMax = 6.283f;  // radians
    float sizeMin = 2.0f, sizeMax = 8.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    float gravityX = 0.0f, gravityY = 0.0f;
    bool  active = true;
    float timer = 0.0f;
};

class ParticleSystem {
public:
    static constexpr int MAX_PARTICLES = 1024;

    ParticleSystem() = default;
    ~ParticleSystem();

    bool init();
    void shutdown();

    int  createEmitter(const Emitter& cfg);
    void destroyEmitter(int id);
    void emit(int emitterId, int count);

    void update(float dt, uint32_t screenW, uint32_t screenH);
    void render(uint16_t viewId);

    // Access
    int  aliveCount() const { return m_aliveCount; }

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