// CarcAssetProvider implementation
#include "CarcAssetProvider.h"

namespace Caesura::carc {

CarcAssetProvider::CarcAssetProvider(std::unique_ptr<CARCReader> reader,
                                     int priority,
                                     std::string sourceName)
    : m_reader(std::move(reader))
    , m_priority(priority)
    , m_sourceName(std::move(sourceName))
{}

std::vector<uint8_t> CarcAssetProvider::read(const std::string& path)
{
    if (!m_reader || !m_reader->isOpen()) return {};
    return m_reader->readFile(path);
}

bool CarcAssetProvider::exists(const std::string& path)
{
    if (!m_reader || !m_reader->isOpen()) return false;
    return m_reader->hasFile(path);
}

} // namespace Caesura::carc
// verify() is defined inline in CarcAssetProvider.h
// (returns m_reader->verifySignature())