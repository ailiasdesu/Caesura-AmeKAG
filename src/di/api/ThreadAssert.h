#pragma once

#include <thread>
#include <cassert>

namespace Caesura { namespace detail {
    extern thread_local std::thread::id g_mainThreadId;
}}

// -- Thread safety assertion macros (Debug only) --
#ifndef NDEBUG
#define CAESURA_ASSERT_MAIN_THREAD() \
    assert(std::this_thread::get_id() == ::Caesura::detail::g_mainThreadId)
#define CAESURA_ASSERT_NOT_MAIN_THREAD() \
    assert(std::this_thread::get_id() != ::Caesura::detail::g_mainThreadId)
#else
#define CAESURA_ASSERT_MAIN_THREAD() ((void)0)
#define CAESURA_ASSERT_NOT_MAIN_THREAD() ((void)0)
#endif