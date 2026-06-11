// carc_pack.exe �� CARC archive packer
// Usage: carc_pack.exe <input_dir> <output.carc> [public_key_out] [private_key_out]
//
// If key paths are omitted, keys are embedded in the archive only.
// Key files are raw binary (32 bytes public, 64 bytes private).

#include "archive/CARCWriter.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static void printUsage() {
    std::cerr << "Usage: carc_pack.exe <input_dir> <output.carc> [public.key] [private.key]\n"
              << "  input_dir      �� directory to pack\n"
              << "  output.carc    �� CARC archive to create\n"
              << "  public.key     �� (optional) path to save public key\n"
              << "  private.key    �� (optional) path to save private key\n";
}

static std::string relativePath(const fs::path& filePath, const fs::path& baseDir)
{
    return fs::relative(filePath, baseDir).generic_string();
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string inputDir = argv[1];
    std::string outputCarc = argv[2];
    std::string pubKeyPath = (argc > 3) ? argv[3] : "";
    std::string privKeyPath = (argc > 4) ? argv[4] : "";

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "Error: input directory not found: " << inputDir << "\n";
        return 1;
    }

    Caesura::carc::CARCWriter writer;
    if (!writer.create(outputCarc, privKeyPath, pubKeyPath)) {
        std::cerr << "Error: failed to create output file: " << outputCarc << "\n";
        return 1;
    }

    std::cout << "Packing directory: " << inputDir << "\n";
    size_t fileCount = 0;

    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;

        fs::path filePath = entry.path();
        std::string relPath = relativePath(filePath, inputDir);

        // Read file
        std::ifstream in(filePath, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            std::cerr << "Warning: cannot read " << filePath << "\n";
            continue;
        }
        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(static_cast<size_t>(size));
        in.read(reinterpret_cast<char*>(data.data()), size);

        if (!writer.addFile(relPath, data.data(), data.size())) {
            std::cerr << "Error: failed to add file: " << relPath << "\n";
            return 1;
        }
        ++fileCount;
    }

    if (!writer.finalize()) {
        std::cerr << "Error: finalize failed\n";
        return 1;
    }

    std::cout << "Created: " << outputCarc << " (" << fileCount << " files)\n";
    if (!pubKeyPath.empty())
        std::cout << "Public key: " << pubKeyPath << "\n";
    if (!privKeyPath.empty())
        std::cout << "Private key: " << privKeyPath << "\n";

    return 0;
}