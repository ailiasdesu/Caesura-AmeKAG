// ===========================================================================
//  Caesura (AmeKAG) — ISaveProvider.cpp
//  Local filesystem save provider implementation.
// ===========================================================================

#include "LocalFileSaveProvider.h"
#include "AtomicSaveFile.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <exception>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <fstream>
#include <cstdio>
#include <filesystem>

namespace Caesura {

std::string LocalFileSaveProvider::readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    in.seekg(0, std::ios::end);
    auto sz = in.tellg();
    if (sz <= 0 || static_cast<size_t>(sz) > 10 * 1024 * 1024) return "";
    std::string content(static_cast<size_t>(sz), '\0');
    in.seekg(0, std::ios::beg);
    in.read(&content[0], sz);
    if (!in.good()) return "";
    return content;
}

namespace detail {
namespace {

thread_local SaveWriteTestHook saveWriteHook;
std::atomic<unsigned long long> temporarySequence{0};

bool proceedWith(SaveWriteStage stage, const std::filesystem::path& temporary) {
    return !saveWriteHook.checkpoint ||
           saveWriteHook.checkpoint(stage, temporary, saveWriteHook.context);
}

unsigned long saveProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}

class TemporarySaveFile {
public:
    TemporarySaveFile() = default;
    TemporarySaveFile(const TemporarySaveFile&) = delete;
    TemporarySaveFile& operator=(const TemporarySaveFile&) = delete;

    ~TemporarySaveFile() {
        close();
        if (m_owned) {
            std::error_code ignored;
            std::filesystem::remove(m_path, ignored);
        }
    }

    bool create(const std::filesystem::path& target) {
        // Exclusive creation, not the name alone, establishes ownership.
        constexpr int maxAttempts = 128;
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            m_path = target.parent_path() /
                (".caesura-save-tmp-" + std::to_string(saveProcessId()) + "-" +
                 std::to_string(temporarySequence.fetch_add(1, std::memory_order_relaxed)));
            if (!proceedWith(SaveWriteStage::CreateTemporary, m_path)) return false;
#ifdef _WIN32
            m_handle = CreateFileW(m_path.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_handle != INVALID_HANDLE_VALUE) {
                m_owned = true;
                return true;
            }
            const DWORD error = GetLastError();
            // CREATE_NEW reports ACCESS_DENIED for a directory collision.
            // It is still someone else's name, so retry without removing it.
            if (error == ERROR_ACCESS_DENIED) {
                const DWORD attributes = GetFileAttributesW(m_path.c_str());
                if (attributes != INVALID_FILE_ATTRIBUTES &&
                    (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
            }
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) return false;
#else
            m_handle = ::open(m_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (m_handle >= 0) {
                m_owned = true;
                return true;
            }
            if (errno != EEXIST) return false;
#endif
        }
        return false;
    }

    bool write(const std::string& bytes) {
        if (!proceedWith(SaveWriteStage::Write, m_path)) return false;
        size_t offset = 0;
        while (offset < bytes.size()) {
            constexpr size_t writeChunkBytes = 64 * 1024;
            const size_t count = std::min(writeChunkBytes, bytes.size() - offset);
#ifdef _WIN32
            DWORD written = 0;
            if (!WriteFile(m_handle, bytes.data() + offset,
                           static_cast<DWORD>(count), &written, nullptr) ||
                written == 0) return false;
#else
            const auto written = ::write(m_handle, bytes.data() + offset, count);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) return false;
#endif
            offset += static_cast<size_t>(written);
            if (!proceedWith(SaveWriteStage::WriteProgress, m_path)) return false;
        }
        return true;
    }

    bool flush() {
        if (!proceedWith(SaveWriteStage::Flush, m_path)) return false;
#ifdef _WIN32
        return FlushFileBuffers(m_handle) != 0;
#else
        int result = 0;
        do { result = ::fsync(m_handle); } while (result < 0 && errno == EINTR);
        return result == 0;
#endif
    }

    bool closeForCommit() {
        return proceedWith(SaveWriteStage::Close, m_path) && close();
    }

    bool commit(const std::filesystem::path& target) {
        if (!proceedWith(SaveWriteStage::Replace, m_path)) return false;
#ifdef _WIN32
        // Do not use COPY_ALLOWED or a remove-then-rename fallback. Both paths
        // would discard the single filesystem publication point.
        const bool published = MoveFileExW(m_path.c_str(), target.c_str(),
                                            MOVEFILE_REPLACE_EXISTING |
                                            MOVEFILE_WRITE_THROUGH) != 0;
#else
        const bool published = ::rename(m_path.c_str(), target.c_str()) == 0;
#endif
        if (published) m_owned = false;
        return published;
    }

private:
    bool close() noexcept {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return true;
        const HANDLE handle = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return CloseHandle(handle) != 0;
#else
        if (m_handle < 0) return true;
        const int handle = m_handle;
        m_handle = -1;
        // Retrying close after EINTR may close a reused descriptor.
        return ::close(handle) == 0;
#endif
    }

    std::filesystem::path m_path;
    bool m_owned = false;
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    int m_handle = -1;
#endif
};

} // namespace

ScopedSaveWriteTestHook::ScopedSaveWriteTestHook(SaveWriteTestHook hook) noexcept
    : m_previous(saveWriteHook) {
    saveWriteHook = hook;
}

ScopedSaveWriteTestHook::~ScopedSaveWriteTestHook() {
    saveWriteHook = m_previous;
}

bool writeSaveFileAtomically(const std::string& path, const std::string& bytes) {
    constexpr size_t maxSaveBytes = 10 * 1024 * 1024;
    if (bytes.size() > maxSaveBytes) return false;
    try {
        const std::filesystem::path target(path);
        TemporarySaveFile temporary;
        return temporary.create(target) && temporary.write(bytes) &&
               temporary.flush() && temporary.closeForCommit() &&
               temporary.commit(target);
    } catch (const std::exception&) {
        // All fallible work precedes publication; RAII cleans only our own
        // temporary. A failed write must never delete the destination.
        return false;
    }
}

} // namespace detail

bool LocalFileSaveProvider::writeFile(const std::string& path, const std::string& content) {
    return detail::writeSaveFileAtomically(path, content);
}

bool LocalFileSaveProvider::deleteFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

std::vector<std::string> LocalFileSaveProvider::listFiles(const std::string& pattern) {
    std::vector<std::string> result;
    std::string p = pattern;
    auto slash = p.find_last_of("/\\");
    std::string dirPath = (slash != std::string::npos) ? p.substr(0, slash) : ".";
    std::string glob = (slash != std::string::npos) ? p.substr(slash + 1) : pattern;
    bool matchAll = (glob == "*" || glob == "*.*");

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            std::string fn = entry.path().filename().string();
            if (matchAll || fn == glob) {
                result.push_back(entry.path().string());
            }
        }
    } catch (const std::exception&) {
        // Directory may not exist — return empty
    }
    return result;
}

bool LocalFileSaveProvider::pushToCloud(const std::string&) {
    return false;
}

bool LocalFileSaveProvider::pullFromCloud(const std::string&) {
    return false;
}

} // namespace Caesura
