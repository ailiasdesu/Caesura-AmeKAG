#pragma once

#include <filesystem>
#include <string>

namespace Caesura::detail {

// Storage-internal implementation shared by the provider and the no-provider
// SaveManager path. Publication is a single same-directory replacement.
// Interrupted temporary files are never promoted or reused. They may be
// removed offline only after all writers using the directory have stopped.
bool writeSaveFileAtomically(const std::string& path, const std::string& bytes);

enum class SaveWriteStage { CreateTemporary, Write, Flush, Close, Replace, WriteProgress };

// Narrow fault/checkpoint seam for the real writer. A false result fails the
// operation before the selected syscall (or after a written chunk at
// WriteProgress). The callback sees only this write's temporary path and
// never runs after the commit point.
struct SaveWriteTestHook {
    bool (*checkpoint)(SaveWriteStage, const std::filesystem::path&, void*) = nullptr;
    void* context = nullptr;
};

// Hooks apply only to the calling thread; nested scopes restore the previous
// callback. No hook is installed in ordinary production calls.
class ScopedSaveWriteTestHook {
public:
    explicit ScopedSaveWriteTestHook(SaveWriteTestHook hook) noexcept;
    ~ScopedSaveWriteTestHook();
    ScopedSaveWriteTestHook(const ScopedSaveWriteTestHook&) = delete;
    ScopedSaveWriteTestHook& operator=(const ScopedSaveWriteTestHook&) = delete;

private:
    SaveWriteTestHook m_previous;
};

} // namespace Caesura::detail
