// test_steam.cpp — Steam backend tests (NullSteamBackend)
#include "doctest.h"
#include "steam/NullSteamBackend.h"
#include "steam/ISteamBackend.h"

using namespace Caesura;

TEST_CASE("NullSteamBackend::init returns false") {
    NullSteamBackend steam;
    CHECK(steam.init() == false);
}

TEST_CASE("NullSteamBackend::name") {
    NullSteamBackend steam;
    CHECK(std::string(steam.name()) == "NullSteam");
}

TEST_CASE("NullSteamBackend::overlay is never active") {
    NullSteamBackend steam;
    CHECK(steam.isOverlayActive() == false);
}

TEST_CASE("NullSteamBackend::achievements always return false") {
    NullSteamBackend steam;
    CHECK(steam.unlockAchievement("ACH_TEST") == false);
    CHECK(steam.isAchievementUnlocked("ACH_TEST") == false);
    CHECK(steam.resetAchievement("ACH_TEST") == false);
    CHECK(steam.resetAllAchievements() == false);
}

TEST_CASE("NullSteamBackend::stats return default values") {
    NullSteamBackend steam;
    CHECK(steam.setStatInt("kills", 10) == false);
    CHECK(steam.getStatInt("kills") == 0);
    CHECK(steam.setStatFloat("time", 1.5f) == false);
    CHECK(steam.getStatFloat("time") == 0.0f);
    CHECK(steam.storeStats() == false);
}

TEST_CASE("NullSteamBackend::cloud operations return empty") {
    NullSteamBackend steam;
    const char* data = "test save data";
    CHECK(steam.cloudWrite("save.dat", data, 13) == false);
    char buf[256] = {};
    CHECK(steam.cloudRead("save.dat", buf, 256) == 0);
    CHECK(steam.cloudFileSize("save.dat") == 0);
    CHECK(steam.cloudFileExists("save.dat") == false);
    CHECK(steam.cloudDelete("save.dat") == false);
    CHECK(steam.cloudQuotaTotal() == 0);
    CHECK(steam.cloudQuotaUsed() == 0);
}

TEST_CASE("NullSteamBackend::runCallbacks does not crash") {
    NullSteamBackend steam;
    steam.runCallbacks();  // should be no-op
}

TEST_CASE("NullSteamBackend::shutdown is idempotent") {
    NullSteamBackend steam;
    steam.shutdown();
    steam.shutdown();  // second call should not crash
}
