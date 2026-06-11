// CarcAssetProvider -- wraps CARCReader as an IAssetProvider
#pragma once
#include "Resource/api/IAssetProvider.h"
#include "CARCReader.h"
#include <memory>

namespace Caesura::carc {

class CarcAssetProvider : public ::caesura::IAssetProvider {
public:
    explicit CarcAssetProvider(std::unique_ptr<CARCReader> reader);

    std::vector<uint8_t> read(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::string getSource() const override { return "CARC"; }
    int priority() const override { return 10; }
    bool verify() override {
        if (!m_reader || !m_reader->isOpen()) return false;
        return m_reader->verifySignature();
    }

private:
    std::unique_ptr<CARCReader> m_reader;
};

} // namespace Caesura::carc
