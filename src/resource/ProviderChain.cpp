#include "ProviderChain.h"
#include "../debug/api/DebugLog.h"
#include <exception>

namespace Caesura {

void ProviderChain::addProvider(std::unique_ptr<IAssetProvider> provider)
{
    m_providers.push_back(std::move(provider));
    sortByPriority();
}

void ProviderChain::clear() noexcept
{
    m_providers.clear();
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
        try {
            if (!p->exists(path)) continue;
            auto data = p->read(path);
            if (!data.empty()) return data;
        } catch (const std::exception& e) {
            // A faulting provider is treated as "not servable": log and fall
            // through to the next provider (same semantics as a miss). Swallowing
            // here matches AsyncLoader/NullJobSystem exception isolation.
            DEBUG_WARN(SubSys::Resource, ErrCode::Ok,
                "[ProviderChain] provider %s faulted on read('%s'): %s",
                p->getSource().c_str(), path.c_str(), e.what());
        } catch (...) {
            DEBUG_WARN(SubSys::Resource, ErrCode::Ok,
                "[ProviderChain] provider %s faulted on read('%s'): unknown exception",
                p->getSource().c_str(), path.c_str());
        }
    }
    return {};
}

bool ProviderChain::exists(const std::string& path)
{
    for (auto& p : m_providers) {
        try {
            if (p->exists(path)) return true;
        } catch (const std::exception& e) {
            DEBUG_WARN(SubSys::Resource, ErrCode::Ok,
                "[ProviderChain] provider %s faulted on exists('%s'): %s",
                p->getSource().c_str(), path.c_str(), e.what());
        } catch (...) {
            DEBUG_WARN(SubSys::Resource, ErrCode::Ok,
                "[ProviderChain] provider %s faulted on exists('%s'): unknown exception",
                p->getSource().c_str(), path.c_str());
        }
    }
    return false;
}

} // namespace Caesura
