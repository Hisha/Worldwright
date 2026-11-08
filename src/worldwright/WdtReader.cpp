#include "worldwright/WdtReader.h"

#include <fstream>
#include <string>
#include <vector>

namespace worldwright {
namespace {

// WoW chunk identifiers are stored as little-endian 32-bit values. On disk
// the bytes therefore appear in reverse order ("REVM" for MVER). Building the
// constant with the first character in the most significant byte keeps
// readU32() and the human-readable name in agreement.
constexpr std::uint32_t fourcc(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d));
}

constexpr std::uint32_t kMver = fourcc('M', 'V', 'E', 'R');
constexpr std::uint32_t kMphd = fourcc('M', 'P', 'H', 'D');
constexpr std::uint32_t kMain = fourcc('M', 'A', 'I', 'N');

constexpr std::size_t kChunkHeaderSize = 8;
constexpr std::uint32_t kMverPayloadSize = 4;
constexpr std::uint32_t kMphdPayloadSize = 32;
constexpr std::uint32_t kMainEntrySize = 8;
constexpr std::uint32_t kMainPayloadSize =
    static_cast<std::uint32_t>(WdtReader::kGridCellCount) * kMainEntrySize;
constexpr std::uint32_t kMainExistsFlag = 0x0001;
constexpr std::uintmax_t kMaxFileSize = 64u * 1024u * 1024u;

std::uint32_t readU32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

}  // namespace

bool WdtReader::load(const std::filesystem::path& path) {
    reset();
    m_sourceName = path.empty() ? std::string("<empty path>") : path.string();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return fail(WdtError::FileOpenFailed, "could not open file");
    }

    const std::streamoff end = file.tellg();
    if (end < 0) {
        return fail(WdtError::FileReadFailed, "could not determine file size");
    }

    const auto fileSize = static_cast<std::uintmax_t>(end);
    if (fileSize > kMaxFileSize) {
        return fail(WdtError::FileTooLarge, "file exceeds the 64 MiB reader limit");
    }

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    if (!buffer.empty() &&
        !file.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()))) {
        return fail(WdtError::FileReadFailed, "could not read the complete file");
    }

    return parseBuffer(buffer.empty() ? nullptr : buffer.data(), buffer.size());
}

bool WdtReader::loadFromMemory(const void* data, std::size_t size) {
    reset();
    m_sourceName = "<memory>";
    return parseBuffer(data, size);
}

bool WdtReader::parseBuffer(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (bytes == nullptr || size == 0) {
        return fail(WdtError::TruncatedData, "file is empty");
    }

    bool sawMver = false;
    bool sawMphd = false;
    bool sawMain = false;
    std::uint32_t version = 0;
    std::uint32_t headerFlags = 0;
    std::array<std::uint8_t, kGridCellCount> parsedTiles{};
    std::size_t parsedTileCount = 0;

    std::size_t offset = 0;
    while (offset < size) {
        if (size - offset < kChunkHeaderSize) {
            return fail(WdtError::TruncatedData,
                        "incomplete chunk header at offset " + std::to_string(offset));
        }

        const std::uint32_t id = readU32(bytes + offset);
        const std::uint32_t chunkSize = readU32(bytes + offset + 4);
        offset += kChunkHeaderSize;

        if (chunkSize > size - offset) {
            return fail(WdtError::TruncatedData,
                        "chunk at offset " + std::to_string(offset - kChunkHeaderSize) +
                            " declares " + std::to_string(chunkSize) +
                            " bytes but only " + std::to_string(size - offset) + " remain");
        }

        const std::uint8_t* payload = bytes + offset;

        if (id == kMver) {
            if (sawMver) {
                return fail(WdtError::DuplicateChunk, "duplicate MVER chunk");
            }
            sawMver = true;
            if (chunkSize != kMverPayloadSize) {
                return fail(WdtError::InvalidChunkSize,
                            "MVER chunk must be 4 bytes");
            }
            version = readU32(payload);
            if (version != kSupportedVersion) {
                return fail(WdtError::UnsupportedVersion,
                            "unsupported WDT version " + std::to_string(version) +
                                " (expected " + std::to_string(kSupportedVersion) + ")");
            }
        } else if (id == kMphd) {
            if (sawMphd) {
                return fail(WdtError::DuplicateChunk, "duplicate MPHD chunk");
            }
            sawMphd = true;
            if (chunkSize != kMphdPayloadSize) {
                return fail(WdtError::InvalidChunkSize,
                            "MPHD chunk must be 32 bytes");
            }
            headerFlags = readU32(payload);
        } else if (id == kMain) {
            if (sawMain) {
                return fail(WdtError::DuplicateChunk, "duplicate MAIN chunk");
            }
            sawMain = true;
            if (chunkSize != kMainPayloadSize) {
                return fail(WdtError::InvalidChunkSize,
                            "MAIN chunk must be " + std::to_string(kMainPayloadSize) +
                                " bytes");
            }

            std::array<std::uint8_t, kGridCellCount> tiles{};
            std::size_t tileCount = 0;
            for (std::size_t i = 0; i < kGridCellCount; ++i) {
                const std::uint32_t flags =
                    readU32(payload + i * kMainEntrySize);
                if ((flags & kMainExistsFlag) != 0) {
                    tiles[i] = 1;
                    ++tileCount;
                }
            }
            parsedTiles = tiles;
            parsedTileCount = tileCount;
        } else {
            // Optional and unknown chunks are skipped by their declared size.
        }

        offset += chunkSize;
    }

    if (!sawMver) {
        return fail(WdtError::MissingRequiredChunk, "required MVER chunk is missing");
    }
    if (!sawMphd) {
        return fail(WdtError::MissingRequiredChunk, "required MPHD chunk is missing");
    }
    if (!sawMain) {
        if ((headerFlags & kFlagGlobalWmo) != 0) {
            return fail(WdtError::UnsupportedWmoOnly,
                        "WMO-only WDT files have no terrain MAIN grid");
        }
        return fail(WdtError::MissingRequiredChunk, "required MAIN chunk is missing");
    }

    m_version = version;
    m_headerFlags = headerFlags;
    m_tiles = parsedTiles;
    m_tileCount = parsedTileCount;
    m_loaded = true;
    return true;
}

bool WdtReader::fail(WdtError error, std::string message) {
    m_loaded = false;
    m_error = error;
    m_errorMessage = m_sourceName.empty() ? std::move(message)
                                          : m_sourceName + ": " + message;
    return false;
}

void WdtReader::reset() noexcept {
    m_sourceName.clear();
    m_tiles.fill(0);
    m_tileCount = 0;
    m_version = 0;
    m_headerFlags = 0;
    m_loaded = false;
    m_error = WdtError::None;
    m_errorMessage.clear();
}

bool WdtReader::isLoaded() const noexcept {
    return m_loaded;
}

bool WdtReader::hasTile(int x, int y) const noexcept {
    if (!m_loaded || x < 0 || y < 0 || x >= kGridWidth || y >= kGridHeight) {
        return false;
    }
    const auto index = static_cast<std::size_t>(y) * kGridWidth +
                       static_cast<std::size_t>(x);
    return m_tiles[index] != 0;
}

std::size_t WdtReader::tileCount() const noexcept {
    return m_loaded ? m_tileCount : 0;
}

std::vector<std::pair<int, int>> WdtReader::existingTiles() const {
    std::vector<std::pair<int, int>> tiles;
    if (!m_loaded) {
        return tiles;
    }

    tiles.reserve(m_tileCount);
    for (int y = 0; y < kGridHeight; ++y) {
        for (int x = 0; x < kGridWidth; ++x) {
            const auto index = static_cast<std::size_t>(y) * kGridWidth +
                               static_cast<std::size_t>(x);
            if (m_tiles[index] != 0) {
                tiles.emplace_back(x, y);
            }
        }
    }
    return tiles;
}

bool WdtReader::tileBounds(int& minX, int& minY, int& maxX, int& maxY) const noexcept {
    if (!m_loaded || m_tileCount == 0) {
        return false;
    }

    int foundMinX = kGridWidth;
    int foundMinY = kGridHeight;
    int foundMaxX = -1;
    int foundMaxY = -1;

    for (int y = 0; y < kGridHeight; ++y) {
        for (int x = 0; x < kGridWidth; ++x) {
            const auto index = static_cast<std::size_t>(y) * kGridWidth +
                               static_cast<std::size_t>(x);
            if (m_tiles[index] == 0) {
                continue;
            }
            if (x < foundMinX) {
                foundMinX = x;
            }
            if (y < foundMinY) {
                foundMinY = y;
            }
            if (x > foundMaxX) {
                foundMaxX = x;
            }
            if (y > foundMaxY) {
                foundMaxY = y;
            }
        }
    }

    minX = foundMinX;
    minY = foundMinY;
    maxX = foundMaxX;
    maxY = foundMaxY;
    return true;
}

std::uint32_t WdtReader::version() const noexcept {
    return m_loaded ? m_version : 0;
}

std::uint32_t WdtReader::headerFlags() const noexcept {
    return m_loaded ? m_headerFlags : 0;
}

bool WdtReader::hasGlobalWmo() const noexcept {
    return m_loaded && (m_headerFlags & kFlagGlobalWmo) != 0;
}

bool WdtReader::hasMccv() const noexcept {
    return m_loaded && (m_headerFlags & kFlagHasMccv) != 0;
}

bool WdtReader::hasBigAlpha() const noexcept {
    return m_loaded && (m_headerFlags & kFlagHasBigAlpha) != 0;
}

WdtError WdtReader::error() const noexcept {
    return m_error;
}

const std::string& WdtReader::errorMessage() const noexcept {
    return m_errorMessage;
}

}  // namespace worldwright
