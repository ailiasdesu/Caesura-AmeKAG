// CarcAssetProvider implementation
#include "CarcAssetProvider.h"

namespace caesura::carc {

CarcAssetProvider::CarcAssetProvider(std::unique_ptr<CARCReader> reader)
    : m_reader(std::move(reader))
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

} // namespace caesura::carc
// verify() is defined inline in CarcAssetProvider.h
// (returns m_reader->verifySignature())