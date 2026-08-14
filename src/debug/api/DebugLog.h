// DebugLog.h — thin public header for the DEBUG_* macros (round 21 P1-6).
// Macros used to live in the concrete DebugManager.h, forcing consumers in
// other modules to include a concrete implementation header. This api/
// header exposes the same macros through a free function so modules only
// need the debug API surface.
#pragma once

#include "IDebugManager.h"
#include <cstdarg>

namespace Caesura {
namespace debug {

// Free-function sink implemented in DebugManager.cpp (forwards to
// DebugManager::instance().log). One varargs entry point for all levels.
void log(DbgLevel level, SubSys sub, ErrCode ec, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

} // namespace debug
} // namespace Caesura

#ifndef CAESURA_DEBUG
  #define CAESURA_DEBUG 0
#endif

#if CAESURA_DEBUG
  #define DEBUG_TRACE(ss, ec, fmt, ...) \
    Caesura::debug::log(Caesura::DbgLevel::Trace, ss, ec, fmt, ##__VA_ARGS__)
  #define DEBUG_DBG(ss, ec, fmt, ...) \
    Caesura::debug::log(Caesura::DbgLevel::Debug, ss, ec, fmt, ##__VA_ARGS__)
  #define DEBUG_INFO(ss, ec, fmt, ...) \
    Caesura::debug::log(Caesura::DbgLevel::Info, ss, ec, fmt, ##__VA_ARGS__)
#else
  #define DEBUG_TRACE(...)  ((void)0)
  #define DEBUG_DBG(...)    ((void)0)
  #define DEBUG_INFO(...)   ((void)0)
#endif

#define DEBUG_WARN(ss, ec, fmt, ...) \
  Caesura::debug::log(Caesura::DbgLevel::Warn, ss, ec, fmt, ##__VA_ARGS__)
#define DEBUG_ERR(ss, ec, fmt, ...) \
  Caesura::debug::log(Caesura::DbgLevel::Err, ss, ec, fmt, ##__VA_ARGS__)
#define DEBUG_FATAL(ss, ec, fmt, ...) \
  Caesura::debug::log(Caesura::DbgLevel::Fatal, ss, ec, fmt, ##__VA_ARGS__)

// Backward-compat aliases (DebugManager.h keeps its own for its users).
#define DEBUG_ERROR DEBUG_ERR
#define DEBUG_DEBUG DEBUG_DBG
