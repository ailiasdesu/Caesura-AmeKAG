#include "ParticleSystem.h"
#include "BgfxRenderDevice.h"
#include "../Core/BackendRegistry.h"
#include "../Core/JobSystem.h"
#include <bx/math.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>

namespace Caesura {
// Security: replaced rand() with std::mt19937 for proper RNG

// ===========================================================================
//  processSimBatch -- JobSystem worker payload (pure CPU, no GPU/alloc)
// ===========================================================================

void processSimBatch(SimBatch& batch) {
    int dead = 0;
    for (uint32_t i = batch.startIdx; i < batch.endIdx; ++i) {
        auto& p = batch.particles[i];
        if (!p.alive) continue;

        p.life -= batch.dt;
        if (p.life <= 0.0f) {
            p.alive = false;
            ++dead;
            continue;
        }

        p.vx += batch.gravityX * batch.dt;
        p.vy += batch.gravityY * batch.dt;
        p.x  += p.vx * batch.dt;
        p.y  += p.vy * batch.dt;
    }
    batch.deadCount = dead;
}

static std::mt19937& rng() { static std::mt19937 r(std::random_device{}()); return r; }

ParticleSystem::~ParticleSystem() { shutdown(); }

bool ParticleSystem::init() {
        auto* renderDev = BackendRegistry::instance().getRenderDevice();
    if (!renderDev) return false;
    m_particles.resize(MAX_PARTICLES);

    // Get bgfx-specific handles from the concrete BgfxRenderDevice (only impl)
    auto* bgfxDev = dynamic_cast<BgfxRenderDevice*>(renderDev);
    if (!bgfxDev) return false;
    m_texSampler = bgfxDev->getTexSampler();
    m_program = bgfxDev->getFallbackProgram();

    // Create a 4x4 white point texture for particles
    uint8_t white[16] = { 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255 };
    const bgfx::Memory* mem = bgfx::copy(white, 16);
    m_particleTex = bgfx::createTexture2D(2, 2, false, 1,
        bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_POINT, mem);

    m_layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();m_initialized = true;
    printf("[ParticleSystem] Initialized (max %d particles)\n", MAX_PARTICLES);
    return true;
}

void ParticleSystem::shutdown() {
    if (bgfx::isValid(m_particleTex)) {
        bgfx::destroy(m_particleTex);
        m_particleTex = BGFX_INVALID_HANDLE;
    }
    m_initialized = false;
}

int ParticleSystem::createEmitter(const Emitter& cfg) {
    int id = (int)m_emitters.size();
    m_emitters.push_back(cfg);
    printf("[ParticleSystem] Emitter %d created\n", id);
    return id;
}

void ParticleSystem::destroyEmitter(int id) {
    if (id >= 0 && id < (int)m_emitters.size())
        m_emitters[id].active = false;
}

void ParticleSystem::emit(int emitterId, int count) {
    if (emitterId < 0 || emitterId >= (int)m_emitters.size()) return;
    auto& em = m_emitters[emitterId];
    if (!em.active) return;

    for (int n = 0; n < count; n++) {
        int slot = -1;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!m_particles[i].alive) { slot = i; break; }
        }
        if (slot < 0) break; // pool full

        float angle = em.angleMin + (float)std::uniform_real_distribution<float>(0.0f, 1.0f)(rng()) * (em.angleMax - em.angleMin);
        float speed = em.speedMin + (float)std::uniform_real_distribution<float>(0.0f, 1.0f)(rng()) * (em.speedMax - em.speedMin);
        float life  = em.lifeMin + (float)std::uniform_real_distribution<float>(0.0f, 1.0f)(rng()) * (em.lifeMax - em.lifeMin);

        auto& p = m_particles[slot];
        p.x = em.x; p.y = em.y;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.life = life; p.maxLife = life;
        p.size = em.sizeMin + (float)std::uniform_real_distribution<float>(0.0f, 1.0f)(rng()) * (em.sizeMax - em.sizeMin);
        p.r = em.r; p.g = em.g; p.b = em.b; p.a = em.a;
        p.alive = true;
        m_aliveCount++;
    }
}

void ParticleSystem::update(float dt, uint32_t screenW, uint32_t screenH) {
    if (!m_initialized) return;
    m_screenW = screenW;
    m_screenH = screenH;

    for (auto& em : m_emitters) {
        if (!em.active) continue;
        em.timer += dt;
        float rate = em.rate < 0.1f ? 0.1f : em.rate;
        while (em.timer >= 1.0f / rate) {
            em.timer -= 1.0f / em.rate;
            emit((int)(&em - m_emitters.data()), 1);
        }
    }

    // Update particles
    for (auto& p : m_particles) {
        if (!p.alive) continue;
        p.life -= dt;
        if (p.life <= 0) {
            p.alive = false;
            m_aliveCount--;
            continue;
        }
        // Find emitter for gravity
        for (auto& em : m_emitters) {
            if (em.active) {
                p.vx += em.gravityX * dt;
                p.vy += em.gravityY * dt;
                break;
            }
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
}

void ParticleSystem::render(uint16_t viewId) {
    if (!m_initialized || m_aliveCount <= 0) return;
    if (!bgfx::isValid(m_program)) return;

    struct PtVertex { float x, y, u, v; uint8_t r, g, b, a; };
    int maxVerts = m_aliveCount * 4;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, (uint32_t)maxVerts, m_layout);
    if (tvb.size < (uint32_t)maxVerts) {
        fprintf(stderr, "[ParticleSystem] render: transient VB alloc failed (need %d, got %d)\n", maxVerts, (int)tvb.size);
        return;
    }
    auto* vtx = (PtVertex*)tvb.data;

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, (uint32_t)(m_aliveCount * 6));
    if (tib.size < (uint32_t)(m_aliveCount * 6)) {
        fprintf(stderr, "[ParticleSystem] render: transient IB alloc failed\n");
        return;
    }
    auto* idx = (uint16_t*)tib.data;

    int vi = 0, ii = 0;
    for (auto& p : m_particles) {
        if (!p.alive) continue;
        float t = p.life / p.maxLife;
        float s = p.size * t;
        float hs = s * 0.5f;
        uint8_t cr = (uint8_t)(p.r * 255.0f);
        uint8_t cg = (uint8_t)(p.g * 255.0f);
        uint8_t cb = (uint8_t)(p.b * 255.0f);
        uint8_t ca = (uint8_t)(p.a * t * 255.0f);

        vtx[vi+0] = { p.x - hs, p.y - hs, 0.0f, 0.0f, cr, cg, cb, ca };
        vtx[vi+1] = { p.x + hs, p.y - hs, 1.0f, 0.0f, cr, cg, cb, ca };
        vtx[vi+2] = { p.x + hs, p.y + hs, 1.0f, 1.0f, cr, cg, cb, ca };
        vtx[vi+3] = { p.x - hs, p.y + hs, 0.0f, 1.0f, cr, cg, cb, ca };

        uint16_t base = (uint16_t)vi;
        idx[ii+0]=base; idx[ii+1]=base+1; idx[ii+2]=base+2;
        idx[ii+3]=base; idx[ii+4]=base+2; idx[ii+5]=base+3;
        vi+=4; ii+=6;
    }

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                           BGFX_STATE_BLEND_INV_SRC_ALPHA);

    float ortho[16];
    const bgfx::Caps* caps = bgfx::getCaps();
    bx::mtxOrtho(ortho, 0.0f, (float)m_screenW, (float)m_screenH, 0.0f, -1.0f, 1.0f, 0.0f,
                 caps ? caps->homogeneousDepth : false, bx::Handedness::Left);
    bgfx::setViewTransform(viewId, nullptr, ortho);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, m_texSampler, m_particleTex);
    bgfx::setState(state);
    bgfx::submit(viewId, m_program);
}

} // namespace Caesura