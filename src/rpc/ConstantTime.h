#pragma once

#include <cstddef>
#include <string>

namespace Caesura {

// Constant-time string equality: loops over every byte of `expected`
// unconditionally (no early exit on the first mismatch) so response latency
// cannot reveal the first-differing position. `expected` is typically the
// secret (e.g. "Bearer <token>"); its length is considered public.
//
// Returns true only when both strings are byte-identical. Length mismatch
// sets the accumulator without changing the iteration count, so the timing
// profile depends only on the (public) expected length.
inline bool constantTimeEquals(const std::string& actual,
                               const std::string& expected) {
    unsigned char diff = 0;
    const std::size_t n = expected.size();
    if (actual.size() != n) {
        diff = 1;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char a = (i < actual.size())
            ? static_cast<unsigned char>(actual[i]) : 0;
        diff |= static_cast<unsigned char>(
            a ^ static_cast<unsigned char>(expected[i]));
    }
    return diff == 0;
}

} // namespace Caesura
