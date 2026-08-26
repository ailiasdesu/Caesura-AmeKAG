// carc_pack.exe — CARC archive tool
// Usage:
//   carc_pack.exe <input_dir> <output.carc> [public.key] [private.key]
//   carc_pack.exe list <archive.carc> [public.key]
//   carc_pack.exe extract <archive.carc> <out_dir> [public.key]
//
// If key paths are omitted, keys are embedded in the archive only.
// Key files are raw binary (32 bytes public, 64 bytes private).
// `list` prints archive file names one per line (machine-readable, for
// the KAG3 importer's --carc mode); `extract` restores all files.

#include "archive/CARCReader.h"
#include "archive/CARCWriter.h"
#include "archive/DeltaCARC.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static void printUsage() {
    std::cerr << "Usage: carc_pack.exe <input_dir> <output.carc> [public.key] [private.key]\n"
              << "       carc_pack.exe pack <input_dir> <output.carc> [public.key] [private.key]\n"
              << "       carc_pack.exe list <archive.carc> [public.key]\n"
              << "       carc_pack.exe extract <archive.carc> <out_dir> [public.key] [--path rel]\n"
              << "       carc_pack.exe delta <old.carc> <new.carc> <delta.carc>\n"
              << "       carc_pack.exe apply <base.carc> <delta.carc> <output.carc>\n"
              << "       carc_pack.exe verify-delta <delta.carc>\n"
              << "  input_dir      directory to pack\n"
              << "  output.carc    CARC archive to create\n"
              << "  public.key     (optional) path to public key (verify on read)\n"
              << "  private.key    (optional) path to save private key\n"
              << "  list           print the archive's file names (one per line)\n"
              << "  extract        extract all files into out_dir\n"
              << "  delta/patch    generate differential patch between two CARC archives\n"
              << "  apply          apply differential patch to base archive to produce output archive\n"
              << "  verify-delta   verify cryptographic integrity and structure of delta patch\n";
}

static std::string relativePath(const fs::path& filePath, const fs::path& baseDir)
{
    return fs::relative(filePath, baseDir).generic_string();
}

static int doDelta(int argc, char* argv[])
{
    if (argc < 5) {
        std::cerr << "Usage: carc_pack.exe delta <old.carc> <new.carc> <delta.carc>\n";
        return 1;
    }
    std::string oldPath = argv[2];
    std::string newPath = argv[3];
    std::string deltaPath = argv[4];

    if (!fs::exists(oldPath)) {
        std::cerr << "Error: old archive not found: " << oldPath << "\n";
        return 1;
    }
    if (!fs::exists(newPath)) {
        std::cerr << "Error: new archive not found: " << newPath << "\n";
        return 1;
    }

    if (!Caesura::carc::DeltaCARC::generate(oldPath, newPath, deltaPath)) {
        std::cerr << "Error: failed to generate delta patch from " << oldPath
                  << " to " << newPath << "\n";
        return 1;
    }
    std::cout << "Created differential patch: " << deltaPath << "\n";
    return 0;
}

static int doApply(int argc, char* argv[])
{
    if (argc < 5) {
        std::cerr << "Usage: carc_pack.exe apply <base.carc> <delta.carc> <output.carc>\n";
        return 1;
    }
    std::string basePath = argv[2];
    std::string deltaPath = argv[3];
    std::string outputPath = argv[4];

    if (!fs::exists(basePath)) {
        std::cerr << "Error: base archive not found: " << basePath << "\n";
        return 1;
    }
    if (!fs::exists(deltaPath)) {
        std::cerr << "Error: delta patch not found: " << deltaPath << "\n";
        return 1;
    }

    if (!Caesura::carc::DeltaCARC::apply(basePath, deltaPath, outputPath)) {
        std::cerr << "Error: failed to apply delta patch " << deltaPath
                  << " to base " << basePath << "\n";
        return 1;
    }
    std::cout << "Applied delta patch: " << outputPath << "\n";
    return 0;
}

static int doVerifyDelta(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: carc_pack.exe verify-delta <delta.carc>\n";
        return 1;
    }
    std::string deltaPath = argv[2];
    if (!fs::exists(deltaPath)) {
        std::cerr << "Error: delta patch file not found: " << deltaPath << "\n";
        return 1;
    }

    if (!Caesura::carc::DeltaCARC::verify(deltaPath)) {
        std::cerr << "Error: delta patch verification failed: " << deltaPath << "\n";
        return 1;
    }
    std::cout << "Delta patch verified successfully: " << deltaPath << "\n";
    return 0;
}

static int doList(int argc, char* argv[])
{
    if (argc < 3) {
        printUsage();
        return 1;
    }
    std::string carcPath = argv[2];
    std::string pubKey = (argc > 3) ? argv[3] : "";
    Caesura::carc::CARCReader reader;
    if (!reader.open(carcPath, pubKey)) {
        std::cerr << "Error: cannot open archive (bad key or corrupt file): "
                  << carcPath << "\n";
        return 1;
    }
    for (const auto& name : reader.fileList()) {
        std::cout << name << "\n";
    }
    return 0;
}

static int doExtract(int argc, char* argv[])
{
    // carc_pack.exe extract <archive.carc> <out_dir> [public.key] [--path rel]
    if (argc < 4) {
        printUsage();
        return 1;
    }
    std::string carcPath = argv[2];
    std::string outDir = argv[3];
    std::string pubKey = "";
    std::string onlyPath = "";
    for (int i = 4; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--path" && i + 1 < argc) {
            onlyPath = argv[++i];
        } else if (pubKey.empty()) {
            pubKey = a;
        }
    }
    Caesura::carc::CARCReader reader;
    if (!reader.open(carcPath, pubKey)) {
        std::cerr << "Error: cannot open archive (bad key or corrupt file): "
                  << carcPath << "\n";
        return 1;
    }
    // With --path: extract exactly one known relative path (the CARC
    // stores only path hashes, so the caller must know the name).
    if (!onlyPath.empty()) {
        if (!reader.hasFile(onlyPath)) {
            std::cerr << "Error: not in archive: " << onlyPath << "\n";
            return 1;
        }
        auto data = reader.readFile(onlyPath);
        if (data.empty()) {
            std::cerr << "Error: empty read for " << onlyPath << "\n";
            return 1;
        }
        fs::path out = fs::path(outDir) / onlyPath;
        fs::create_directories(out.parent_path());
        std::ofstream f(out, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
        std::cout << "Extracted: " << onlyPath << "\n";
        return 0;
    }
    // Full extract: dump everything by its path hash name (the original
    // names are not stored; callers with --path get real names back).
    fs::create_directories(outDir);
    size_t count = 0;
    for (const auto& name : reader.fileList()) {
        // hex hash -> bytes -> readFileByHash
        uint8_t hash[Caesura::carc::PATH_HASH_SIZE] = {0};
        bool okHex = name.size() == Caesura::carc::PATH_HASH_SIZE * 2;
        if (okHex) {
            for (size_t i = 0; i < Caesura::carc::PATH_HASH_SIZE; i++) {
                auto hexToNib = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                int hi = hexToNib(name[i * 2]);
                int lo = hexToNib(name[i * 2 + 1]);
                if (hi < 0 || lo < 0) { okHex = false; break; }
                hash[i] = static_cast<uint8_t>((hi << 4) | lo);
            }
        }
        if (!okHex) {
            std::cerr << "Warning: skip malformed hash " << name << "\n";
            continue;
        }
        auto data = reader.readFileByHash(hash);
        if (data.empty()) {
            std::cerr << "Warning: empty read for " << name << "\n";
            continue;
        }
        fs::path out = fs::path(outDir) / name;
        std::ofstream f(out, std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "Error: cannot write " << out << "\n";
            return 1;
        }
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
        ++count;
    }
    std::cout << "Extracted: " << count << " files into " << outDir << "\n";
    return 0;
}

int main(int argc, char* argv[])
{
    // subcommand dispatch
    if (argc >= 2) {
        std::string cmd = argv[1];
        if (cmd == "list") {
            return doList(argc, argv);
        }
        if (cmd == "extract") {
            return doExtract(argc, argv);
        }
        if (cmd == "delta" || cmd == "patch") {
            return doDelta(argc, argv);
        }
        if (cmd == "apply") {
            return doApply(argc, argv);
        }
        if (cmd == "verify-delta" || cmd == "verify") {
            return doVerifyDelta(argc, argv);
        }
    }

    // ---- pack mode (original or explicit 'pack' subcommand) ----
    int argOffset = 0;
    if (argc >= 2 && std::string(argv[1]) == "pack") {
        argOffset = 1;
    }

    if (argc < 3 + argOffset) {
        printUsage();
        return 1;
    }

    std::string inputDir = argv[1 + argOffset];
    std::string outputCarc = argv[2 + argOffset];
    std::string pubKeyPath = (argc > 3 + argOffset) ? argv[3 + argOffset] : "";
    std::string privKeyPath = (argc > 4 + argOffset) ? argv[4 + argOffset] : "";

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
