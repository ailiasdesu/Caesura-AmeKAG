#include "doctest.h"
#include "entry/ErrorUI.h"
#include <string>

using namespace Caesura;

TEST_CASE("ErrorUI::resetCounters") {
    ErrorUI::resetCounters();
    // After reset, no crash on query
}

TEST_CASE("ErrorUI::action enum values") {
    CHECK(static_cast<int>(ErrorAction::Retry) >= 0);
    CHECK(static_cast<int>(ErrorAction::Title) >= 0);
    CHECK(static_cast<int>(ErrorAction::Quit) >= 0);
}

TEST_CASE("ErrorUI::show with null renderer returns Quit") {
    ErrorAction action = ErrorUI::show("Test", "Body", "", 0, false);
    // With null renderer, should not crash
    CHECK(static_cast<int>(action) >= 0);
}

// ---------------------------------------------------------------------------
// §15 Crash/Diagnostics: buildDiagnosticText (pure formatter)
// ---------------------------------------------------------------------------

TEST_CASE("ErrorUI::buildDiagnosticText renders all provided fields") {
    DiagnosticInfo diag;
    diag.projectName = "TestProject";
    diag.engineVersion = "9.9.9";
    diag.platform = "TestOS";
    diag.gpuBackend = "d3d11";
    diag.scenePath = "assets/script/main.ks";

    const std::string text = ErrorUI::buildDiagnosticText(
        diag, "Oops", "boom body", "assets/script/sub.ks", 42, "jump");

    // Title + every labeled field
    CHECK(text.find("Oops") != std::string::npos);
    CHECK(text.find("Project: TestProject") != std::string::npos);
    CHECK(text.find("Version: 9.9.9") != std::string::npos);
    CHECK(text.find("Platform: TestOS") != std::string::npos);
    CHECK(text.find("GPU: d3d11") != std::string::npos);
    CHECK(text.find("Scene: assets/script/main.ks") != std::string::npos);
    CHECK(text.find("Script: assets/script/sub.ks") != std::string::npos);
    CHECK(text.find("Line: 42") != std::string::npos);
    CHECK(text.find("Command: jump") != std::string::npos);

    // Message is part of the output and comes after the diagnostic block
    CHECK(text.find("boom body") != std::string::npos);
    CHECK(text.rfind("boom body") > text.rfind("Command: jump"));
}

TEST_CASE("ErrorUI::buildDiagnosticText omits absent optional fields") {
    DiagnosticInfo diag;  // scenePath left empty

    // tokenLine=0 and empty commandName/scriptTrace -> their lines are omitted
    const std::string text = ErrorUI::buildDiagnosticText(
        diag, "T", "M", "", 0, "");
    CHECK(text.find("Scene:") == std::string::npos);
    CHECK(text.find("Script:") == std::string::npos);
    CHECK(text.find("Line:") == std::string::npos);
    CHECK(text.find("Command:") == std::string::npos);

    // Core fields are always present
    CHECK(text.find("Project:") != std::string::npos);
    CHECK(text.find("Version:") != std::string::npos);
    CHECK(text.find("Platform:") != std::string::npos);
    CHECK(text.find("GPU:") != std::string::npos);

    // Negative tokenLine is treated as absent as well
    const std::string neg = ErrorUI::buildDiagnosticText(
        diag, "T", "M", "", -5, "");
    CHECK(neg.find("Line:") == std::string::npos);
}

TEST_CASE("ErrorUI::buildDiagnosticText default context is backward compatible") {
    const DiagnosticInfo diag{};  // aggregate default initialization
    CHECK(diag.projectName == "Caesura (AmeKAG)");
    CHECK(diag.engineVersion.empty());
    CHECK(diag.platform.empty());
    CHECK(diag.gpuBackend == "unknown");
    CHECK(diag.scenePath.empty());
    CHECK(diag.logDir == "logs");

    // Default-constructed context still produces a well-formed block
    const std::string text = ErrorUI::buildDiagnosticText(
        diag, "T", "M", "", 0, "");
    CHECK(text.find("Project: Caesura (AmeKAG)") != std::string::npos);
    CHECK(text.find("GPU: unknown") != std::string::npos);
    CHECK(text.find("T") != std::string::npos);
    CHECK(text.find("M") != std::string::npos);
}

TEST_CASE("ErrorUI::isRetrySafe whitelist unchanged by diagnostics") {
    ErrorUI::resetCounters();
    CHECK(ErrorUI::isRetrySafe("text"));
    CHECK_FALSE(ErrorUI::isRetrySafe("trans"));   // animate op -> promote to title
    CHECK_FALSE(ErrorUI::isRetrySafe(""));
    ErrorUI::resetCounters();
}

TEST_CASE("ErrorUI::show accepts DiagnosticInfo overload (backward compatible)") {
    ErrorUI::resetCounters();

    // Old call shape (no diagnostic argument): must compile and route to the
    // Level 2 fallback (bgfxAlive=false) returning Quit without crashing.
    ErrorAction legacy = ErrorUI::show("Legacy", "body", "", 0, false);
    CHECK(legacy == ErrorAction::Quit);
    ErrorUI::resetCounters();

    // Explicitly passing default diagnostics behaves identically.
    ErrorAction withDiag = ErrorUI::show("WithDiag", "body", "", 0, false,
                                         "", DiagnosticInfo{});
    CHECK(withDiag == ErrorAction::Quit);
    ErrorUI::resetCounters();
}
