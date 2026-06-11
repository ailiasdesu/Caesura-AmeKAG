#include "doctest.h"
#include "resource/ResourceHandle.h"

using namespace Caesura;

TEST_CASE("ResourceHandle::copy and assign") {
    ResourceHandle h1;
    h1.id = 42;
    ResourceHandle h2 = h1;
    CHECK(h2.id == 42);
    ResourceHandle h3;
    h3 = h1;
    CHECK(h3.id == 42);
}

TEST_CASE("ResourceHandle::operator bool") {
    ResourceHandle zero;
    CHECK_FALSE(zero);
    ResourceHandle valid;
    valid.id = 1;
    CHECK(valid);
}

TEST_CASE("ResourceHandle::handleTypeName covers all types") {
    CHECK(handleTypeName(HandleType::TEXTURE) != nullptr);
    CHECK(handleTypeName(HandleType::AUDIO) != nullptr);
    CHECK(handleTypeName(HandleType::VIDEO) != nullptr);
}

TEST_CASE("GenerationTracker::handle reuse after invalidate") {
    GenerationTracker gt;
    auto h1 = gt.makeHandle(HandleType::TEXTURE, 99);
    CHECK(gt.isCurrent(h1));
    gt.invalidate(HandleType::TEXTURE);
    CHECK_FALSE(gt.isCurrent(h1));
    auto h2 = gt.makeHandle(HandleType::TEXTURE, 99);
    CHECK(gt.isCurrent(h2));
}
