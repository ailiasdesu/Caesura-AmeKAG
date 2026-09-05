// Windows process-isolated allocation failures using real archive code.
// No global operator-new hooks and no oversized physical files/allocations.
#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/CryptoEngine.h"
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace Caesura::carc;
using Bytes = std::vector<uint8_t>;
constexpr size_t kMiB = 1024 * 1024;
constexpr size_t kHeadroom = 8 * kMiB;
constexpr size_t kOuterLimit = 128 * kMiB;
constexpr DWORD kChildTimeoutMs = 20000;
const Bytes kGood{'g', 'o', 'o', 'd'};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE handle) : value(handle) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

PROCESS_MEMORY_COUNTERS_EX memory() {
    PROCESS_MEMORY_COUNTERS_EX result{};
    result.cb = sizeof(result);
    require(GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&result), sizeof(result)), "memory query");
    return result;
}

ArchivePublicKey readKey(const char* path) {
    ArchivePublicKey key{};
    require(CryptoEngine::readPublicKey(path, key.data()), "public key read");
    return key;
}

Bytes readBytes(const char* path, size_t maximum) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    require(size > 0 && size <= static_cast<std::streamoff>(maximum), "bounded file size");
    Bytes bytes(static_cast<size_t>(size));
    input.seekg(0);
    require(bool(input.read(reinterpret_cast<char*>(bytes.data()), size)), "fixture read");
    return bytes;
}

void createArchive(const char* path, const char* pub, const Bytes& payload) {
    {
        CARCWriter writer;
        require(writer.create(path, "", pub), "writer create");
        require(writer.addFile("payload.bin", payload.data(), payload.size()), "writer payload");
        require(writer.addFile("good.txt", kGood.data(), kGood.size()), "writer intact entry");
        require(writer.finalize(), "writer finalize");
    }
    CARCReader reader;
    require(reader.open(path, readKey(pub)), "canonical host-selected open");
    require(reader.readFile("payload.bin") == payload, "canonical payload exact");
    require(reader.readFile("good.txt") == kGood, "canonical intact entry exact");
}

void indexCrypto(const ArchivePublicKey& key, uint32_t version, uint8_t* hash, uint8_t* nonce) {
    CryptoEngine::sha256(key.data(), key.size(), hash);
    std::memcpy(nonce, &version, sizeof(version));
    std::memcpy(nonce + sizeof(version), hash, AES_NONCE_SIZE - sizeof(version));
}

// The actual compressed content remains 8 KiB. Its signed original-size field
// reaches the allowed 512 MiB boundary; the capped allocation fails before
// decompression can inspect the deliberately inconsistent output length.
void prepareReadFixture() {
    createArchive("read.carc", "read.pub", Bytes(8192, 'A'));
    auto key = readKey("read.pub");
    auto bytes = readBytes("read.carc", kMiB);
    CARCHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    uint8_t hash[PATH_HASH_SIZE]{}, nonce[AES_NONCE_SIZE]{}, tag[AES_TAG_SIZE]{};
    indexCrypto(key, header.version, hash, nonce);
    const size_t encryptedSize = static_cast<size_t>(header.indexSize - AES_TAG_SIZE);
    auto plain = CryptoEngine::decrypt(bytes.data() + header.indexOffset, encryptedSize,
        hash, nonce, bytes.data() + header.indexOffset + encryptedSize);
    require(plain.size() == sizeof(uint32_t) + 2 * sizeof(FileEntry), "index length");
    FileEntry entry{};
    std::memcpy(&entry, plain.data() + sizeof(uint32_t), sizeof(entry));
    require(entry.originalSize == 8192 && entry.compressedSize < 8192, "compressed baseline");
    entry.originalSize = 512 * kMiB;
    std::memcpy(plain.data() + sizeof(uint32_t), &entry, sizeof(entry));
    uint8_t secret[64]{}, signature[SIGNATURE_SIZE]{};
    CryptoEngine::generateKeyPair(key.data(), secret); // never reuse an index key/nonce
    indexCrypto(key, header.version, hash, nonce);
    auto encrypted = CryptoEngine::encrypt(plain.data(), plain.size(), hash, nonce, tag);
    require(encrypted.size() == plain.size(), "index encryption");
    bytes.resize(static_cast<size_t>(header.indexOffset));
    bytes.insert(bytes.end(), encrypted.begin(), encrypted.end());
    bytes.insert(bytes.end(), tag, tag + sizeof(tag));
    require(CryptoEngine::sign(bytes.data(), bytes.size(), secret, signature), "signature create");
    SecureZeroMemory(secret, sizeof(secret));
    require(CryptoEngine::verify(bytes.data(), bytes.size(), key.data(), signature), "signature valid");
    bytes.insert(bytes.end(), signature, signature + sizeof(signature));
    bytes.insert(bytes.end(), key.begin(), key.end());
    std::ofstream output("read.carc", std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    output.close();
    require(output.good() && bytes.size() < kMiB, "bounded fixture write");
    require(CryptoEngine::writePublicKey("read.pub", key.data()), "public write");
    CARCReader reader;
    require(reader.open("read.carc", key), "signed at-cap open");
    require(reader.readFile("good.txt") == kGood, "signed intact entry exact");
    std::cout << "READ_FIXTURE\t" << bytes.size() << '\t' << entry.compressedSize
              << '\t' << entry.originalSize << "\tsignature_valid\n";
}

void prepareOpenFixture() {
    Bytes noise(12 * kMiB);
    uint32_t state = 0x12345678;
    for (auto& byte : noise) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        byte = static_cast<uint8_t>(state);
    }
    createArchive("open.carc", "open.pub", noise);
    const auto bytes = readBytes("open.carc", 13 * kMiB);
    require(bytes.size() > 12 * kMiB, "incompressible signed archive");
    std::cout << "OPEN_FIXTURE\t" << bytes.size() << "\tcanonical_exact\n";
}

size_t setMeasuredLimit(HANDLE job) {
    const auto baseline = memory();
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = baseline.PagefileUsage + kHeadroom;
    require(limits.ProcessMemoryLimit < 32 * kMiB, "unexpected child baseline commit");
    require(SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)), "inner set limit");
    require(AssignProcessToJobObject(job, GetCurrentProcess()), "inner job assignment");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION actual{};
    require(QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &actual, sizeof(actual), nullptr), "inner query limit");
    require(actual.ProcessMemoryLimit == limits.ProcessMemoryLimit, "limit mismatch");
    std::cout << "LIMIT\t" << baseline.PagefileUsage << '\t' << actual.ProcessMemoryLimit << std::endl;
    return actual.ProcessMemoryLimit;
}

bool attempt(CARCReader& reader, bool reading, const ArchivePublicKey& openKey) {
    try {
        if (reading) {
            const auto result = reader.readFile("payload.bin");
            std::cout << "RESULT\tread\treturned\t" << result.size() << std::endl;
            return result.empty();
        }
        const bool result = reader.open("open.carc", openKey);
        std::cout << "RESULT\topen\treturned\t" << result << std::endl;
        return !result;
    } catch (const std::bad_alloc&) {
        std::cout << "RESULT\t" << (reading ? "read" : "open") << "\tstd::bad_alloc" << std::endl;
    } catch (const std::exception& error) {
        std::cout << "RESULT\tother_exception\t" << error.what() << std::endl;
    }
    return false;
}

int runChild(bool reading) {
    const auto key = readKey("read.pub");
    const auto openKey = readKey("open.pub");
    CARCReader reader;
    require(reader.open("read.carc", key), "child signed at-cap open");
    require(reader.readFile("good.txt") == kGood, "child initial intact entry exact");
    const Handle innerJob(CreateJobObjectW(nullptr, nullptr));
    require(innerJob.value != nullptr, "inner CreateJobObject");
    setMeasuredLimit(innerJob.value);
    const bool returnedFailure = attempt(reader, reading, openKey);
    std::cout << "STATE\t" << reader.isOpen() << '\t' << reader.numFiles() << '\t'
              << reader.hasPublicKey() << '\t' << reader.hasFile("good.txt") << std::endl;
    const bool stateCorrect = reading
        ? reader.isOpen() && reader.numFiles() == 2 && reader.readFile("good.txt") == kGood
        : !reader.isOpen() && reader.numFiles() == 0 && !reader.hasPublicKey()
            && !reader.hasFile("good.txt") && reader.readFile("good.txt").empty();
    reader.close();
    const bool closeClean = !reader.isOpen() && reader.numFiles() == 0 && !reader.hasPublicKey();
    const bool reopen = reader.open("read.carc", key);
    const bool recovered = reopen && reader.readFile("good.txt") == kGood;
    const auto after = memory();
    std::cout << "RECOVERY\t" << stateCorrect << '\t' << closeClean << '\t' << reopen << '\t' << recovered << '\n'
              << "PEAK\t" << after.PeakWorkingSetSize << '\t' << after.PeakPagefileUsage << '\n'
              << "CONTRACT\t" << (returnedFailure && stateCorrect && closeClean && recovered) << std::endl;
    return returnedFailure && stateCorrect && closeClean && recovered ? 0 : 10;
}

int launch(bool reading) {
    const Handle outerJob(CreateJobObjectW(nullptr, nullptr));
    require(outerJob.value != nullptr, "outer CreateJobObject");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    limits.ProcessMemoryLimit = kOuterLimit;
    require(SetInformationJobObject(outerJob.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)), "outer set limit");
    wchar_t executable[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    require(length > 0 && length < 32768, "executable path");
    std::wstring command = L"\"" + std::wstring(executable) + (reading ? L"\" child read" : L"\" child open");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    require(CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process), "CreateProcess");
    const Handle processHandle(process.hProcess), threadHandle(process.hThread);
    if (!AssignProcessToJobObject(outerJob.value, processHandle.value)) {
        const DWORD error = GetLastError();
        TerminateProcess(processHandle.value, 90);
        WaitForSingleObject(processHandle.value, 1000);
        std::cerr << "OUTER_ASSIGN_FAILED\t" << error << '\n';
        return 11;
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    require(GetProcessTimes(processHandle.value, &created, &exited, &kernel, &user), "child creation time");
    std::cout << "CHILD\t" << process.dwProcessId << '\t'
              << ((uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime)
              << '\t' << kOuterLimit << '\t' << kChildTimeoutMs << std::endl;
    require(ResumeThread(threadHandle.value) != DWORD(-1), "ResumeThread");
    const DWORD waited = WaitForSingleObject(processHandle.value, kChildTimeoutMs);
    if (waited != WAIT_OBJECT_0) {
        TerminateJobObject(outerJob.value, 91);
        WaitForSingleObject(processHandle.value, 1000);
    }
    DWORD code = 92;
    require(GetExitCodeProcess(processHandle.value, &code), "child exit code");
    std::cout << "CHILD_EXIT\t" << code << '\t' << waited << std::endl;
    return waited == WAIT_OBJECT_0 ? static_cast<int>(code) : 12;
}
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try {
        if (argc == 2 && std::string(argv[1]) == "prepare") {
            prepareReadFixture();
            prepareOpenFixture();
            return 0;
        }
        require(argc == 3, "Usage: prepare | launch read|open | child read|open");
        const std::string mode(argv[1]), phase(argv[2]);
        require(phase == "read" || phase == "open", "unknown phase");
        if (mode == "child") return runChild(phase == "read");
        require(mode == "launch", "unknown mode");
        return launch(phase == "read");
    } catch (const std::exception& error) {
        std::cerr << "PROBE_ERROR\t" << error.what() << '\n';
        return 13;
    }
}
