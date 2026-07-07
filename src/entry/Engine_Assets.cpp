#include "../archive/CARCReader.h"
#include "../archive/CarcAssetProvider.h"
#include "../resource/AssetManager.h"

#include <cstdio>
#include <memory>

namespace Caesura {

void registerDefaultAssetProviders() {
    const char* carcFiles[] = {"data.carc", "game.carc", "patch.carc"};
    for (const char* fname : carcFiles) {
        auto reader = std::make_unique<carc::CARCReader>();
        if (reader->open(fname)) {
            AssetManager::instance().addProvider(
                std::make_unique<carc::CarcAssetProvider>(std::move(reader)));
            printf("[Engine] Registered CARC: %s\n", fname);
        }
    }
}

} // namespace Caesura
