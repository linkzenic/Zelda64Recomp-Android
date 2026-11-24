#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <array>
#include "lib_recomp.hpp"
#include "defines_z64o.h"
#include "defines_ooto.h"
#include "defines_mmo.h"
#include "playermodelmanager_api.h"
#include "sha1.hpp"
#include "yaz0.hpp"

namespace fs = std::filesystem;

struct ModelDiskEntry {
    std::vector<char> modelData;
    std::array<bool, PMM_MODEL_TYPE_MAX> modelTypes;
    std::string internalName;
    std::string displayName;
    std::string authorName;
};

static std::vector<ModelDiskEntry> sModelDiskEntries;

static fs::path sPMMDir;

const fs::path MODEL_DIR("models");

const fs::path ASSET_DIR("assets");

const fs::path OOT_ASSET_DIR(ASSET_DIR / "oot");

extern "C" {
    DLLEXPORT uint32_t recomp_api_version = 1;
}

// Check if zobj is standalone.
// Modifies stream input position.
bool isValidStandaloneZobj(std::ifstream &f) {
    if (!f) {
        return false;
    }

    {
        const size_t MINIMUM_STANDALONE_ZOBJ_SIZE = 0x5800;

        f.seekg(MINIMUM_STANDALONE_ZOBJ_SIZE);

        char tmp;
        f.read(&tmp, 1);

        if (f.eof()) {
            return false;
        }
    }

    f.seekg(Z64O_MODLOADER_HEADER);

    const std::array<char, sizeof("MODLOADER64")> MODLOADER64_HEADER("MODLOADER64");
    std::array<char, MODLOADER64_HEADER.size()> headerBuf;

    f.read(headerBuf.data(), headerBuf.size());

    if (f.eof()) {
        return false;
    }

    if (memcmp(MODLOADER64_HEADER.data(), headerBuf.data(), MODLOADER64_HEADER.size() - 1) == 0) {
        switch (headerBuf[headerBuf.size() - 1]) {
            case MMO_FORM_BYTE_CHILD:
            case MMO_FORM_BYTE_ADULT:
            case OOTO_FORM_BYTE_ADULT:
            case OOTO_FORM_BYTE_CHILD:
                return true;
                break;
            default:
                break;
        }
    }

    return false;
}

RECOMP_DLL_FUNC(PMMZobj_setPMMDir) {
    sPMMDir = RECOMP_ARG_U8STR(0);

    sPMMDir = sPMMDir.parent_path() / "mod_data" / "yazmt_z64_playermodelmanager";

    fs::path modelDir = sPMMDir / "models";

    if (!fs::is_directory(modelDir)) {
        fs::create_directories(modelDir);
    }

    RECOMP_RETURN(bool, true);
}

void extractEmbeddedInfo(const std::vector<char> &v, ModelDiskEntry &entry) {
    const std::array<char, sizeof("PLAYERMODELINFO")> PLAYER_MODEL_INFO_HEADER_STR("PLAYERMODELINFO");

    const size_t EMBEDDED_INFO_HEADER_LOCATION = 0x5500;

    if (memcmp(v.data() + EMBEDDED_INFO_HEADER_LOCATION, PLAYER_MODEL_INFO_HEADER_STR.data(), PLAYER_MODEL_INFO_HEADER_STR.size() - 1) == 0) {
        const size_t EMBEDDED_INFO_VERSION_LOCATION = EMBEDDED_INFO_HEADER_LOCATION + PLAYER_MODEL_INFO_HEADER_STR.size() - 1;

        if (v[EMBEDDED_INFO_VERSION_LOCATION] == 1) {
            const size_t INTERNAL_NAME_FIELD_SIZE = 64;
            const size_t DISPLAY_NAME_FIELD_SIZE = 32;
            const size_t AUTHOR_NAME_FIELD_SIZE = 64;

            const size_t EMBEDDED_INTERNAL_NAME_LOCATION = EMBEDDED_INFO_HEADER_LOCATION + PLAYER_MODEL_INFO_HEADER_STR.size();
            const size_t EMBEDDED_DISPLAY_NAME_LOCATION = EMBEDDED_INTERNAL_NAME_LOCATION + INTERNAL_NAME_FIELD_SIZE;
            const size_t EMBEDDED_AUTHOR_NAME_LOCATION = EMBEDDED_DISPLAY_NAME_LOCATION + DISPLAY_NAME_FIELD_SIZE;

            auto setEntryStr = [v](size_t offset, size_t maxSize, std::string &result) {
                auto start = v.begin() + offset;
                auto end = start + maxSize;

                auto it = start;
                while (*it != '\0' && it != end) {
                    it++;
                }

                result = start != it ? std::string(start, it) : "";
            };

            setEntryStr(EMBEDDED_INTERNAL_NAME_LOCATION, INTERNAL_NAME_FIELD_SIZE, entry.internalName);
            setEntryStr(EMBEDDED_DISPLAY_NAME_LOCATION, DISPLAY_NAME_FIELD_SIZE, entry.displayName);
            setEntryStr(EMBEDDED_AUTHOR_NAME_LOCATION, AUTHOR_NAME_FIELD_SIZE, entry.authorName);
        }
    }
}

std::string rsubstr(const std::string& s, size_t size) {
    if (size >= s.size()) {
        return s;
    }

    size_t numToTrim = s.size() - size;

    return s.substr(numToTrim, size);
}

RECOMP_DLL_FUNC(PMMZobj_scanForDiskEntries) {
    if (!fs::is_directory(sPMMDir)) {
        RECOMP_RETURN(int, -1);
    }

    sModelDiskEntries.clear();

    fs::directory_entry folderEntry(sPMMDir / MODEL_DIR);

    if (!folderEntry.is_directory()) {
        RECOMP_RETURN(int, -1);
    }

    // Don't bother evaluating files >= 4 MiB
    // If a model is that big, it should be replaced through RT64 instead
    const size_t MAX_MODEL_SIZE = 4 * 1024 * 1024;

    // Only 128 MiB of models should be loaded at the absolute maximum
    // Recomp only has 512 MiB of RAM
    // This upper limmit should never be anywhere close to reached
    const size_t MAX_TOTAL_SIZE = 128 * 1024 * 1024;

    size_t runningTotalSize = 0;

    // TODO: This entire section needs a rewrite to support more model types
    for (const auto &dirEntry : fs::recursive_directory_iterator(folderEntry.path())) {
        if (dirEntry.is_regular_file() || (dirEntry.is_symlink() && fs::is_regular_file(fs::weakly_canonical(dirEntry.path())))) {
            if (runningTotalSize < MAX_TOTAL_SIZE && dirEntry.file_size() < MAX_MODEL_SIZE) {
                std::ifstream file(dirEntry.path(), std::ios::binary);

                if (isValidStandaloneZobj(file)) {
                    file.seekg(0);

                    std::vector<char> fileBuf(std::istreambuf_iterator<char>{file}, {});

                    ModelDiskEntry mde;

                    extractEmbeddedInfo(fileBuf, mde);

                    fs::path dirEntryPath = dirEntry.path();

                    if (mde.internalName == "") {
                        mde.internalName = fs::relative(dirEntryPath, sPMMDir).string();

                        if (mde.internalName.size() > PMM_MAX_INTERNAL_NAME_LENGTH) {
                            mde.internalName = rsubstr(mde.internalName, PMM_MAX_INTERNAL_NAME_LENGTH);
                        }
                    }

                    if (mde.internalName.size() < PMM_MAX_AUTHOR_NAME_LENGTH) {
                        mde.internalName += " "; // leave 1 byte of padding
                    }

                    if (mde.displayName == "") {
                        mde.displayName = dirEntryPath.stem().string();

                        if (mde.displayName.size() > PMM_MAX_DISPLAY_NAME_LENGTH) {
                            mde.displayName = mde.displayName.substr(0, PMM_MAX_DISPLAY_NAME_LENGTH);
                        }
                    }

                    if (mde.authorName == "") {
                        mde.authorName = "N/A";
                    }

                    mde.modelData = fileBuf;

                    mde.modelTypes.fill(false);

                    switch (mde.modelData[Z64O_FORM_BYTE]) {
                        case OOTO_FORM_BYTE_ADULT:
                        case MMO_FORM_BYTE_ADULT:
                            mde.modelTypes[PMM_MODEL_TYPE_ADULT] = true;
                            break;

                        case OOTO_FORM_BYTE_CHILD:
                        case MMO_FORM_BYTE_CHILD:
                            mde.modelTypes[PMM_MODEL_TYPE_CHILD] = true;
                            break;

                        default:
                            break;
                    }

                    sModelDiskEntries.push_back(mde);
                    runningTotalSize += fileBuf.size();
                }
            }
        }
    }

    std::sort(sModelDiskEntries.begin(), sModelDiskEntries.end(), [](const ModelDiskEntry &a, const ModelDiskEntry &b) {
        return a.displayName < b.displayName;
    });

    RECOMP_RETURN(int, sModelDiskEntries.size());
}

RECOMP_DLL_FUNC(PMMZobj_getNumDiskEntries) {
    RECOMP_RETURN(int, sModelDiskEntries.size());
}

ModelDiskEntry *getDiskEntry(int i) {
    return i >= sModelDiskEntries.size() || i < 0 ? nullptr : &sModelDiskEntries[i];
}

bool writeDataToRecompBuffer(uint8_t *rdram, recomp_context *ctx, PTR(char) recompBuf, int recompBufSize, const void *data, size_t dataSize) {
    if (!recompBuf) {
        return false;
    }

    if (recompBufSize < dataSize) {
        return false;
    }

    const char *bytes = static_cast<const char *>(data);

    for (size_t i = 0; i < dataSize; i++) {
        MEM_B(recompBuf, i) = bytes[i];
    }

    return true;
}

bool writeEntryDataToRecompBuffer(uint8_t *rdram, recomp_context *ctx, const void *data, size_t dataSize) {
    int bufLen = RECOMP_ARG(int, 2);

    // clang-format off
    PTR(char) buf = RECOMP_ARG(PTR(char), 1);
    // clang-format on

    return writeDataToRecompBuffer(rdram, ctx, buf, bufLen, data, dataSize);
}

bool writeEntryStringToRecompBuffer(uint8_t *rdram, recomp_context *ctx, const std::string &s) {
    return writeEntryDataToRecompBuffer(rdram, ctx, s.c_str(), s.size() + 1);
}

RECOMP_DLL_FUNC(PMMZobj_entryInternalNameLength) {
    int index = RECOMP_ARG(int, 0);

    ModelDiskEntry *entry = getDiskEntry(index);

    if (!entry) {
        RECOMP_RETURN(int, -1);
    }

    RECOMP_RETURN(int, entry->internalName.size());
}

RECOMP_DLL_FUNC(PMMZobj_entryDisplayNameLength) {
    int index = RECOMP_ARG(int, 0);

    ModelDiskEntry *entry = getDiskEntry(index);

    if (!entry) {
        RECOMP_RETURN(int, -1);
    }

    RECOMP_RETURN(int, entry->displayName.size());
}

RECOMP_DLL_FUNC(PMMZobj_entryAuthorNameLength) {
    int index = RECOMP_ARG(int, 0);

    ModelDiskEntry *entry = getDiskEntry(index);

    if (!entry) {
        RECOMP_RETURN(int, -1);
    }

    RECOMP_RETURN(int, entry->authorName.size());
}

RECOMP_DLL_FUNC(PMMZobj_writeInternalNameToBuffer) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    RECOMP_RETURN(bool, writeEntryStringToRecompBuffer(rdram, ctx, entry->internalName));
}

RECOMP_DLL_FUNC(PMMZobj_writeDisplayNameToBuffer) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    RECOMP_RETURN(bool, writeEntryStringToRecompBuffer(rdram, ctx, entry->displayName));
}

RECOMP_DLL_FUNC(PMMZobj_writeAuthorNameToBuffer) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    RECOMP_RETURN(bool, writeEntryStringToRecompBuffer(rdram, ctx, entry->authorName));
}

RECOMP_DLL_FUNC(PMMZobj_getEntryFileSize) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(int, -1);
    }

    RECOMP_RETURN(int, entry->modelData.size());
}

RECOMP_DLL_FUNC(PMMZobj_getEntryFileData) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    RECOMP_RETURN(bool, writeEntryDataToRecompBuffer(rdram, ctx, entry->modelData.data(), entry->modelData.size()));
}

RECOMP_DLL_FUNC(PMMZobj_createDirectory) {
    RECOMP_RETURN(bool, fs::create_directories(RECOMP_ARG_STR(0)));
}

RECOMP_DLL_FUNC(PMMZobj_isDirectoryExist) {
    RECOMP_RETURN(bool, fs::is_directory(RECOMP_ARG_STR(0)));
}

RECOMP_DLL_FUNC(PMMZobj_readEntryU8) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    int offset = RECOMP_ARG(int, 1);

    if (offset >= entry->modelData.size()) {
        RECOMP_RETURN(bool, false);
    }

    // clang-format off
    PTR(unsigned char) out = RECOMP_ARG(PTR(unsigned char), 2);
    // clang-format on

    if (!out) {
        RECOMP_RETURN(bool, false);
    }

    MEM_BU(static_cast<size_t>(out), 0) = entry->modelData[offset];

    RECOMP_RETURN(bool, true);
}

RECOMP_DLL_FUNC(PMMZobj_readEntryU16) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    int offset = RECOMP_ARG(int, 1);

    if (offset >= entry->modelData.size() + sizeof(uint16_t)) {
        RECOMP_RETURN(bool, false);
    }

    // clang-format off
    PTR(unsigned char) out = RECOMP_ARG(PTR(unsigned char), 2);
    // clang-format on

    if (!out) {
        RECOMP_RETURN(bool, false);
    }

    MEM_HU(static_cast<size_t>(out), 0) = (static_cast<uint16_t>(entry->modelData[offset]) << 8) | entry->modelData[offset + 1];

    RECOMP_RETURN(bool, true);
}

RECOMP_DLL_FUNC(PMMZobj_readEntryU32) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    int offset = RECOMP_ARG(int, 1);

    if (offset >= entry->modelData.size() + sizeof(uint32_t)) {
        RECOMP_RETURN(bool, false);
    }

    // clang-format off
    PTR(unsigned char) out = RECOMP_ARG(PTR(unsigned char), 2);
    // clang-format on

    if (!out) {
        RECOMP_RETURN(bool, false);
    }

    MEM_W(static_cast<size_t>(out), 0) =
        (static_cast<uint32_t>(entry->modelData[offset]) << 24) |
        (static_cast<uint32_t>(entry->modelData[offset + 1]) << 16) |
        (static_cast<uint32_t>(entry->modelData[offset + 2]) << 8) |
        (static_cast<uint32_t>(entry->modelData[offset + 3]));

    RECOMP_RETURN(bool, true);
}

RECOMP_DLL_FUNC(PMMZobj_isModelType) {
    ModelDiskEntry *entry = getDiskEntry(RECOMP_ARG(int, 0));

    if (!entry) {
        RECOMP_RETURN(bool, false);
    }

    int type = RECOMP_ARG(int, 1);

    if (type < 0 || type >= PMM_MODEL_TYPE_MAX) {
        RECOMP_RETURN(bool, false);
    }

    RECOMP_RETURN(bool, entry->modelTypes[type]);
}

RECOMP_DLL_FUNC(PMMZobj_clearDiskEntries) {
    sModelDiskEntries.clear();
    sModelDiskEntries.shrink_to_fit();
}

static const std::unordered_map<std::string, std::pair<std::string, unsigned>> DMA_DATA_OFFSETS = {
    {"ad69c91157f6705e8ab06c79fe08aad47bb57ba7", {"OOT NTSC 1.0 (US)", 0x7430}}, // NTSC 1.0 (US)
    {"d3ecb253776cd847a5aa63d859d8c89a2f37b364", {"OOT NTSC 1.1 (US)", 0x7430}}, // NTSC 1.1 (US)
    {"41b3bdc48d98c48529219919015a1af22f5057c2", {"OOT NTSC 1.2 (US)", 0x7960}}, // NTSC 1.2 (US)
    {"c892bbda3993e66bd0d56a10ecd30b1ee612210f", {"OOT NTSC 1.0 (JP)", 0x7430}}, // NTSC 1.0 (JP)
    {"dbfc81f655187dc6fefd93fa6798face770d579d", {"OOT NTSC 1.1 (JP)", 0x7430}}, // NTSC 1.1 (JP)
    {"fa5f5942b27480d60243c2d52c0e93e26b9e6b86", {"OOT NTSC 1.2 (JP)", 0x7960}}, // NTSC 1.2 (JP)
};

const int ROM_LITTLE_ENDIAN = 0x40;
const int ROM_BYTESWAPPED = 0x37;

void doLE2BE(std::string &rom) {
    for (size_t i = 0; i < rom.size(); i += 4) {
        char b0 = rom[i];
        char b1 = rom[i + 1];
        char b2 = rom[i + 2];
        char b3 = rom[i + 3];

        rom[i] = b3;
        rom[i + 1] = b2;
        rom[i + 2] = b1;
        rom[i + 3] = b0;
    }
}

void doBS2BE(std::string &rom) {
    for (size_t i = 0; i < rom.size(); i += 2) {
        char tmp = rom[i];
        rom[i] = rom[i + 1];
        rom[i + 1] = tmp;
    }
}

uint32_t toU32(const unsigned char *buf) {
    uint32_t b0 = buf[0];
    uint32_t b1 = buf[1];
    uint32_t b2 = buf[2];
    uint32_t b3 = buf[3];

    return b0 << 24 | b1 << 16 | b2 << 8 | b3;
}

#define SIZE_OF_YAZ0_HEADER 0x10

struct DMADataEntry {
    std::pair<uint32_t, uint32_t> virtualAddr;
    std::pair<uint32_t, uint32_t> physicalAddr;

    const bool isCompressed() {
        return physicalAddr.second != 0;
    }

    const bool isExist() {
        return physicalAddr.first == 0xFFFFFFFF;
    }

    const size_t getUncompressedSize() {
        if (virtualAddr.first > virtualAddr.second) {
            return 0;
        }

        return virtualAddr.second - virtualAddr.first;
    }

    const size_t getCompressedSize() {
        if (!isCompressed()) {
            return 0;
        }

        if (physicalAddr.first > physicalAddr.second) {
            return 0;
        }

        return physicalAddr.second - physicalAddr.first;
    }

    DMADataEntry() : virtualAddr({0, 0}), physicalAddr({0xFFFFFFFF, 0xFFFFFFFF}) {}

    static void fillEntry(DMADataEntry &entry, const char *rom, unsigned dmaStart, unsigned index) {
        const unsigned char *entryRaw = reinterpret_cast<const unsigned char *>(rom + dmaStart + index * (sizeof(virtualAddr) + sizeof(physicalAddr)));

        entry.virtualAddr.first = toU32(entryRaw);
        entry.virtualAddr.second = toU32(entryRaw + sizeof(uint32_t));
        entry.physicalAddr.first = toU32(entryRaw + sizeof(uint32_t) * 2);
        entry.physicalAddr.second = toU32(entryRaw + sizeof(uint32_t) * 3);

        // std::cout << std::hex << "fillEntry:\ndmaStart: 0x" << dmaStart << "\nindex: 0x" << index << "\n"
        //           << "ROM Offset: 0x" << dmaStart + index * (sizeof(virtualAddr) + sizeof(physicalAddr)) << "\n"
        //           << "Virtual Address: {Start: 0x" << entry.virtualAddr.first << ", End: 0x" << entry.virtualAddr.second << "}\n"
        //           << "Physical Address: {Start: 0x"
        //           << entry.physicalAddr.first << ", End: 0x" << entry.physicalAddr.second << "}\n";
    }

    DMADataEntry(const char *rom, unsigned dmaStart, unsigned index) {
        DMADataEntry::fillEntry(*this, rom, dmaStart, index);
    }
};

static struct {
    std::vector<char> rom;
    unsigned dmaStart;
} sZ64Rom;

bool unloadOOTRom() {
    sZ64Rom.dmaStart = 0;
    sZ64Rom.rom.clear();
    sZ64Rom.rom.shrink_to_fit();
    return true;
}

bool tryLoadOOTRom() {
    if (sPMMDir == "") {
        std::cout << "[INFO] PlayerModelManager_FSAddon: Called tryLoadOOTRom before setting PMM directory!\n";
        return false;
    }

    fs::path romPathZ64 = sPMMDir / "oot.z64";
    fs::path romPathN64 = sPMMDir / "oot.n64";
    fs::path romPathV64 = sPMMDir / "oot.v64";
    fs::path *romPath = nullptr;

    if (fs::exists(romPathZ64)) {
        romPath = &romPathZ64;
    } else if (fs::exists(romPathN64)) {
        romPath = &romPathN64;
    } else if (fs::exists(romPathV64)) {
        romPath = &romPathV64;
    }

    if (romPath) {
        std::cout << "[INFO] PlayerModelManager_FSAddon: Found OoT ROM at " << romPath->string() << '\n';

        std::string rom;

        {
            std::ifstream file(*romPath, std::ios::binary);
            rom.assign(std::istreambuf_iterator{file}, {});
        }

        if (rom.size() > 0) {
            if (rom[0] == ROM_LITTLE_ENDIAN) {
                std::cout << "[INFO] PlayerModelManager_FSAddon: ROM is little endian (.v64)! Converting to big endian (.z64)...\n";
                doLE2BE(rom);
            } else if (rom[0] == ROM_BYTESWAPPED) {
                std::cout << "[INFO] PlayerModelManager_FSAddon: ROM is byte-swapped (.n64)! Converting to big endian (.z64)...\n";
                doBS2BE(rom);
            }

            SHA1 checksum;
            checksum.update(rom);

            std::string checksumStr = checksum.final();

            auto dmaIt = DMA_DATA_OFFSETS.find(checksumStr);

            if (dmaIt != DMA_DATA_OFFSETS.end()) {
                std::cout << "[INFO] PlayerModelManager_FSAddon: ROM identified as " << dmaIt->second.first << '\n';

                sZ64Rom.dmaStart = dmaIt->second.second;
                sZ64Rom.rom.clear();
                rom.reserve(rom.size());
                for (size_t i = 0; i < rom.size(); ++i) {
                    sZ64Rom.rom.push_back(rom[i]);
                }

                return true;
            }
        }
    }

    unloadOOTRom();
    return false;
}

bool isRomLoaded() {
    return sZ64Rom.rom.size() > 0;
}

RECOMP_DLL_FUNC(PMMZobj_tryLoadOOTROM) {
    RECOMP_RETURN(bool, tryLoadOOTRom());
}

RECOMP_DLL_FUNC(PMMZobj_unloadOOTROM) {
    RECOMP_RETURN(bool, unloadOOTRom());
}

RECOMP_DLL_FUNC(PMMZobj_isOOTRomLoaded) {
    RECOMP_RETURN(bool, isRomLoaded());
}

std::vector<u8> extractZ64Object(const void *romData, unsigned dmaOffset, int dmaIndex, const std::string &checksum, const fs::path &outPath) {
    const char *rom = static_cast<const char *>(romData);

    DMADataEntry entry = DMADataEntry(rom, dmaOffset, dmaIndex);

    size_t entrySize = entry.getUncompressedSize();

    if (entrySize > SIZE_OF_YAZ0_HEADER) {
        std::cout << "[INFO] PlayerModelManager_FSAddon: Extracting " << outPath.filename() << " from 0x" << std::hex << entry.physicalAddr.first << std::endl;

        bool isExtracted = false;

        uint32_t physicalStart = entry.physicalAddr.first + SIZE_OF_YAZ0_HEADER;

        std::unique_ptr<u8> buf(new u8[entrySize]);

        if (entry.isCompressed()) {
            std::cout << "[INFO] PlayerModelManager_FSAddon: Extracting compressed file..." << std::endl;

            size_t entrySizeCompressed = entry.getCompressedSize();

            if (entrySizeCompressed) {
                yaz0_decompress(entrySize, entrySizeCompressed - SIZE_OF_YAZ0_HEADER, reinterpret_cast<const uint8_t *>(rom + physicalStart), buf.get());
                isExtracted = true;
            }
        } else {
            memcpy(buf.get(), rom + physicalStart, entrySize);
            isExtracted = true;
        }

        if (isExtracted) {
            SHA1 entryChecksum;
            entryChecksum.update(std::string(reinterpret_cast<char *>(buf.get()), entrySize));

            if (checksum == "" || entryChecksum.final() == checksum) {
                if (outPath.has_parent_path()) {
                    fs::create_directories(outPath.parent_path());
                }

                std::cout << "[INFO] PlayerModelManager_FSAddon: Writing DMA entry " << std::dec << dmaIndex << " to " << outPath.string() << std::endl;
                std::ofstream fsOut(outPath, std::ios::binary);
                fsOut.write(reinterpret_cast<char *>(buf.get()), entrySize);
                fsOut.flush();
                std::cout << "[INFO] PlayerModelManager_FSAddon: Finished writing DMA entry " << std::dec << dmaIndex << " to " << outPath.string() << std::endl;

                std::vector<u8> ret;

                ret.resize(entrySize);
                for (size_t i = 0; i < entrySize; ++i) {
                    ret[i] = buf.get()[i];
                }
                return ret;
            } else {
                std::cout << "[INFO] PlayerModelManager_FSAddon: Checksum of " << outPath.filename() << " extracted from ROM did not match!" << std::endl;
            }
        }
    }

    return std::vector<u8>({});
}

bool extractOrLoadCachedOOTObject(uint8_t *rdram, recomp_context *ctx, unsigned dmaIndex, const fs::path &assetPath, const std::string &assetChecksum) {
    // clang-format off
    PTR(char) rdramBuf = RECOMP_ARG(PTR(char), 0);
    // clang-format on

    if (!rdramBuf) {
        std::cout << "[INFO] PlayerModelManager_FSAddon: Cannot load OOT object " << assetPath.filename() << " into NULL pointer!" << std::endl;
        return false;
    }

    unsigned rdramBufSize = RECOMP_ARG(unsigned, 1);

    if (assetChecksum != "") {
        fs::directory_entry assetDirEnt(assetPath);

        if (assetDirEnt.exists() && !assetDirEnt.is_directory()) {
            std::cout << "[INFO] PlayerModelManager_FSAddon: Found cached OOT object " << assetPath.filename() << "!" << std::endl;

            std::ifstream file(assetPath, std::ios::binary);

            std::string assetCandidate(std::istreambuf_iterator<char>{file}, {});

            SHA1 candidateChecksum;
            candidateChecksum.update(assetCandidate);

            if (candidateChecksum.final() == assetChecksum && writeDataToRecompBuffer(rdram, ctx, rdramBuf, rdramBufSize, assetCandidate.data(), assetCandidate.size())) {
                return true;
            } else {
                std::cout << "[INFO] PlayerModelManager_FSAddon: Cached OOT object " << assetPath.filename() << " checksum did not match! Deleting and re-extracting..." << std::endl;
                file.close();
            }
        }
    }

    if (isRomLoaded()) {
        auto obj = extractZ64Object(sZ64Rom.rom.data(), sZ64Rom.dmaStart, dmaIndex, assetChecksum, assetPath);
        if (obj.size() > 0) {
            return writeDataToRecompBuffer(rdram, ctx, rdramBuf, rdramBufSize, obj.data(), obj.size());
        };
    }

    return false;
}

RECOMP_DLL_FUNC(PMMZobj_extractGameplayKeep) {
    RECOMP_RETURN(bool, extractOrLoadCachedOOTObject(rdram, ctx, 498, sPMMDir / OOT_ASSET_DIR / "gameplay_keep.zobj", "82f9c4c3224fa82884401fabf1b7e24b00d45883"));
}

RECOMP_DLL_FUNC(PMMZobj_extractAdultLink) {
    RECOMP_RETURN(bool, extractOrLoadCachedOOTObject(rdram, ctx, 502, sPMMDir / OOT_ASSET_DIR / "object_link_boy.zobj", "4ccb321f01568b4aa3a1af1572466a1a78d706ce"));
}

RECOMP_DLL_FUNC(PMMZobj_extractChildLink) {
    RECOMP_RETURN(bool, extractOrLoadCachedOOTObject(rdram, ctx, 503, sPMMDir / OOT_ASSET_DIR / "object_link_child.zobj", "30f0d604b60d3c11ba7f8821e08964d6a640d225"));
}

RECOMP_DLL_FUNC(PMMZobj_extractMirrorShieldRay) {
    RECOMP_RETURN(bool, extractOrLoadCachedOOTObject(rdram, ctx, 651, sPMMDir / OOT_ASSET_DIR / "object_mir_ray.zobj", "0ff156e2b37a6252037906599afe2533f1d9dcf7"));
}
