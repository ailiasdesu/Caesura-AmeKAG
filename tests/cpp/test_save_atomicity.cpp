#include "doctest.h"
#include "storage/LocalFileSaveProvider.h"
#include "TestPaths.h"
#include "storage/AtomicSaveFile.h"
#include "storage/SaveManager.h"
#include "archive/CryptoEngine.h"
#include "di/BackendRegistry.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#endif

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace Caesura;

namespace {

std::string atomicSaveRawBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void atomicSaveSeed(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE(output.good());
}

#ifdef _WIN32
class AtomicSaveFileLock {
public:
    explicit AtomicSaveFileLock(const std::filesystem::path& path)
        : handle(CreateFileW(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
    ~AtomicSaveFileLock() {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
    AtomicSaveFileLock(const AtomicSaveFileLock&) = delete;
    AtomicSaveFileLock& operator=(const AtomicSaveFileLock&) = delete;

private:
    HANDLE handle;
};
#endif

} // namespace

TEST_CASE("Storage atomic save: public provider replaces complete opaque bytes") {
    TestPaths::ScopedTempDir directory("atomic_opaque");
    const auto slot = directory.path() / "slot.sav";
    const std::string previous("CAES\0old\0ciphertext", 19);
    const std::string replacement("CAES\0new\0ciphertext", 19);
    LocalFileSaveProvider provider;
    REQUIRE(provider.writeFile(slot.string(), previous));
    REQUIRE(provider.readFile(slot.string()) == previous);
    REQUIRE(provider.writeFile(slot.string(), replacement));
    CHECK(provider.readFile(slot.string()) == replacement);
    CHECK(atomicSaveRawBytes(slot) == replacement);
    CHECK(std::distance(std::filesystem::directory_iterator(directory.path()),
                        std::filesystem::directory_iterator()) == 1);
}

TEST_CASE("Storage atomic save: an interrupted legacy temp is never reused or promoted") {
    TestPaths::ScopedTempDir directory("atomic_stale_temp");
    const auto slot = directory.path() / "slot.sav";
    const auto legacyTemp = directory.path() / "slot.sav.tmp";
    const std::string previous = "last complete save";
    const std::string orphan = "interrupted partial bytes";
    const std::string replacement = "next complete save";
    atomicSaveSeed(slot, previous);
    atomicSaveSeed(legacyTemp, orphan);
    LocalFileSaveProvider provider;
    CHECK(provider.readFile(slot.string()) == previous);
    REQUIRE(provider.writeFile(slot.string(), replacement));
    CHECK(provider.readFile(slot.string()) == replacement);
    CHECK(atomicSaveRawBytes(legacyTemp) == orphan);
}

TEST_CASE("Storage atomic save: a directory target survives failed replacement") {
    TestPaths::ScopedTempDir directory("atomic_directory_target");
    const auto slot = directory.path() / "slot.sav";
    REQUIRE(std::filesystem::create_directory(slot));
    LocalFileSaveProvider provider;
    bool saved = true;
    CHECK_NOTHROW(saved = provider.writeFile(slot.string(), "complete save"));
    CHECK_FALSE(saved);
    CHECK(std::filesystem::is_directory(slot));
}

TEST_CASE("Storage atomic save: rejected oversized bytes preserve the previous save") {
    TestPaths::ScopedTempDir directory("atomic_oversized");
    const auto slot = directory.path() / "slot.sav";
    const std::string previous = "last complete save";
    atomicSaveSeed(slot, previous);
    LocalFileSaveProvider provider;
    CHECK_FALSE(provider.writeFile(slot.string(), std::string(10 * 1024 * 1024 + 1, 'x')));
    CHECK(atomicSaveRawBytes(slot) == previous);
}

#ifdef _WIN32
TEST_CASE("Storage atomic save: an occupied destination returns failure and preserves old bytes") {
    TestPaths::ScopedTempDir directory("atomic_destination_lock");
    const auto slot = directory.path() / "slot.sav";
    const std::string previous("CAES\0last complete bytes", 24);
    atomicSaveSeed(slot, previous);
    AtomicSaveFileLock occupied(slot);
    REQUIRE(occupied.valid());
    LocalFileSaveProvider provider;
    bool saved = true;
    CHECK_NOTHROW(saved = provider.writeFile(slot.string(), "replacement bytes"));
    CHECK_FALSE(saved);
    CHECK(atomicSaveRawBytes(slot) == previous);
    CHECK(std::distance(std::filesystem::directory_iterator(directory.path()),
                        std::filesystem::directory_iterator()) == 1);
}

TEST_CASE("Storage atomic save: another writer owning the legacy temp cannot destroy the slot") {
    TestPaths::ScopedTempDir directory("atomic_legacy_lock");
    const auto slot = directory.path() / "slot.sav";
    const auto legacyTemp = directory.path() / "slot.sav.tmp";
    const std::string previous = "last complete save";
    const std::string orphan = "another writer owns these bytes";
    const std::string replacement = "next complete save";
    atomicSaveSeed(slot, previous);
    atomicSaveSeed(legacyTemp, orphan);
    AtomicSaveFileLock occupied(legacyTemp);
    REQUIRE(occupied.valid());
    LocalFileSaveProvider provider;
    bool saved = false;
    CHECK_NOTHROW(saved = provider.writeFile(slot.string(), replacement));
    CHECK(saved);
    CHECK(atomicSaveRawBytes(slot) == replacement);
    CHECK(atomicSaveRawBytes(legacyTemp) == orphan);
}
#endif

namespace {

using detail::SaveWriteStage;
using detail::ScopedSaveWriteTestHook;

constexpr std::array<SaveWriteStage, 6> atomicSaveStages = {
    SaveWriteStage::CreateTemporary, SaveWriteStage::Write, SaveWriteStage::Flush,
    SaveWriteStage::Close, SaveWriteStage::Replace, SaveWriteStage::WriteProgress
};

bool rejectAtomicSaveStage(SaveWriteStage stage, const std::filesystem::path&, void* context) {
    return stage != *static_cast<SaveWriteStage*>(context);
}

struct AtomicSaveCommitBarrier {
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<std::filesystem::path> temporaries;
    std::vector<std::string> preparedBytes;
};

bool meetAtAtomicCommit(SaveWriteStage stage, const std::filesystem::path& path, void* context) {
    if (stage != SaveWriteStage::Replace) return true;
    auto& barrier = *static_cast<AtomicSaveCommitBarrier*>(context);
    std::unique_lock<std::mutex> lock(barrier.mutex);
    barrier.temporaries.push_back(path);
    barrier.preparedBytes.push_back(atomicSaveRawBytes(path));
    barrier.ready.notify_all();
    return barrier.ready.wait_for(lock, std::chrono::seconds(10),
                                  [&] { return barrier.temporaries.size() == 2; });
}

class AtomicSaveCryptoRegistration {
public:
    AtomicSaveCryptoRegistration() : previous(BackendRegistry::instance().getCryptoEngine()) {
        BackendRegistry::instance().setCryptoEngine(&crypto);
    }
    ~AtomicSaveCryptoRegistration() {
        BackendRegistry::instance().setCryptoEngine(previous);
    }

private:
    carc::CryptoEngine crypto;
    carc::ICryptoEngine* previous;
};

} // namespace

TEST_CASE("Storage atomic save: every precommit failure preserves previous bytes and cleans owned temp") {
    TestPaths::ScopedTempDir directory("atomic_failures");
    const auto slot = directory.path() / "slot.sav";
    const std::string previous("CAES\0last complete bytes", 24);
    atomicSaveSeed(slot, previous);
    LocalFileSaveProvider provider;
    for (auto stage : atomicSaveStages) {
        INFO("stage=", static_cast<int>(stage));
        ScopedSaveWriteTestHook hook({rejectAtomicSaveStage, &stage});
        CHECK_FALSE(provider.writeFile(slot.string(), "next complete save"));
        CHECK(atomicSaveRawBytes(slot) == previous);
        CHECK(std::distance(std::filesystem::directory_iterator(directory.path()),
                            std::filesystem::directory_iterator()) == 1);
    }
}

TEST_CASE("Storage atomic save: exclusive creation retries preserve colliding files and directories") {
    TestPaths::ScopedTempDir directory("atomic_collision");
    const auto slot = directory.path() / "slot.sav";
    const std::string previous = "last complete save";
    atomicSaveSeed(slot, previous);
    std::vector<std::filesystem::path> attempts;
    ScopedSaveWriteTestHook hook({
        [](SaveWriteStage stage, const std::filesystem::path& path, void* context) {
            if (stage != SaveWriteStage::CreateTemporary) return true;
            auto& paths = *static_cast<std::vector<std::filesystem::path>*>(context);
            paths.push_back(path);
            if (paths.size() == 1) atomicSaveSeed(path, "another writer's interrupted bytes");
            if (paths.size() == 2) std::filesystem::create_directory(path);
            return true;
        }, &attempts
    });
    LocalFileSaveProvider provider;
    REQUIRE(provider.writeFile(slot.string(), "next complete save"));
    REQUIRE(attempts.size() == 3);
    CHECK(atomicSaveRawBytes(slot) == "next complete save");
    CHECK(atomicSaveRawBytes(attempts[0]) == "another writer's interrupted bytes");
    CHECK(std::filesystem::is_directory(attempts[1]));
    CHECK_FALSE(std::filesystem::exists(attempts[2]));
}

TEST_CASE("Storage atomic save: fault hooks restore nested state and remain on their thread") {
    TestPaths::ScopedTempDir directory("atomic_thread_hook");
    LocalFileSaveProvider provider;
    auto outerStage = SaveWriteStage::CreateTemporary;
    auto innerStage = SaveWriteStage::Replace;
    {
        ScopedSaveWriteTestHook outer({rejectAtomicSaveStage, &outerStage});
        {
            ScopedSaveWriteTestHook inner({rejectAtomicSaveStage, &innerStage});
            CHECK_FALSE(provider.writeFile((directory.path() / "inner.sav").string(), "complete"));
        }
        CHECK_FALSE(provider.writeFile((directory.path() / "outer.sav").string(), "complete"));
        bool workerSaved = false;
        std::thread worker([&] {
            LocalFileSaveProvider local;
            workerSaved = local.writeFile((directory.path() / "worker.sav").string(), "complete");
        });
        worker.join();
        CHECK(workerSaved);
    }
    CHECK(provider.writeFile((directory.path() / "restored.sav").string(), "complete"));
    CHECK_FALSE(std::filesystem::exists(directory.path() / "inner.sav"));
    CHECK_FALSE(std::filesystem::exists(directory.path() / "outer.sav"));
}

TEST_CASE("Storage atomic save: concurrent same-slot writes prepare independent complete temporaries") {
    TestPaths::ScopedTempDir directory("atomic_concurrent");
    const auto slot = directory.path() / "slot.sav";
    atomicSaveSeed(slot, "previous complete save");
    const std::string first(256 * 1024, 'a');
    const std::string second(384 * 1024, 'b');
    AtomicSaveCommitBarrier barrier;
    bool firstSaved = false;
    bool secondSaved = false;
    auto write = [&](const std::string& bytes, bool& saved) {
        ScopedSaveWriteTestHook hook({meetAtAtomicCommit, &barrier});
        LocalFileSaveProvider provider;
        saved = provider.writeFile(slot.string(), bytes);
    };
    std::thread firstWriter([&] { write(first, firstSaved); });
    std::thread secondWriter([&] { write(second, secondSaved); });
    firstWriter.join();
    secondWriter.join();
    REQUIRE(barrier.temporaries.size() == 2);
    CHECK(barrier.temporaries[0] != barrier.temporaries[1]);
    REQUIRE(barrier.preparedBytes.size() == 2);
    CHECK(((barrier.preparedBytes[0] == first && barrier.preparedBytes[1] == second) ||
           (barrier.preparedBytes[0] == second && barrier.preparedBytes[1] == first)));
    CHECK((firstSaved || secondSaved));
    const auto finalBytes = atomicSaveRawBytes(slot);
    CHECK((finalBytes == first || finalBytes == second));
    CHECK(std::distance(std::filesystem::directory_iterator(directory.path()),
                        std::filesystem::directory_iterator()) == 1);
}

TEST_CASE("Storage atomic save: encrypted manager paths preserve the exact old envelope on failure") {
    AtomicSaveCryptoRegistration crypto;
    const std::array<uint8_t, 32> key{1, 2, 3, 4};
    for (const bool useProvider : {false, true}) {
        INFO("explicit local provider=", useProvider);
        TestPaths::ScopedTempDir directory("atomic_manager");
        const auto slot = directory.path() / "save_3.json";
        SaveManager manager;
        manager.init(directory.string());
        if (useProvider) manager.setSaveProvider(std::make_unique<LocalFileSaveProvider>());
        manager.setEncryptionKey(key.data());
        const json previous = {{"chapter", 1}, {"text", "last complete save"}};
        const json replacement = {{"chapter", 2}, {"text", "next complete save"}};
        REQUIRE(manager.save(3, previous, "old", 7));
        const auto oldEnvelope = atomicSaveRawBytes(slot);
        REQUIRE(oldEnvelope.substr(0, 4) == "CAES");
        for (auto stage : atomicSaveStages) {
            ScopedSaveWriteTestHook hook({rejectAtomicSaveStage, &stage});
            CHECK_FALSE(manager.save(3, replacement, "new", 8));
            CHECK(atomicSaveRawBytes(slot) == oldEnvelope);
            CHECK(manager.load(3) == previous);
        }
        REQUIRE(manager.save(3, replacement, "new", 8));
        SaveManager restarted;
        restarted.init(directory.string());
        restarted.setEncryptionKey(key.data());
        CHECK(restarted.load(3) == replacement);
    }
}

#if !defined(__EMSCRIPTEN__)
namespace {

constexpr int atomicSaveChildExit = 73;

bool interruptAtomicSaveStage(SaveWriteStage stage, const std::filesystem::path&, void* context) {
    if (static_cast<int>(stage) == *static_cast<int*>(context)) std::_Exit(atomicSaveChildExit);
    return true;
}

void runInterruptedAtomicWrite(const std::filesystem::path& slot, int stage) {
    ScopedSaveWriteTestHook hook({interruptAtomicSaveStage, &stage});
    LocalFileSaveProvider provider;
    const bool saved = provider.writeFile(slot.string(), std::string(256 * 1024, 'n'));
    std::_Exit(saved ? atomicSaveChildExit : atomicSaveChildExit + 1);
}

#ifdef _WIN32
constexpr wchar_t atomicSaveChildStageEnv[] = L"CAESURA_U7_SAVE_CHILD_STAGE";
constexpr wchar_t atomicSaveChildSlotEnv[] = L"CAESURA_U7_SAVE_CHILD_SLOT";

class AtomicSaveChildEnvironment {
public:
    AtomicSaveChildEnvironment(const wchar_t* key, const std::wstring& value) : key(key) {
        const DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
        if (size > 0) {
            previous.resize(size);
            GetEnvironmentVariableW(key, previous.data(), size);
            previous.resize(size - 1);
            existed = true;
        }
        installed = SetEnvironmentVariableW(key, value.c_str()) != 0;
    }
    ~AtomicSaveChildEnvironment() {
        SetEnvironmentVariableW(key, existed ? previous.c_str() : nullptr);
    }
    bool valid() const { return installed; }

private:
    const wchar_t* key;
    std::wstring previous;
    bool existed = false;
    bool installed = false;
};

int atomicSaveInterruptionChild(const std::filesystem::path& slot, int stage) {
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
                                           static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return -1;
    std::wstring command = L"\"" + std::wstring(executable.data()) +
        L"\" --test-case=\"Storage atomic save: interruption checkpoints recover a complete version\"";
    PROCESS_INFORMATION process{};
    {
        AtomicSaveChildEnvironment stageEnv(atomicSaveChildStageEnv, std::to_wstring(stage));
        AtomicSaveChildEnvironment slotEnv(atomicSaveChildSlotEnv, slot.wstring());
        if (!stageEnv.valid() || !slotEnv.valid()) return -1;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        if (!CreateProcessW(executable.data(), command.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return -1;
    }
    CloseHandle(process.hThread);
    const DWORD waited = WaitForSingleObject(process.hProcess, 10000);
    DWORD code = static_cast<DWORD>(-1);
    if (waited == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &code);
    } else {
        TerminateProcess(process.hProcess, 74);
        WaitForSingleObject(process.hProcess, 5000);
    }
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
#else
int atomicSaveInterruptionChild(const std::filesystem::path& slot, int stage) {
    const pid_t child = ::fork();
    if (child < 0) return -1;
    if (child == 0) runInterruptedAtomicWrite(slot, stage);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        const pid_t result = ::waitpid(child, &status, WNOHANG);
        if (result == child) return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (result < 0 && errno != EINTR) return -1;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ::kill(child, SIGKILL);
    while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return -1;
}
#endif

} // namespace

TEST_CASE("Storage atomic save: interruption checkpoints recover a complete version") {
#ifdef _WIN32
    std::array<wchar_t, 16> childStage{};
    if (GetEnvironmentVariableW(atomicSaveChildStageEnv, childStage.data(),
                                static_cast<DWORD>(childStage.size())) > 0) {
        std::array<wchar_t, 32768> childSlot{};
        REQUIRE(GetEnvironmentVariableW(atomicSaveChildSlotEnv, childSlot.data(),
                                         static_cast<DWORD>(childSlot.size())) > 0);
        runInterruptedAtomicWrite(std::filesystem::path(childSlot.data()),
                                  std::stoi(childStage.data()));
    }
#endif
    // Stages 0..4 stop before a syscall; stage 5 stops after a written chunk.
    // Stage 6 exits immediately after the
    // successful public call, covering both sides of the publication point.
    for (int stage = 0; stage <= 6; ++stage) {
        INFO("interruption stage=", stage);
        TestPaths::ScopedTempDir directory("atomic_interruption");
        const auto slot = directory.path() / "slot.sav";
        const std::string previous = "last complete save";
        const std::string replacement(256 * 1024, 'n');
        atomicSaveSeed(slot, previous);
        REQUIRE(atomicSaveInterruptionChild(slot, stage) == atomicSaveChildExit);
        LocalFileSaveProvider restarted;
        CHECK(restarted.readFile(slot.string()) == (stage == 6 ? replacement : previous));
        std::vector<std::filesystem::path> orphans;
        for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
            if (entry.path().filename().string().find(".caesura-save-tmp-") == 0)
                orphans.push_back(entry.path());
        }
        CHECK(orphans.size() == (stage > 0 && stage < 6 ? 1u : 0u));
        if (stage == static_cast<int>(SaveWriteStage::WriteProgress)) {
            REQUIRE(orphans.size() == 1);
            CHECK(atomicSaveRawBytes(orphans.front()) == std::string(64 * 1024, 'n'));
        }
        REQUIRE(restarted.writeFile(slot.string(), "a later complete save"));
        CHECK(restarted.readFile(slot.string()) == "a later complete save");
        // The child has exited and the replacement call has returned. There
        // are no live writers, so offline cleanup can remove known orphans.
        for (const auto& orphan : orphans) {
            CHECK(std::filesystem::exists(orphan));
            CHECK(std::filesystem::remove(orphan));
        }
        CHECK(std::distance(std::filesystem::directory_iterator(directory.path()),
                            std::filesystem::directory_iterator()) == 1);
    }
}
#endif
