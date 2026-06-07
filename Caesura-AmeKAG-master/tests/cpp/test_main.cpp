#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "Core/Engine.h"
#include <thread>

// Set main thread ID before any tests run, so CAESURA_ASSERT_MAIN_THREAD() passes
static int s_testSetup = []() -> int {
    Caesura::Engine::s_mainThreadId = std::this_thread::get_id();
    return 0;
}();
