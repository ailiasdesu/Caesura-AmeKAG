#include "ProviderChain.h"

namespace caesura {

void ProviderChain::addProvider(std::unique_ptr<IAssetProvider> provider)
{
    m_providers.push_back(std::move(provider));
    sortByPriority();
}

void ProviderChain::sortByPriority()
{
    std::sort(m_providers.begin(), m_providers.end(),
              [](const std::unique_ptr<IAssetProvider>& a,
                 const std::unique_ptr<IAssetProvider>& b) {
                  return a->priority() > b->priority();
              });
}

std::vector<uint8_t> ProviderChain::read(const std::string& path)
{
    for (auto& p : m_providers) {
        if (p->exists(path)) {
            auto data = p->read(path);
            if (!data.empty()) return data;
        }
    }
    return {};
}

bool ProviderChain::exists(const std::string& path)
{
    for (auto& p : m_providers) {
        if (p->exists(path)) return true;
    }
    return false;
}

} // namespace caesura