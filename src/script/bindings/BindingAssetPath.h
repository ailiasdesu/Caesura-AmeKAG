#pragma once
#include <cstddef>

namespace Caesura {
inline bool validBindingAssetPath(const char* path, size_t size) {
    if (!path || size == 0 || size > 4096 || path[0] == '/') return false;
    for (size_t i = 0; i < size; ++i) {
        if (static_cast<unsigned char>(path[i]) < 32 || path[i] == '\\' || path[i] == ':'
            || (path[i] == '.' && i + 1 < size && path[i + 1] == '.')) return false;
    }
    return true;
}
}
