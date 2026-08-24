#include "GestureDetector.h"
#include <cmath>

namespace Caesura {

void GestureDetector::reset() {
    for (auto& f : m_fingers) { f = Finger{}; }
    m_active = 0;
    m_longPressFired = false;
    m_pinchBaseDist = 0.0f;
    m_pinchLastRatio = 1.0f;
    m_pinchStarted = false;
}

GestureDetector::Finger* GestureDetector::findFinger(int id) {
    for (auto& f : m_fingers) {
        if (f.active && f.id == id) return &f;
    }
    return nullptr;
}

void GestureDetector::beginPinchBase(double /*nowMs*/) {
    float ax = 0, ay = 0, bx = 0, by = 0;
    int n = 0;
    for (const auto& f : m_fingers) {
        if (!f.active) continue;
        if (n == 0) { ax = f.x; ay = f.y; }
        else if (n == 1) { bx = f.x; by = f.y; }
        ++n;
    }
    if (n >= 2 && std::hypot(bx - ax, by - ay) > 0.0f) {
        m_pinchBaseDist = std::hypot(bx - ax, by - ay);
        m_pinchLastRatio = 1.0f;
        m_pinchStarted = false;
    } else {
        m_pinchBaseDist = 0.0f;
    }
}

void GestureDetector::onFingerDown(int id, float x, float y, double nowMs) {
    if (findFinger(id)) return;  // duplicate down: ignore
    for (auto& f : m_fingers) {
        if (!f.active) {
            f = Finger{};
            f.active = true;
            f.id = id;
            f.x = x; f.y = y;
            f.downAtMs = nowMs;
            ++m_active;
            break;
        }
    }
    // A new finger invalidates an in-flight long-press and re-bases a pinch.
    m_longPressFired = false;
    beginPinchBase(nowMs);
}

void GestureDetector::onFingerMove(int id, float x, float y, double /*nowMs*/) {
    Finger* f = findFinger(id);
    if (!f) return;
    const float dx = x - f->x, dy = y - f->y;
    if ((dx * dx + dy * dy) > kMoveSlop * kMoveSlop) {
        f->moved = true;
    }
    f->x = x; f->y = y;
}

void GestureDetector::onFingerUp(int id, double nowMs) {
    Finger* f = findFinger(id);
    if (f) {
        *f = Finger{};
        --m_active;
    }
    // Gesture boundary: long-press candidate gone, pinch base gone.
    m_longPressFired = false;
    beginPinchBase(nowMs);
}

GestureEvent GestureDetector::tick(double nowMs) {
    // -- Long press: one finger, held still, past the timeout. --
    if (m_active == 1 && !m_longPressFired) {
        for (const auto& f : m_fingers) {
            if (!f.active) continue;
            if (!f.moved && (nowMs - f.downAtMs) >= kLongPressMs) {
                m_longPressFired = true;
                return { GestureEvent::Kind::LongPress, f.x, f.y, 1.0f };
            }
            break;
        }
    }

    // -- Pinch: two or more fingers, distance ratio pulses. --
    if (m_active >= 2 && m_pinchBaseDist > 0.0f) {
        float ax = 0, ay = 0, bx = 0, by = 0;
        int n = 0;
        for (const auto& f : m_fingers) {
            if (!f.active) continue;
            if (n == 0) { ax = f.x; ay = f.y; }
            else if (n == 1) { bx = f.x; by = f.y; }
            ++n;
        }
        if (n >= 2) {
            const float d = std::hypot(bx - ax, by - ay);
            if (d > 0.0f) {
                const float ratio = d / m_pinchBaseDist;
                const bool start = !m_pinchStarted && std::fabs(ratio - 1.0f) >= kPinchInitial;
                const bool step  = m_pinchStarted && std::fabs(ratio - m_pinchLastRatio) >= kPinchStep;
                if (start || step) {
                    m_pinchStarted = true;
                    m_pinchLastRatio = ratio;
                    return { GestureEvent::Kind::Pinch,
                             (ax + bx) * 0.5f, (ay + by) * 0.5f, ratio };
                }
            }
        }
    }
    return { GestureEvent::Kind::None, 0.0f, 0.0f, 1.0f };
}

int GestureDetector::activeFingerCount() const { return m_active; }

} // namespace Caesura
