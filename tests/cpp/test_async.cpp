#include "doctest.h"
#include "Resource/AsyncLoader.h"

using namespace Caesura;

TEST_CASE("AsyncLoader::singleton") {
    auto& a = AsyncLoader::instance();
    auto& b = AsyncLoader::instance();
    CHECK(&a == &b);
}

TEST_CASE("AsyncLoader::shutdown is idempotent") {
    auto& al = AsyncLoader::instance();
    al.init();
    al.shutdown();
    al.shutdown();
}

TEST_CASE("AsyncLoader::enqueue returns positive id") {
    auto& al = AsyncLoader::instance();
    al.init();
    int id = al.enqueue("test.png", "texture");
    CHECK(id > 0);
    al.cancelAll();
    al.shutdown();
}

TEST_CASE("AsyncLoader::rejects path traversal") {
    auto& al = AsyncLoader::instance();
    al.init();
    int id = al.enqueue("../secret.png", "texture");
    CHECK(id < 0);
    al.shutdown();
}

TEST_CASE("AsyncLoader::cancelAll clears queue") {
    auto& al = AsyncLoader::instance();
    al.init();
    al.enqueue("a.png", "texture");
    al.enqueue("b.png", "texture");
    al.cancelAll();
    CHECK(al.pendingCount() == 0);
    al.shutdown();
}

TEST_CASE("AsyncLoader::poll does not crash") {
    auto& al = AsyncLoader::instance();
    al.init();
    bool has = al.poll();
    // poll may return true or false
    al.shutdown();
}
