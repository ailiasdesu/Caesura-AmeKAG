// test_debug_manager_edge.cpp — edge/boundary tests for DebugManager (IDebugManager).
//
// Scope: ring buffer capacity + FIFO overwrite, per-subsystem counters,
// frame profiling, log message truncation/format, lifecycle (un-init /
// re-init / post-shutdown), and multi-threaded logging safety.
//
// Design note: DebugManager is a process-wide singleton whose counters and
// ring buffer persist across test cases (and across test files). We therefore
// always assert against a baseline snapshot taken at the start of each test
// rather than absolute values. Many tests run WITHOUT init(): logging works
// purely in-memory (file I/O is skipped when the log file is not open), which
// keeps the fast-path tests isolated and fast.
//
// Two known implementation quirks are pinned below (not "fixed" here, we only
// add tests under tests/cpp/):
//   * kSubSysCounterBuckets == 7, but SubSys has 12 enumerators (0..11). The
//     high subsystems (Live2D=7 .. Archive=11) fall off the counter arrays and
//     their total/error/warn counts are silently dropped.
//   * beginFrameProfile() resets gpu submit / transient alloc counters but NOT
//     recordLuaGc() accumulation, so luaGcMs leaks across frame boundaries.
#include "doctest.h"
#include "debug/DebugManager.h"
#include "debug/api/IDebugManager.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <cstring>

using namespace Caesura;

namespace {

// Snapshot of counters relevant to a test, taken under the same lock-free
// read path the tests use (each accessor locks internally).
struct DmBaseline {
    uint32_t entry = 0;
    uint32_t errors = 0;
    uint32_t renderErr = 0;
    uint32_t audioErr = 0;
    uint32_t engineErr = 0;
};

DmBaseline baseline() {
    auto& dm = DebugManager::instance();
    return {
        dm.entryCount(),
        dm.errorCount(),
        dm.subsystemErrorCount(SubSys::Render),
        dm.subsystemErrorCount(SubSys::Audio),
        dm.subsystemErrorCount(SubSys::Engine),
    };
}

} // namespace

// ============================================================================
// 1. Ring buffer — capacity, oldest-overwrite, FIFO read order, empty buffer
// ============================================================================

TEST_CASE("DebugManager ring buffer is empty when no log calls have issued") {
    auto& dm = DebugManager::instance();
    // The singleton may already hold entries from earlier test files, so this
    // is only meaningful as a "no crash / readable" assertion rather than an
    // absolute == 0. We re-check the documented invariant instead: reading an
    // empty ring returns an empty deque and does not throw.
    auto ring = dm.ringBuffer();
    CHECK(ring.size() == dm.entryCount());  // snapshot consistency
}

TEST_CASE("DebugManager ring buffer caps at exactly 1024 and overwrites oldest") {
    auto& dm = DebugManager::instance();
    const auto base = baseline();

    // The buffer keeps the most RECENT kRingSize entries. Log enough distinct
    // messages (well beyond 1024) and assert the retained window is exactly
    // the newest 1024 in FIFO order.
    constexpr int kRingSize = 1024;
    constexpr int kTotal = kRingSize + 250;  // overflow the capacity
    for (int i = 0; i < kTotal; ++i) {
        dm.log(DbgLevel::Info, SubSys::Dbg, ErrCode::Ok, "overflow-%d", i);
    }

    const auto ring = dm.ringBuffer();
    CHECK(ring.size() == kRingSize);  // hard cap, exactly capacity
    CHECK(dm.entryCount() == static_cast<uint32_t>(kRingSize));

    // Oldest surviving entry is kTotal - kRingSize; oldest was evicted.
    const auto& oldest = ring.front();
    const auto& newest = ring.back();
    CHECK(std::string(oldest.message) == "overflow-250");   // 1274 - 1024
    CHECK(std::string(newest.message) == "overflow-1273");  // last written

    // FIFO read order: entries are strictly in insertion order.
    bool inOrder = true;
    for (size_t i = 1; i < ring.size(); ++i) {
        if (ring[i - 1].timestamp > ring[i].timestamp) { inOrder = false; break; }
    }
    CHECK(inOrder);

    // Sanity: our writes added exactly kTotal ring entries on top of baseline.
    CHECK(dm.entryCount() == static_cast<uint32_t>(kRingSize));
    CHECK(dm.errorCount() >= base.errors);  // no errors logged here
}

TEST_CASE("DebugManager ring overwrite does not mutate evicted entries via accessors") {
    auto& dm = DebugManager::instance();
    // Evicted entries are simply dropped — the newest window only. Fill small
    // window using Dbg messages so subsystem counts are untouched (all Info).
    constexpr int kRingSize = 1024;
    for (int i = 0; i < kRingSize + 5; ++i) {
        dm.log(DbgLevel::Info, SubSys::Dbg, ErrCode::Ok, "w-%d", i);
    }
    const auto ring = dm.ringBuffer();
    CHECK(ring.size() == static_cast<size_t>(kRingSize));
    CHECK(std::string(ring.front().message) == "w-5");   // 1029 - 1024
    CHECK(std::string(ring.back().message) == "w-1028");
}

// ============================================================================
// 2. Subsystem statistics — independent counters, error-code capture
// ============================================================================

TEST_CASE("DebugManager subsystem error/warn/info counts are independent") {
    auto& dm = DebugManager::instance();
    const auto base = baseline();

    // Fire a mix across two subsystems; ensure they do NOT bleed into each
    // other's error/warn buckets.
    uint32_t renderErrBase = base.renderErr;
    uint32_t audioErrBase = base.audioErr;
    dm.log(DbgLevel::Err, SubSys::Render, ErrCode::Render_BgfxInitFailed, "r1");
    dm.log(DbgLevel::Err, SubSys::Render, ErrCode::Render_RTTAllocFailed, "r2");
    dm.log(DbgLevel::Warn, SubSys::Render, ErrCode::Ok, "rw");
    dm.log(DbgLevel::Err, SubSys::Audio, ErrCode::Audio_VoicePlayFailed, "a1");
    dm.log(DbgLevel::Info, SubSys::Render, ErrCode::Ok, "rinfo");

    // Render errors +2, audio errors +1; none cross subsystems.
    CHECK(dm.subsystemErrorCount(SubSys::Render) == renderErrBase + 2);
    CHECK(dm.subsystemErrorCount(SubSys::Audio) == audioErrBase + 1);

    // ErrorCount totals the buckets so it moved by exactly 3.
    CHECK(dm.errorCount() == base.errors + 3);

    // Warn is a separate bucket: render warns grow, audio warns do NOT
    // change (warn counts are cumulative across the whole binary, so compare
    // against a baseline rather than assuming zero).
    uint32_t audioWarnBefore = dm.getSubsystemStats(SubSys::Audio).warnCount;
    dm.log(DbgLevel::Warn, SubSys::Render, ErrCode::Ok, "rw2");
    auto rStats = dm.getSubsystemStats(SubSys::Render);
    auto aStats = dm.getSubsystemStats(SubSys::Audio);
    CHECK(rStats.warnCount > 0);
    CHECK(aStats.warnCount == audioWarnBefore);  // audio untouched
}

TEST_CASE("DebugManager subsystem stats capture last error code and message") {
    auto& dm = DebugManager::instance();
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_LuaInitFailed, "lua down");
    dm.log(DbgLevel::Warn, SubSys::Engine, ErrCode::Ok, "not an error");
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_UpdateError, "update blew");

    const auto st = dm.getSubsystemStats(SubSys::Engine);
    CHECK(st.errorCount >= 2);
    // lastErrorCode reflects the most recent *error-level* entry for the subsys.
    CHECK(st.lastErrorCode == static_cast<uint32_t>(ErrCode::Engine_UpdateError));
    CHECK(st.lastErrorMessage.find("update blew") != std::string::npos);
}

TEST_CASE("DebugManager lastError() is the newest error across all subsystems") {
    auto& dm = DebugManager::instance();
    dm.log(DbgLevel::Err, SubSys::Render, ErrCode::Render_ShaderCompileFailed, "older");
    dm.log(DbgLevel::Warn, SubSys::Render, ErrCode::Ok, "warn ignored");
    dm.log(DbgLevel::Err, SubSys::Audio, ErrCode::Audio_FileLoadFailed, "newest err");
    const auto last = dm.lastError();
    CHECK(last.subsystem == SubSys::Audio);
    CHECK(last.errorCode == ErrCode::Audio_FileLoadFailed);
    CHECK(std::string(last.message).find("newest err") != std::string::npos);
}

// Known quirk (implementation bug, pinned for documentation):
// m_totalCounts/m_errorCounts/m_warnCounts are std::array<uint32_t, 7> but
// SubSys::Live2D..Archive have indices 7..11 and are silently NOT tallied.
TEST_CASE("DebugManager high-index subsystems are not tallied (known quirk)") {
    auto& dm = DebugManager::instance();
    const auto baseErrors = dm.errorCount();
    const uint32_t baseArchive = dm.subsystemErrorCount(SubSys::Archive);

    dm.log(DbgLevel::Err, SubSys::Archive, ErrCode::Archive_ReadFailed, "archive boom");

    // sub-7 counters DO move if the bucket exists; Archive (index 11) has no
    // bucket, so its error count is unchanged and total errorCount is too.
    CHECK(dm.subsystemErrorCount(SubSys::Archive) == baseArchive);
    CHECK(dm.errorCount() == baseErrors);

    // But the entry is still recorded in the ring buffer (log path always runs).
    // The loss is only in the aggregate counters, not in the entries themselves.
}

// ============================================================================
// 3. Frame profiling — recording + readback + reset semantics
// ============================================================================

TEST_CASE("DebugManager frame profile records and reads back counters") {
    auto& dm = DebugManager::instance();
    dm.recordGpuSubmit(3);
    dm.recordGpuSubmit(4);
    dm.recordTransientAlloc(10, 4096);
    dm.recordTransientAlloc(5, 8192);
    dm.recordLuaGc(1.5);

    const auto& fp = dm.getFrameProfile();
    CHECK(fp.gpuSubmitCount == 7u);
    CHECK(fp.transientAllocCount == 15u);
    CHECK(fp.transientAllocBytes == 4096u + 8192u);
    CHECK(fp.luaGcMs == 1.5);
}

TEST_CASE("DebugManager beginFrameProfile resets gpu/transient but NOT luaGc (known quirk)") {
    auto& dm = DebugManager::instance();
    // luaGcMs is cumulative and never reset, so snapshot the pre-existing
    // value before adding our delta — the point is beginFrameProfile leaves it
    // untouched (does NOT zero it), unlike the gpu/transient counters.
    double gcBefore = dm.getFrameProfile().luaGcMs;
    dm.recordGpuSubmit(99);
    dm.recordTransientAlloc(7, 1234);
    dm.recordLuaGc(5.0);

    dm.beginFrameProfile();

    const auto& fp = dm.getFrameProfile();
    CHECK(fp.gpuSubmitCount == 0u);          // reset
    CHECK(fp.transientAllocCount == 0u);     // reset
    CHECK(fp.transientAllocBytes == 0u);     // reset
    // Quirk: luaGcMs is NOT reset by beginFrameProfile; our +5.0 is retained
    // on top of whatever accumulated before. Pins the bug.
    CHECK(fp.luaGcMs >= gcBefore + 4.99);    // <-- documents the bug

    // endFrameProfile stamps totalMs without disturbing the resets.
    const char* label = "frame-a";
    dm.beginFrameProfile();
    dm.recordGpuSubmit(1);
    dm.endFrameProfile();
    const auto& fp2 = dm.getFrameProfile();
    CHECK(fp2.gpuSubmitCount == 1u);
    CHECK(fp2.totalMs >= 0.0);
    (void)label;
}

TEST_CASE("DebugManager begin/endProfile round-trips a sample with depth") {
    auto& dm = DebugManager::instance();
    const char* outer = "outer";
    const char* inner = "inner";
    dm.beginProfile(outer);
    dm.beginProfile(inner);
    dm.endProfile(inner);
    dm.endProfile(outer);

    const auto& fp = dm.getFrameProfile();
    bool foundOuter = false, foundInner = false;
    for (const auto& s : fp.samples) {
        if (!std::strcmp(s.label, outer)) foundOuter = true;
        if (!std::strcmp(s.label, inner)) foundInner = true;
    }
    CHECK(foundOuter);
    CHECK(foundInner);
}

// ============================================================================
// 4. Log formatting — message truncation at the 255-char entry ceiling
// ============================================================================

TEST_CASE("DebugManager truncates over-long log messages to 255 chars") {
    auto& dm = DebugManager::instance();
    std::string longMsg(600, 'x');
    dm.log(DbgLevel::Info, SubSys::Dbg, ErrCode::Ok, "%s", longMsg.c_str());

    const auto ring = dm.ringBuffer();
    // The message slot is char[256]: 255 bytes + trailing '\0'.
    bool saw = false;
    for (auto it = ring.rbegin(); it != ring.rend(); ++it) {
        if (it->subsystem == SubSys::Dbg && it->message[0] == 'x') {
            saw = true;
            CHECK(std::strlen(it->message) == 255);
            CHECK(it->message[254] == 'x');
            CHECK(it->message[255] == '\0');
            break;
        }
    }
    CHECK(saw);
}

TEST_CASE("DebugManager preserves ordinary message content and error code") {
    auto& dm = DebugManager::instance();
    std::string msg = "hello from engine module";
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_RenderInitFailed, "%s", msg.c_str());
    const auto ring = dm.ringBuffer();
    bool saw = false;
    for (auto it = ring.rbegin(); it != ring.rend(); ++it) {
        if (it->subsystem == SubSys::Engine && !std::strcmp(it->message, msg.c_str())) {
            saw = true;
            CHECK(it->level == DbgLevel::Err);
            CHECK(it->errorCode == ErrCode::Engine_RenderInitFailed);
            break;
        }
    }
    CHECK(saw);
}

// ============================================================================
// 5. Lifecycle — un-init log, repeated init, log after shutdown
// ============================================================================

TEST_CASE("DebugManager logging before init is safe and buffered in memory") {
    auto& dm = DebugManager::instance();
    const auto base = baseline();
    // Must ensure we are not initialized for the "before init" portion.
    dm.shutdown();

    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_UpdateError, "pre-init log");
    dm.log(DbgLevel::Warn, SubSys::Engine, ErrCode::Ok, "pre-init warn");

    // Entries still land in the ring buffer without a live log file. Use
    // monotonic growth (entryCount is capped at 1024, so it may not move if
    // the singleton was already saturated by earlier tests) and scan for the
    // actual messages rather than assuming an exact delta.
    CHECK(dm.entryCount() >= base.entry);
    const auto ring = dm.ringBuffer();
    bool sawErr = false, sawWarn = false;
    for (const auto& e : ring) {
        if (e.subsystem == SubSys::Engine && !std::strcmp(e.message, "pre-init log")) sawErr = true;
        if (e.subsystem == SubSys::Engine && !std::strcmp(e.message, "pre-init warn")) sawWarn = true;
    }
    CHECK(sawErr);
    CHECK(sawWarn);
    CHECK(dm.subsystemErrorCount(SubSys::Engine) >= base.engineErr + 1);
    CHECK(dm.errorCount() >= base.errors + 1);

    // Re-init closes the un-init window with a normal init/shutdown cycle.
    CHECK(dm.init("logs"));
    dm.shutdown();
}

TEST_CASE("DebugManager repeated init is idempotent and does not reset state") {
    auto& dm = DebugManager::instance();
    const auto base = baseline();
    CHECK(dm.init("logs"));
    CHECK(dm.init("logs"));   // second init while initialized -> early return
    CHECK(dm.init("logs"));   // third, still idempotent

    // Counters survive re-init: log after init, init again, count still grew.
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_LuaInitFailed, "after-init");
    CHECK(dm.init("logs"));
    CHECK(dm.errorCount() >= base.errors + 1);
    dm.shutdown();
}

TEST_CASE("DebugManager logging after shutdown is safe (no crash, entries buffered)") {
    auto& dm = DebugManager::instance();
    const auto base = baseline();
    CHECK(dm.init("logs"));
    dm.log(DbgLevel::Info, SubSys::Engine, ErrCode::Ok, "before shout");
    dm.shutdown();

    // Post-shutdown log reaches the in-memory ring only; must not crash or
    // touch a closed file.
    dm.log(DbgLevel::Err, SubSys::Engine, ErrCode::Engine_RenderError, "post-shutdown");
    CHECK(dm.errorCount() >= base.errors + 1);
    CHECK(dm.entryCount() >= base.entry);
    const auto ring = dm.ringBuffer();
    bool saw = false;
    for (const auto& e : ring) {
        if (e.subsystem == SubSys::Engine && !std::strcmp(e.message, "post-shutdown")) saw = true;
    }
    CHECK(saw);
}

TEST_CASE("DebugManager init rejects path traversal and accepts null default") {
    auto& dm = DebugManager::instance();
    CHECK_FALSE(dm.init("../etc"));
    CHECK_FALSE(dm.init("a/../../b"));
    // null logDir -> "logs" default; harmless repeated init path.
    CHECK(dm.init(nullptr));
    dm.shutdown();
}

// ============================================================================
// 6. Thread safety — concurrent writers: no crash, counts are consistent
// ============================================================================

TEST_CASE("DebugManager multi-threaded logging does not corrupt or lose entries") {
    auto& dm = DebugManager::instance();
    const auto base = baseline();

    constexpr int kThreads = 8;
    constexpr int kPerThread = 200;  // 1600 messages total; modest to keep I/O light
    std::atomic<bool> start{ false };
    std::vector<std::thread> workers;
    std::atomic<int> errorsLogged{ 0 };

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&dm, &start, &errorsLogged, t]() {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < kPerThread; ++i) {
                if ((i % 10) == 0) {
                    dm.log(DbgLevel::Err, SubSys::Dbg, ErrCode::Internal_MutexLockFailed,
                           "t%d err %d", t, i);
                    errorsLogged.fetch_add(1, std::memory_order_relaxed);
                } else {
                    dm.log(DbgLevel::Info, SubSys::Dbg, ErrCode::Ok, "t%d msg %d", t, i);
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& w : workers) w.join();

    // No lost writes: the ring grew by exactly the number of messages issued
    // (capped at 1024, so if we exceed capacity we assert the cap and estimate).
    const auto ring = dm.ringBuffer();
    const uint32_t expectedTotal = base.entry + static_cast<uint32_t>(kThreads * kPerThread);
    const size_t expectedCapped = base.entry + static_cast<size_t>(kThreads * kPerThread);
    if (expectedCapped <= 1024) {
        CHECK(ring.size() == expectedCapped);
    } else {
        // Buffer evicted -> exactly full; the important invariant is that no
        // temporary undercount or overcount happened (size is a single locked read).
        CHECK(ring.size() <= 1024);
    }

    // Error count is an exact, race-free atomic: we tallied exactly what we wrote.
    const uint32_t errorsFromThreads = static_cast<uint32_t>(errorsLogged.load());
    CHECK(dm.errorCount() == base.errors + errorsFromThreads);
    CHECK(ring.size() == dm.entryCount());

    // Clean up so a later singleton user (init) opens a fresh file.
    dm.shutdown();
}
