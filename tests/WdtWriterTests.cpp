#include "worldwright/WdtReader.h"
#include "worldwright/WdtWriter.h"
#include "worldwright/WorldDefinition.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

int g_checks = 0;
int g_failures = 0;

void recordFailure(const std::string& message, const char* file, int line) {
    ++g_failures;
    std::cerr << "  FAIL " << file << ':' << line << ": " << message << '\n';
}

void check(bool condition, const char* expression, const char* file, int line) {
    ++g_checks;
    if (!condition) {
        recordFailure(std::string("expected: ") + expression, file, line);
    }
}

template <typename A, typename B>
void checkEqual(const A& actual, const B& expected, const char* actualExpression,
                const char* expectedExpression, const char* file, int line) {
    ++g_checks;
    if (!(actual == expected)) {
        std::ostringstream message;
        message << "expected " << actualExpression << " == " << expectedExpression
                << "; actual: " << actual << ", expected: " << expected;
        recordFailure(message.str(), file, line);
    }
}

#define CHECK(condition) check((condition), #condition, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

class TempDir {
public:
    TempDir() {
        static unsigned long long counter = 0;
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("worldwright-wdt-writer-tests-" + std::to_string(stamp) + "-" +
                  std::to_string(counter++));
        std::filesystem::create_directories(m_path);
    }

    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

std::filesystem::path writeFile(const std::filesystem::path& path,
                                const Bytes& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(data.data()),
                             static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("failed to write " + path.string());
    }
    return path;
}

Bytes readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("failed to open " + path.string());
    }
    const auto end = file.tellg();
    if (end < 0) {
        throw std::runtime_error("failed to size " + path.string());
    }
    Bytes data(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!data.empty() &&
        !file.read(reinterpret_cast<char*>(data.data()),
                   static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("failed to read " + path.string());
    }
    return data;
}

std::uint32_t readU32(const Bytes& data, std::size_t offset) {
    if (offset + 4 > data.size()) {
        return 0xFFFFFFFFu;
    }
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

struct ChunkInfo {
    std::string id;
    std::uint32_t size = 0;
    std::size_t headerOffset = 0;
    std::size_t payloadOffset = 0;
};

// Independent chunk walker: it does not use WdtReader or WdtWriter internals.
std::vector<ChunkInfo> parseChunks(const Bytes& data) {
    std::vector<ChunkInfo> chunks;
    std::size_t offset = 0;

    while (offset < data.size()) {
        if (data.size() - offset < 8) {
            break;
        }

        std::string id;
        for (std::size_t i = 0; i < 4; ++i) {
            id.push_back(static_cast<char>(data[offset + i]));
        }
        std::reverse(id.begin(), id.end());

        const std::uint32_t size = readU32(data, offset + 4);
        chunks.push_back(ChunkInfo{id, size, offset, offset + 8});

        if (size > data.size() - offset - 8) {
            break;
        }
        offset += 8 + size;
    }

    return chunks;
}

std::vector<std::pair<int, int>> toPairs(
    const worldwright::WorldDefinition& world) {
    std::vector<std::pair<int, int>> pairs;
    pairs.reserve(world.tileCount());
    for (const auto& tile : world.enabledTiles()) {
        pairs.emplace_back(tile.x, tile.y);
    }
    return pairs;
}

// Checks the raw MAIN payload against the model without using WdtReader.
std::size_t mainMismatchCount(const Bytes& data,
                              const worldwright::WorldDefinition& world) {
    constexpr std::size_t kMainPayloadOffset = 60;
    if (data.size() < kMainPayloadOffset +
                          worldwright::WdtWriter::kMainPayloadSize) {
        return worldwright::WdtWriter::kMainEntryCount + 1;
    }

    std::size_t mismatches = 0;
    for (int y = 0; y < worldwright::WorldDefinition::GridHeight; ++y) {
        for (int x = 0; x < worldwright::WorldDefinition::GridWidth; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) *
                    worldwright::WorldDefinition::GridWidth +
                static_cast<std::size_t>(x);
            const std::size_t offset =
                kMainPayloadOffset + index *
                                         worldwright::WdtWriter::kMainEntrySize;
            const std::uint32_t expected =
                world.hasTile(x, y) ? 1u : 0u;
            if (readU32(data, offset) != expected) {
                ++mismatches;
            }
            if (readU32(data, offset + 4) != 0u) {
                ++mismatches;
            }
        }
    }
    return mismatches;
}

std::size_t countMainEntries(const Bytes& data, std::uint32_t flag) {
    constexpr std::size_t kMainPayloadOffset = 60;
    std::size_t count = 0;
    for (std::size_t index = 0;
         index < worldwright::WdtWriter::kMainEntryCount; ++index) {
        const std::size_t offset =
            kMainPayloadOffset + index * worldwright::WdtWriter::kMainEntrySize;
        if (readU32(data, offset) == flag) {
            ++count;
        }
    }
    return count;
}

std::size_t countTempFiles(const std::filesystem::path& directory) {
    std::size_t count = 0;
    std::error_code error;
    for (const auto& entry :
         std::filesystem::directory_iterator(directory, error)) {
        const std::string name = entry.path().filename().string();
        if (name.find(".worldwright-") != std::string::npos) {
            ++count;
        }
    }
    return count;
}

void testEmptyWorld() {
    std::cout << "Test: empty world definition\n";
    worldwright::WorldDefinition world;

    CHECK_EQ(world.tileCount(), std::size_t{0});
    CHECK(world.empty());
    CHECK(!world.hasTile(0, 0));
    CHECK(!world.hasTile(63, 63));
    CHECK(world.enabledTiles().empty());
    CHECK(!world.bounds().has_value());
    CHECK_EQ(worldwright::WorldDefinition::TileCapacity, std::size_t{4096});
}

void testEnableDisable() {
    std::cout << "Test: enable and disable a tile\n";
    worldwright::WorldDefinition world;

    CHECK(world.setTile(10, 20, true));
    CHECK_EQ(world.tileCount(), std::size_t{1});
    CHECK(!world.empty());
    CHECK(world.hasTile(10, 20));
    CHECK(!world.hasTile(20, 10));

    CHECK(world.setTile(10, 20, false));
    CHECK_EQ(world.tileCount(), std::size_t{0});
    CHECK(world.empty());
    CHECK(!world.hasTile(10, 20));
}

void testIdempotentOperations() {
    std::cout << "Test: repeated enable/disable does not double-count\n";
    worldwright::WorldDefinition world;

    CHECK(world.setTile(5, 5, true));
    CHECK(world.setTile(5, 5, true));
    CHECK(world.setTile(5, 5, true));
    CHECK_EQ(world.tileCount(), std::size_t{1});

    CHECK(world.setTile(5, 5, false));
    CHECK(world.setTile(5, 5, false));
    CHECK_EQ(world.tileCount(), std::size_t{0});

    CHECK(world.setTile(6, 6, false));
    CHECK_EQ(world.tileCount(), std::size_t{0});
}

void testInvalidCoordinates() {
    std::cout << "Test: invalid coordinates are rejected\n";
    worldwright::WorldDefinition world;

    const int invalid[][2] = {
        {-1, 0}, {0, -1}, {64, 0}, {0, 64}, {64, 64},
        {std::numeric_limits<int>::min(), 0},
        {0, std::numeric_limits<int>::min()},
        {std::numeric_limits<int>::max(), 0},
        {0, std::numeric_limits<int>::max()},
    };

    for (const auto& coordinate : invalid) {
        CHECK(!worldwright::WorldDefinition::isValidTile(coordinate[0],
                                                         coordinate[1]));
        CHECK(!world.setTile(coordinate[0], coordinate[1], true));
        CHECK(!world.hasTile(coordinate[0], coordinate[1]));
    }
    CHECK_EQ(world.tileCount(), std::size_t{0});
}

void testBoundaryCoordinates() {
    std::cout << "Test: boundary coordinates (0,0) and (63,63)\n";
    worldwright::WorldDefinition world;

    CHECK(worldwright::WorldDefinition::isValidTile(0, 0));
    CHECK(worldwright::WorldDefinition::isValidTile(63, 63));
    CHECK(world.setTile(0, 0, true));
    CHECK(world.setTile(63, 63, true));
    CHECK_EQ(world.tileCount(), std::size_t{2});
    CHECK(world.hasTile(0, 0));
    CHECK(world.hasTile(63, 63));
    CHECK((world.bounds() == worldwright::TileBounds{0, 0, 63, 63}));
}

void testClear() {
    std::cout << "Test: clearing a world removes all tiles\n";
    worldwright::WorldDefinition world;
    for (int y = 10; y < 20; ++y) {
        for (int x = 10; x < 20; ++x) {
            CHECK(world.setTile(x, y, true));
        }
    }
    CHECK_EQ(world.tileCount(), std::size_t{100});

    world.clear();
    CHECK_EQ(world.tileCount(), std::size_t{0});
    CHECK(world.empty());
    CHECK(world.enabledTiles().empty());
    CHECK(!world.bounds().has_value());
    CHECK(!world.hasTile(10, 10));
}

void testEnumerationAndBounds() {
    std::cout << "Test: enumeration order and bounding rectangle\n";
    worldwright::WorldDefinition world;
    world.setTile(5, 6, true);
    world.setTile(1, 2, true);
    world.setTile(3, 4, true);

    const std::vector<worldwright::TileCoord> expected{
        {1, 2}, {3, 4}, {5, 6}};
    CHECK(world.enabledTiles() == expected);
    CHECK((world.bounds() == worldwright::TileBounds{1, 2, 5, 6}));

    world.clear();
    world.setTile(30, 31, true);
    world.setTile(32, 33, true);
    world.setTile(31, 32, true);
    CHECK((world.bounds() == worldwright::TileBounds{30, 31, 32, 33}));
}

void testEmptySerializationLayout() {
    std::cout << "Test: empty world raw WDT layout\n";
    worldwright::WorldDefinition world;
    const auto data = worldwright::WdtWriter::serialize(world);

    CHECK_EQ(data.size(), worldwright::WdtWriter::kSerializedSize);
    CHECK_EQ(data.size(), std::size_t{32828});

    // MVER: on-disk bytes R E V M, payload size 4, version 18.
    CHECK_EQ(data[0], std::uint8_t{'R'});
    CHECK_EQ(data[1], std::uint8_t{'E'});
    CHECK_EQ(data[2], std::uint8_t{'V'});
    CHECK_EQ(data[3], std::uint8_t{'M'});
    CHECK_EQ(readU32(data, 4), 4u);
    CHECK_EQ(readU32(data, 8), 18u);

    // MPHD: D H P M, payload size 32, flags 0, 28 reserved zero bytes.
    CHECK_EQ(data[12], std::uint8_t{'D'});
    CHECK_EQ(data[13], std::uint8_t{'H'});
    CHECK_EQ(data[14], std::uint8_t{'P'});
    CHECK_EQ(data[15], std::uint8_t{'M'});
    CHECK_EQ(readU32(data, 16), 32u);
    for (std::size_t offset = 20; offset < 52; ++offset) {
        CHECK_EQ(data[offset], std::uint8_t{0});
    }

    // MAIN: N I A M, payload size 32768, all entries zero.
    CHECK_EQ(data[52], std::uint8_t{'N'});
    CHECK_EQ(data[53], std::uint8_t{'I'});
    CHECK_EQ(data[54], std::uint8_t{'A'});
    CHECK_EQ(data[55], std::uint8_t{'M'});
    CHECK_EQ(data[56], std::uint8_t{0x00});
    CHECK_EQ(data[57], std::uint8_t{0x80});
    CHECK_EQ(data[58], std::uint8_t{0x00});
    CHECK_EQ(data[59], std::uint8_t{0x00});
    CHECK_EQ(readU32(data, 56), 32768u);

    CHECK_EQ(countMainEntries(data, 0u),
             worldwright::WdtWriter::kMainEntryCount);
    CHECK_EQ(mainMismatchCount(data, world), std::size_t{0});
}

void testChunkLayout() {
    std::cout << "Test: independent chunk ordering and sizes\n";
    worldwright::WorldDefinition world;
    world.setTile(30, 30, true);
    const auto data = worldwright::WdtWriter::serialize(world);
    const auto chunks = parseChunks(data);

    CHECK_EQ(chunks.size(), std::size_t{3});
    if (chunks.size() == 3) {
        CHECK_EQ(chunks[0].id, std::string("MVER"));
        CHECK_EQ(chunks[1].id, std::string("MPHD"));
        CHECK_EQ(chunks[2].id, std::string("MAIN"));

        CHECK_EQ(chunks[0].size, 4u);
        CHECK_EQ(chunks[1].size, 32u);
        CHECK_EQ(chunks[2].size, 32768u);

        CHECK_EQ(chunks[0].headerOffset, std::size_t{0});
        CHECK_EQ(chunks[1].headerOffset, std::size_t{12});
        CHECK_EQ(chunks[2].headerOffset, std::size_t{52});

        CHECK_EQ(chunks[0].payloadOffset, std::size_t{8});
        CHECK_EQ(chunks[1].payloadOffset, std::size_t{20});
        CHECK_EQ(chunks[2].payloadOffset, std::size_t{60});

        CHECK_EQ(chunks[2].payloadOffset + chunks[2].size, data.size());
    }
}

void testSingleTileLayout() {
    std::cout << "Test: single-tile raw MAIN entry\n";
    worldwright::WorldDefinition world;
    world.setTile(32, 32, true);
    const auto data = worldwright::WdtWriter::serialize(world);

    CHECK_EQ(data.size(), std::size_t{32828});
    CHECK_EQ(countMainEntries(data, 1u), std::size_t{1});
    CHECK_EQ(mainMismatchCount(data, world), std::size_t{0});

    const std::size_t index = std::size_t{32} * 64 + 32;
    const std::size_t offset = 60 + index * 8;
    CHECK_EQ(data[offset], std::uint8_t{0x01});
    CHECK_EQ(data[offset + 1], std::uint8_t{0x00});
    CHECK_EQ(data[offset + 2], std::uint8_t{0x00});
    CHECK_EQ(data[offset + 3], std::uint8_t{0x00});
    CHECK_EQ(readU32(data, offset), 1u);
    CHECK_EQ(readU32(data, offset + 4), 0u);
}

void testBoundaryTileLayout() {
    std::cout << "Test: boundary tiles map to first and last MAIN entries\n";
    worldwright::WorldDefinition world;
    world.setTile(0, 0, true);
    world.setTile(63, 63, true);
    const auto data = worldwright::WdtWriter::serialize(world);

    CHECK_EQ(countMainEntries(data, 1u), std::size_t{2});
    CHECK_EQ(mainMismatchCount(data, world), std::size_t{0});
    CHECK_EQ(readU32(data, 60), 1u);
    CHECK_EQ(readU32(data, 60 + std::size_t{4095} * 8), 1u);
}

void testNineTileLayout() {
    std::cout << "Test: nine-tile rectangle raw MAIN layout\n";
    worldwright::WorldDefinition world;
    for (int y = 30; y <= 32; ++y) {
        for (int x = 30; x <= 32; ++x) {
            world.setTile(x, y, true);
        }
    }
    const auto data = worldwright::WdtWriter::serialize(world);

    CHECK_EQ(countMainEntries(data, 1u), std::size_t{9});
    CHECK_EQ(mainMismatchCount(data, world), std::size_t{0});

    const auto entryOffset = [](int x, int y) {
        return std::size_t{60} +
               (static_cast<std::size_t>(y) * 64 +
                static_cast<std::size_t>(x)) *
                   8;
    };

    CHECK_EQ(readU32(data, entryOffset(30, 30)), 1u);
    CHECK_EQ(readU32(data, entryOffset(32, 32)), 1u);
    CHECK_EQ(readU32(data, entryOffset(29, 30)), 0u);
    CHECK_EQ(readU32(data, entryOffset(33, 32)), 0u);
}

void testFullGridLayout() {
    std::cout << "Test: full 64 x 64 raw MAIN layout\n";
    worldwright::WorldDefinition world;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            world.setTile(x, y, true);
        }
    }
    const auto data = worldwright::WdtWriter::serialize(world);

    CHECK_EQ(world.tileCount(), std::size_t{4096});
    CHECK_EQ(countMainEntries(data, 1u), std::size_t{4096});
    CHECK_EQ(countMainEntries(data, 0u), std::size_t{0});
    CHECK_EQ(mainMismatchCount(data, world), std::size_t{0});
    CHECK((world.bounds() == worldwright::TileBounds{0, 0, 63, 63}));
}

void testDeterministicSerialization() {
    std::cout << "Test: serialization is deterministic\n";
    worldwright::WorldDefinition world;
    for (int y = 10; y < 15; ++y) {
        for (int x = 20; x < 27; ++x) {
            world.setTile(x, y, true);
        }
    }

    const auto first = worldwright::WdtWriter::serialize(world);
    const auto second = worldwright::WdtWriter::serialize(world);
    CHECK(first == second);
}

void checkRoundTrip(const worldwright::WorldDefinition& world) {
    const auto data = worldwright::WdtWriter::serialize(world);
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK(reader.isLoaded());
    CHECK_EQ(reader.version(), 18u);
    CHECK_EQ(reader.tileCount(), world.tileCount());
    CHECK(reader.existingTiles() == toPairs(world));

    const auto expectedBounds = world.bounds();
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    const bool hasBounds = reader.tileBounds(minX, minY, maxX, maxY);
    CHECK_EQ(hasBounds, expectedBounds.has_value());
    if (hasBounds && expectedBounds.has_value()) {
        CHECK_EQ(minX, expectedBounds->minX);
        CHECK_EQ(minY, expectedBounds->minY);
        CHECK_EQ(maxX, expectedBounds->maxX);
        CHECK_EQ(maxY, expectedBounds->maxY);
    }
}

void testRoundTripEmpty() {
    std::cout << "Test: round trip empty world\n";
    worldwright::WorldDefinition world;
    checkRoundTrip(world);
}

void testRoundTripSingle() {
    std::cout << "Test: round trip single tile\n";
    worldwright::WorldDefinition world;
    world.setTile(32, 32, true);
    checkRoundTrip(world);
}

void testRoundTripNine() {
    std::cout << "Test: round trip nine-tile rectangle\n";
    worldwright::WorldDefinition world;
    for (int y = 30; y <= 32; ++y) {
        for (int x = 30; x <= 32; ++x) {
            world.setTile(x, y, true);
        }
    }
    checkRoundTrip(world);
}

void testRoundTripFull() {
    std::cout << "Test: round trip full grid\n";
    worldwright::WorldDefinition world;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            world.setTile(x, y, true);
        }
    }
    checkRoundTrip(world);
}

void testRoundTripEveryCoordinate() {
    std::cout << "Test: round trip preserves every tile coordinate\n";
    worldwright::WorldDefinition world;
    const std::vector<std::pair<int, int>> coordinates{
        {0, 0},   {63, 63}, {1, 2},   {30, 30}, {31, 30},
        {32, 30}, {0, 63},  {63, 0},  {17, 42}, {5, 6},
        {6, 5},   {2, 1},   {40, 40},
    };
    for (const auto& coordinate : coordinates) {
        world.setTile(coordinate.first, coordinate.second, true);
    }
    checkRoundTrip(world);
}

void testSaveRepeated() {
    std::cout << "Test: repeated saves produce identical files\n";
    TempDir temp;
    worldwright::WorldDefinition world;
    for (int y = 30; y <= 32; ++y) {
        for (int x = 30; x <= 32; ++x) {
            world.setTile(x, y, true);
        }
    }

    worldwright::WdtWriter writer;
    const auto firstPath = temp.path() / "first.wdt";
    const auto secondPath = temp.path() / "second.wdt";
    const auto expected = worldwright::WdtWriter::serialize(world);

    CHECK(writer.save(world, firstPath));
    CHECK(writer.error() == worldwright::WdtWriteError::None);
    CHECK(writer.errorMessage().empty());
    CHECK(writer.save(world, secondPath));
    CHECK(readFile(firstPath) == expected);
    CHECK(readFile(secondPath) == expected);
    CHECK(readFile(firstPath) == readFile(secondPath));

    CHECK(writer.save(world, firstPath));
    CHECK(readFile(firstPath) == expected);
    CHECK_EQ(countTempFiles(temp.path()), std::size_t{0});
}

void testInvalidOutputPaths() {
    std::cout << "Test: invalid output paths are rejected\n";
    TempDir temp;
    worldwright::WorldDefinition world;
    worldwright::WdtWriter writer;

    CHECK((!writer.save(world, {})));
    CHECK(writer.error() == worldwright::WdtWriteError::InvalidPath);
    CHECK(!writer.errorMessage().empty());

    CHECK(!writer.save(world, temp.path() / "missing" / "out.wdt"));
    CHECK(writer.error() == worldwright::WdtWriteError::InvalidPath);

    const auto blocker = writeFile(temp.path() / "blocker", Bytes{0x41, 0x42});
    CHECK(!writer.save(world, blocker / "out.wdt"));
    CHECK(writer.error() == worldwright::WdtWriteError::InvalidPath);
    CHECK((readFile(blocker) == Bytes{0x41, 0x42}));

    const auto directoryOutput = temp.path() / "directory.wdt";
    std::filesystem::create_directories(directoryOutput);
    CHECK(!writer.save(world, directoryOutput));
    CHECK(writer.error() == worldwright::WdtWriteError::InvalidPath);
    CHECK(std::filesystem::is_directory(directoryOutput));

    const auto childDirectory = temp.path() / "child";
    std::filesystem::create_directories(childDirectory);
    CHECK(!writer.save(world, childDirectory / ""));
    CHECK(writer.error() == worldwright::WdtWriteError::InvalidPath);
}

void testOverwriteProtection() {
    std::cout << "Test: existing-file overwrite protection\n";
    TempDir temp;
    const auto outputPath = temp.path() / "protected.wdt";
    const Bytes sentinel{0xDE, 0xAD, 0xBE, 0xEF};
    writeFile(outputPath, sentinel);

    worldwright::WorldDefinition world;
    world.setTile(1, 1, true);

    worldwright::WdtWriter writer;
    worldwright::WdtWriteOptions protect;
    protect.overwrite = false;

    CHECK(!writer.save(world, outputPath, protect));
    CHECK(writer.error() == worldwright::WdtWriteError::OutputExists);
    CHECK(readFile(outputPath) == sentinel);
    CHECK_EQ(countTempFiles(temp.path()), std::size_t{0});

    worldwright::WdtWriteOptions replace;
    replace.overwrite = true;
    CHECK(writer.save(world, outputPath, replace));
    CHECK(writer.error() == worldwright::WdtWriteError::None);
    CHECK(writer.errorMessage().empty());
    CHECK(readFile(outputPath) == worldwright::WdtWriter::serialize(world));

    worldwright::WdtReader reader;
    CHECK(reader.load(outputPath));
    CHECK_EQ(reader.tileCount(), std::size_t{1});
    CHECK(reader.hasTile(1, 1));
}

void testFailureLeavesNoPartialOutput() {
    std::cout << "Test: failed saves leave no partial output\n";
    TempDir temp;
    worldwright::WorldDefinition world;
    worldwright::WdtWriter writer;

    const auto missingOutput = temp.path() / "missing" / "out.wdt";
    CHECK(!writer.save(world, missingOutput));
    CHECK(!std::filesystem::exists(missingOutput));
    CHECK_EQ(countTempFiles(temp.path()), std::size_t{0});

    const auto blocker = writeFile(temp.path() / "blocker", Bytes{1, 2, 3, 4});
    const auto blockedOutput = blocker / "out.wdt";
    CHECK(!writer.save(world, blockedOutput));
    CHECK(!std::filesystem::exists(blockedOutput));
    CHECK((readFile(blocker) == Bytes{1, 2, 3, 4}));
    CHECK_EQ(countTempFiles(temp.path()), std::size_t{0});

    const auto directoryOutput = temp.path() / "output-directory";
    std::filesystem::create_directories(directoryOutput);
    CHECK(!writer.save(world, directoryOutput));
    CHECK(std::filesystem::is_directory(directoryOutput));
    CHECK_EQ(countTempFiles(temp.path()), std::size_t{0});

    // A failed save must not poison a later successful save.
    const auto goodOutput = temp.path() / "good.wdt";
    CHECK(writer.save(world, goodOutput));
    CHECK(writer.error() == worldwright::WdtWriteError::None);
    CHECK(writer.errorMessage().empty());
    CHECK(std::filesystem::exists(goodOutput));
}

struct TestCase {
    const char* name;
    void (*function)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"empty world", testEmptyWorld},
        {"enable/disable", testEnableDisable},
        {"idempotent operations", testIdempotentOperations},
        {"invalid coordinates", testInvalidCoordinates},
        {"boundary coordinates", testBoundaryCoordinates},
        {"clear", testClear},
        {"enumeration and bounds", testEnumerationAndBounds},
        {"empty serialization layout", testEmptySerializationLayout},
        {"chunk layout", testChunkLayout},
        {"single tile layout", testSingleTileLayout},
        {"boundary tile layout", testBoundaryTileLayout},
        {"nine tile layout", testNineTileLayout},
        {"full grid layout", testFullGridLayout},
        {"deterministic serialization", testDeterministicSerialization},
        {"round trip empty", testRoundTripEmpty},
        {"round trip single", testRoundTripSingle},
        {"round trip nine", testRoundTripNine},
        {"round trip full", testRoundTripFull},
        {"round trip every coordinate", testRoundTripEveryCoordinate},
        {"repeated saves", testSaveRepeated},
        {"invalid output paths", testInvalidOutputPaths},
        {"overwrite protection", testOverwriteProtection},
        {"failure leaves no partial output", testFailureLeavesNoPartialOutput},
    };

    std::cout << "Worldwright WDT Writer Test Suite\n";
    std::cout << "=================================\n";

    for (const auto& test : tests) {
        const int failuresBefore = g_failures;
        std::cout << "[ RUN      ] " << test.name << '\n';
        try {
            test.function();
        } catch (const std::exception& error) {
            recordFailure(std::string("unexpected exception: ") + error.what(),
                          __FILE__, __LINE__);
        } catch (...) {
            recordFailure("unexpected non-standard exception", __FILE__, __LINE__);
        }
        if (g_failures == failuresBefore) {
            std::cout << "[       OK ] " << test.name << '\n';
        } else {
            std::cout << "[  FAILED  ] " << test.name << '\n';
        }
    }

    std::cout << "=================================\n";
    std::cout << "Checks: " << g_checks << ", failures: " << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
