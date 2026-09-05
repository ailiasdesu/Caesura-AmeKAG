// A separate process isolates allocation faults from the ordinary test runner.
// Only the production translation unit's EVP context entry points are renamed;
// these wrappers still allocate/free real OpenSSL contexts.
#include "archive/CryptoEngine.h"
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
constexpr size_t kPayloadSize = 4096;
size_t failSize = 0;
bool faultTriggered = false;
std::array<EVP_CIPHER_CTX*, 16> contexts{};
unsigned created = 0;
unsigned released = 0;
unsigned failures = 0;

void check(bool passed, const char* label) {
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", label);
    if (!passed) ++failures;
}

void cleanLeakedContexts() {
    for (auto& context : contexts) {
        if (context) {
            EVP_CIPHER_CTX_free(context);
            context = nullptr;
        }
    }
}
} // namespace

void* operator new(std::size_t size) {
    if (failSize != 0 && size == failSize) {
        failSize = 0;
        faultTriggered = true;
        throw std::bad_alloc();
    }
    if (void* allocation = std::malloc(size == 0 ? 1 : size)) return allocation;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

extern "C" EVP_CIPHER_CTX* caesura_test_cipher_new() {
    auto* context = EVP_CIPHER_CTX_new();
    if (!context) return nullptr;
    auto slot = std::find(contexts.begin(), contexts.end(), nullptr);
    if (slot == contexts.end()) std::abort();
    *slot = context;
    ++created;
    return context;
}

extern "C" void caesura_test_cipher_free(EVP_CIPHER_CTX* context) {
    if (context) {
        auto slot = std::find(contexts.begin(), contexts.end(), context);
        if (slot == contexts.end()) std::abort();
        *slot = nullptr;
        ++released;
    }
    EVP_CIPHER_CTX_free(context);
}

namespace {
using namespace Caesura::carc;
using Key = std::array<uint8_t, AES_KEY_SIZE>;
using Nonce = std::array<uint8_t, AES_NONCE_SIZE>;
using Tag = std::array<uint8_t, AES_TAG_SIZE>;

void allocationFailure(bool encrypting, const std::vector<uint8_t>& cipher,
                       const Key& key, const Nonce& cipherNonce, const Tag& cipherTag) {
    std::array<uint8_t, kPayloadSize> input{};
    Nonce nextNonce{};
    nextNonce.back() = 2; // Separate from the successful fixture encryption.
    Tag outputTag{};
    const auto createdBefore = created;
    const auto releasedBefore = released;
    faultTriggered = false;
    failSize = encrypting ? kPayloadSize + 16 : kPayloadSize;
    bool rejected = false;
    try {
        auto result = encrypting
            ? CryptoEngine::encrypt(input.data(), input.size(), key.data(),
                                    nextNonce.data(), outputTag.data())
            : CryptoEngine::decrypt(cipher.data(), cipher.size(), key.data(),
                                    cipherNonce.data(), cipherTag.data());
        rejected = result.empty();
    } catch (const std::bad_alloc&) {
        rejected = true; // CARCReader owns the public failure-state contract.
    }
    failSize = 0;
    check(faultTriggered && rejected, encrypting ? "encrypt allocation rejected" : "decrypt allocation rejected");
    check(created - createdBefore == released - releasedBefore,
          encrypting ? "encrypt context released on allocation failure" : "decrypt context released on allocation failure");
    cleanLeakedContexts(); // Keep a failing probe bounded too; not counted as production cleanup.
}
} // namespace

int main() {
    std::array<uint8_t, kPayloadSize> input{};
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<uint8_t>(i);
    Key key{};
    key.back() = 7;
    Nonce nonce{};
    nonce.back() = 1;
    Tag tag{};
    const auto cipher = CryptoEngine::encrypt(input.data(), input.size(), key.data(), nonce.data(), tag.data());
    check(cipher.size() == input.size(), "real encrypted fixture");
    if (cipher.size() != input.size()) return 1;
    allocationFailure(true, cipher, key, nonce, tag);
    allocationFailure(false, cipher, key, nonce, tag);

    const auto beforeCreated = created;
    const auto beforeReleased = released;
    const auto plain = CryptoEngine::decrypt(cipher.data(), cipher.size(), key.data(), nonce.data(), tag.data());
    check(plain.size() == input.size() && std::equal(plain.begin(), plain.end(), input.begin()),
          "exact decrypt after allocation failures");
    auto wrongTag = tag;
    wrongTag.front() ^= 1;
    check(CryptoEngine::decrypt(cipher.data(), cipher.size(), key.data(), nonce.data(), wrongTag.data()).empty(),
          "invalid tag rejected");
    nonce.back() = 3;
    const auto nextCipher = CryptoEngine::encrypt(input.data(), input.size(), key.data(), nonce.data(), tag.data());
    const auto nextPlain = CryptoEngine::decrypt(nextCipher.data(), nextCipher.size(), key.data(), nonce.data(), tag.data());
    check(nextPlain == plain, "exact encryption and decryption recovery");
    check(created - beforeCreated == released - beforeReleased, "recovery contexts released");
    check(std::all_of(contexts.begin(), contexts.end(), [](const auto* context) { return context == nullptr; }),
          "no active contexts remain");
    cleanLeakedContexts();
    std::printf("checks=10 failures=%u\n", failures);
    return failures ? 1 : 0;
}
