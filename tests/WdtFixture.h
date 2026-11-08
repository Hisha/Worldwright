#ifndef WORLDWRIGHT_TEST_WDT_FIXTURE_H
#define WORLDWRIGHT_TEST_WDT_FIXTURE_H

// Helpers for building synthetic WDT images from the publicly documented
// 3.3.5a layout. These helpers deliberately do not use WdtReader internals so
// that the tests exercise the reader against an independently built file.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace wdt_fixtures {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t kMverVersion = 18;
constexpr int kGridWidth = 64;
constexpr int kGridHeight = 64;
constexpr std::size_t kGridCellCount =
    static_cast<std::size_t>(kGridWidth) * static_cast<std::size_t>(kGridHeight);
constexpr std::size_t kMainEntrySize = 8;

inline void appendU32(Bytes& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

inline void setU32At(Bytes& out, std::size_t offset, std::uint32_t value) {
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

// WDT chunk IDs are 32-bit little-endian values, so a chunk called "MVER"
// appears as the bytes R, E, V, M in the file.
inline std::uint32_t chunkTag(const char* id) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(id[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(id[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(id[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(id[3]));
}

inline void appendChunkHeader(Bytes& out, const char* id, std::uint32_t declaredSize) {
    appendU32(out, chunkTag(id));
    appendU32(out, declaredSize);
}

inline void appendChunk(Bytes& out, const char* id, const Bytes& payload) {
    appendChunkHeader(out, id, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

inline Bytes makeMver(std::uint32_t version = kMverVersion) {
    Bytes payload;
    appendU32(payload, version);
    return payload;
}

inline Bytes makeMphd(std::uint32_t flags = 0) {
    Bytes payload(32, 0);
    setU32At(payload, 0, flags);
    return payload;
}

inline Bytes makeEmptyMain() {
    return Bytes(kGridCellCount * kMainEntrySize, 0);
}

inline void setMainEntry(Bytes& main, int x, int y, std::uint32_t flags = 1,
                         std::uint32_t asyncId = 0) {
    if (x < 0 || y < 0 || x >= kGridWidth || y >= kGridHeight ||
        main.size() < kGridCellCount * kMainEntrySize) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(y) * kGridWidth +
                              static_cast<std::size_t>(x);
    setU32At(main, index * kMainEntrySize, flags);
    setU32At(main, index * kMainEntrySize + 4, asyncId);
}

inline Bytes makeMain(const std::vector<std::pair<int, int>>& tiles,
                      std::uint32_t flags = 1) {
    Bytes main = makeEmptyMain();
    for (const auto& tile : tiles) {
        setMainEntry(main, tile.first, tile.second, flags);
    }
    return main;
}

inline Bytes makeWdt(std::initializer_list<Bytes> chunks) {
    Bytes out;
    for (const auto& chunk : chunks) {
        out.insert(out.end(), chunk.begin(), chunk.end());
    }
    return out;
}

inline Bytes makeWdtWithChunks(const std::vector<Bytes>& chunks) {
    Bytes out;
    for (const auto& chunk : chunks) {
        out.insert(out.end(), chunk.begin(), chunk.end());
    }
    return out;
}

inline Bytes makeValidWdt(const std::vector<std::pair<int, int>>& tiles = {},
                          std::uint32_t flags = 0,
                          std::uint32_t version = kMverVersion) {
    Bytes out;
    appendChunk(out, "MVER", makeMver(version));
    appendChunk(out, "MPHD", makeMphd(flags));
    appendChunk(out, "MAIN", makeMain(tiles));
    return out;
}

}  // namespace wdt_fixtures

#endif  // WORLDWRIGHT_TEST_WDT_FIXTURE_H
