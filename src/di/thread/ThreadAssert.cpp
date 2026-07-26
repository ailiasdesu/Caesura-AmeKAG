#include "../api/ThreadAssert.h"

namespace Caesura::detail {

thread_local std::thread::id g_mainThreadId;

} // namespace Caesura::detail
