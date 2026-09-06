#include "ParticleSystem.h"
#include "../di/BackendRegistry.h"
#include "../render/api/IRenderDevice.h"
#include "../debug/api/DebugLog.h"   // P1-6: api header instead of concrete DebugManager.h
#include <bx/math.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>

namespace Caesura {
// Security: replaced rand() with std::mt19937 for proper RNG

namespace {

bgfx::ProgramHandle toBgfx(RenderProgramHandle handle) {
    if (!handle.isValid()) return BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle result;
    result.idx = handle.idx;
    return result;
}

bgfx::UniformHandle toBgfx(RenderUniformHandle handle) {
    if (!handle.isValid()) return BGFX_INVALID_HANDLE;
    bgfx::UniformHandle result;
    result.idx = handle.idx;
    return result;
}

} // namespace

static std::mt19937& rng() { static std::mt19937 r(std::random_device{}()); return r; }

ParticleSystem::~ParticleSystem() { shutdown(); }

bool ParticleSystem::init() {
        auto* renderDev = BackendRegistry::instance().getRenderDevice();
    if (!renderDev) return false;
    m_particles.resize(MAX_PARTICLES);

    // Get handles from IRenderDevice abstraction (no concrete dependency)
    m_texSampler = toBgfx(renderDev->getDefaultSampler());
    m_program = toBgfx(renderDev->getFallbackProgram());
    if (!bgfx::isValid(m_texSampler) || !bgfx::isValid(m_program)) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[ParticleSystem] Render device missing sampler or program");
        return false;
    }
    uint8_t white[16] = { 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255 };
    const bgfx::Memory* mem = bgfx::copy(white, 16);
    m_particleTex = bgfx::createTexture2D(2, 2, false, 1,
        bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_POINT, mem);

    m_layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    m_freeSlots.clear();
    m_freeSlots.reserve(MAX_PARTICLES);
    for (int i = MAX_PARTICLES - 1; i >= 0; --i) m_freeSlots.push_back(i);
    m_initialized = true;
    printf("[ParticleSystem] Initialized (max %d particles)\n", MAX_PARTICLES);
    return true;
}

void ParticleSystem::shutdown() {
    if (bgfx::isValid(m_particleTex)) {
        bgfx::destroy(m_particleTex);
        m_particleTex = BGFX_INVALID_HANDLE;
    }
    m_emitters.clear();
    m_particles.clear();
    m_freeSlots.clear();
    m_aliveCount = 0;
    m_initialized = false;
}

int ParticleSystem::createEmitter(const ParticleEmitterConfig& cfg) {
    int id = (int)m_emitters.size();
    m_emitters.emplace_back(cfg);
    printf("[ParticleSystem] Emitter %d created\n", id);
    return id;
}

int ParticleSystem::activeEmitterCount() const {
    int count = 0;
    for (const auto& emitter : m_emitters) if (emitter.active) ++count;
    return count;
}

bool ParticleSystem::destroyEmitter(int id) {
    if (id < 0 || id >= (int)m_emitters.size() || !m_emitters[id].active) {
        return false;
    }
    m_emitters[id].active = false;
    return true;
}

// ---------------------------------------------------------------------------
// Pure particle visual math (GPU-free) -- extracted from render() so the
// decay curve and quad math are unit-testable (G8).
// ---------------------------------------------------------------------------

float ParticleSystem::lifeFade(const Particle& p) {
    // Life fraction in [0,1]; guard against zero/negative maxLife (which
    // would otherwise produce NaN and poison every downstream vertex).
    if (p.maxLife <= 0.0f) return 1.0f;
    const float t = p.life / p.maxLife;
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

ParticleSystem::ParticleQuad ParticleSystem::buildParticleVisual(const Particle& p) {
    const float t = lifeFade(p);
    const float s = p.size * t;          // quad shrinks as life decays
    const float hs = s * 0.5f;
    ParticleQuad q;
    q.x0 = p.x - hs; q.y0 = p.y - hs;
    q.x1 = p.x + hs; q.y1 = p.y + hs;
    q.r = (uint8_t)(p.r * 255.0f);
    q.g = (uint8_t)(p.g * 255.0f);
    q.b = (uint8_t)(p.b * 255.0f);
    q.a = (uint8_t)(p.a * t * 255.0f);   // alpha fades with life too
    return q;
}

void ParticleSystem::emit(int emitterId, int count) {
    if (!m_initialized) return;
    if (emitterId < 0 || emitterId >= (int)m_emitters.size()) return;
    auto& em = m_emitters[emitterId];
    // Reuse one distribution object across the whole burst (P2-6: the
    // per-particle construction was pure overhead - the distribution is
    // stateless w.r.t. its parameters and re-entrant on the engine thread).
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    for (int n = 0; n < count; n++) {
        if (m_freeSlots.empty()) break;  // pool full
        const int slot = m_freeSlots.back();
        m_freeSlots.pop_back();

        float angle = em.angleMin + (float)unit(rng()) * (em.angleMax - em.angleMin);
        float speed = em.speedMin + (float)unit(rng()) * (em.speedMax - em.speedMin);
        float life  = em.lifeMin + (float)unit(rng()) * (em.lifeMax - em.lifeMin);
        auto& p = m_particles[slot];
        p.x = em.x; p.y = em.y;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.life = life; p.maxLife = life;
        p.size = em.sizeMin + (float)std::uniform_real_distribution<float>(0.0f, 1.0f)(rng()) * (em.sizeMax - em.sizeMin);
        p.r = em.r; p.g = em.g; p.b = em.b; p.a = em.a;
        p.emitterId = emitterId;
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
            m_freeSlots.push_back(static_cast<int>(&p - m_particles.data()));
            continue;
        }
        // Per-emitter gravity: a particle keeps the gravity of the emitter
        // that spawned it (previously every particle used the first active
        // emitter's gravity; after that emitter died the wrong gravity drove
        // all particles). Falls back to the first active emitter if the
        // owning one was destroyed.
        int g = p.emitterId;
        if (g < 0 || g >= (int)m_emitters.size() || !m_emitters[g].active) {
            g = -1;
            for (int ei = 0; ei < (int)m_emitters.size(); ++ei) {
                if (m_emitters[ei].active) { g = ei; break; }
            }
        }
        if (g >= 0) {
            p.vx += m_emitters[g].gravityX * dt;
            p.vy += m_emitters[g].gravityY * dt;
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
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[ParticleSystem] render: transient VB alloc failed (need %d, got %d)", maxVerts, (int)tvb.size);
        return;
    }
    auto* vtx = (PtVertex*)tvb.data;

    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientIndexBuffer(&tib, (uint32_t)(m_aliveCount * 6));
    if (tib.size < (uint32_t)(m_aliveCount * 6)) {
        DEBUG_ERR(SubSys::Render, ErrCode::Ok,
                  "[ParticleSystem] render: transient IB alloc failed");
        return;
    }
    auto* idx = (uint16_t*)tib.data;

    int vi = 0, ii = 0;
    for (auto& p : m_particles) {
        if (!p.alive) continue;
        // Pure visual math (life decay + quad) -- unit-tested helper.
        const ParticleQuad vq = buildParticleVisual(p);
        const float x0 = vq.x0, y0 = vq.y0, x1 = vq.x1, y1 = vq.y1;
        const uint8_t cr = vq.r, cg = vq.g, cb = vq.b, ca = vq.a;

        vtx[vi+0] = { x0, y0, 0.0f, 0.0f, cr, cg, cb, ca };
        vtx[vi+1] = { x1, y0, 1.0f, 0.0f, cr, cg, cb, ca };
        vtx[vi+2] = { x1, y1, 1.0f, 1.0f, cr, cg, cb, ca };
        vtx[vi+3] = { x0, y1, 0.0f, 1.0f, cr, cg, cb, ca };

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
