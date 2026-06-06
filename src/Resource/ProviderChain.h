// ProviderChain — ordered chain of IAssetProvider, checked by descending priority.
#pragma once
#include "IAssetProvider.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace caesura {

class ProviderChain {
public:
    // Add a provider — re-sorts by priority descending after insertion.
    void addProvider(std::unique_ptr<IAssetProvider> provider);

    // Read: try each provider in priority order, return first non-empty result.
    std::vector<uint8_t> read(const std::string& path);

    // Exists: return true if any provider has the file.
    bool exists(const std::string& path);

    // Access to providers for iteration.
    const std::vector<std::unique_ptr<IAssetProvider>>& providers() const {
        return m_providers;
    }

private:
    std::vector<std::unique_ptr<IAssetProvider>> m_providers;

    void sortByPriority();
};

} // namespace caesura