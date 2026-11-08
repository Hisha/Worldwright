#include "WdtFixture.h"
#include "worldwright/WdtReader.h"

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
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("worldwright-wdt-tests-" + std::to_string(stamp));
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

std::filesystem::path writeFile(const std::filesystem::path& directory,
                                const std::string& name,
                                const wdt_fixtures::Bytes& data) {
    const std::filesystem::path path = directory / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(data.data()),
                             static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("failed to write " + path.string());
    }
    return path;
}

wdt_fixtures::Bytes makeWdtWithMain(const wdt_fixtures::Bytes& main,
                                    std::uint32_t flags = 0,
                                    std::uint32_t version = wdt_fixtures::kMverVersion) {
    wdt_fixtures::Bytes out;
    wdt_fixtures::appendChunk(out, "MVER", wdt_fixtures::makeMver(version));
    wdt_fixtures::appendChunk(out, "MPHD", wdt_fixtures::makeMphd(flags));
    wdt_fixtures::appendChunk(out, "MAIN", main);
    return out;
}

void testEmptyTerrainGrid() {
    std::cout << "Test: valid empty terrain grid\n";
    const auto data = wdt_fixtures::makeValidWdt({});
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK(reader.isLoaded());
    CHECK_EQ(reader.version(), 18u);
    CHECK_EQ(reader.headerFlags(), 0u);
    CHECK_EQ(reader.tileCount(), std::size_t{0});
    CHECK(reader.existingTiles().empty());
    CHECK(!reader.hasTile(0, 0));
    CHECK(!reader.hasTile(63, 63));

    int minX = -7;
    int minY = -7;
    int maxX = -7;
    int maxY = -7;
    CHECK(!reader.tileBounds(minX, minY, maxX, maxY));
    CHECK_EQ(minX, -7);
    CHECK_EQ(minY, -7);
    CHECK_EQ(maxX, -7);
    CHECK_EQ(maxY, -7);
    CHECK(reader.error() == worldwright::WdtError::None);
}

void testOneTerrainTile() {
    std::cout << "Test: one existing terrain tile\n";
    const auto data = wdt_fixtures::makeValidWdt({{30, 30}});
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK_EQ(reader.tileCount(), std::size_t{1});
    CHECK(reader.hasTile(30, 30));
    CHECK(!reader.hasTile(30, 31));
    CHECK(!reader.hasTile(31, 30));
    const std::vector<std::pair<int, int>> expected{{30, 30}};
    CHECK(reader.existingTiles() == expected);

    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    CHECK(reader.tileBounds(minX, minY, maxX, maxY));
    CHECK_EQ(minX, 30);
    CHECK_EQ(minY, 30);
    CHECK_EQ(maxX, 30);
    CHECK_EQ(maxY, 30);
}

void testNineTerrainTiles() {
    std::cout << "Test: nine existing terrain tiles\n";
    std::vector<std::pair<int, int>> tiles;
    for (int y = 30; y <= 32; ++y) {
        for (int x = 30; x <= 32; ++x) {
            tiles.emplace_back(x, y);
        }
    }
    const auto data = wdt_fixtures::makeValidWdt(tiles);
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK_EQ(reader.tileCount(), std::size_t{9});
    for (int y = 30; y <= 32; ++y) {
        for (int x = 30; x <= 32; ++x) {
            CHECK(reader.hasTile(x, y));
        }
    }
    CHECK(!reader.hasTile(29, 30));
    CHECK(!reader.hasTile(33, 32));
    CHECK(reader.existingTiles() == tiles);

    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    CHECK(reader.tileBounds(minX, minY, maxX, maxY));
    CHECK_EQ(minX, 30);
    CHECK_EQ(minY, 30);
    CHECK_EQ(maxX, 32);
    CHECK_EQ(maxY, 32);
}

void testTileCoordinateLookup() {
    std::cout << "Test: tile coordinate lookup uses row-major MAIN order\n";
    const auto data = wdt_fixtures::makeValidWdt({{1, 2}});
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK(reader.hasTile(1, 2));
    CHECK(!reader.hasTile(2, 1));
    CHECK(!reader.hasTile(1, 1));
    CHECK(!reader.hasTile(2, 2));

    const auto rowMajor = wdt_fixtures::makeValidWdt({{1, 0}, {0, 1}});
    CHECK(reader.loadFromMemory(rowMajor.data(), rowMajor.size()));
    const std::vector<std::pair<int, int>> expected{{1, 0}, {0, 1}};
    CHECK(reader.existingTiles() == expected);
}

void testOutOfRangeCoordinates() {
    std::cout << "Test: out-of-range tile coordinates\n";
    std::vector<std::pair<int, int>> all;
    for (int y = 0; y < wdt_fixtures::kGridHeight; ++y) {
        for (int x = 0; x < wdt_fixtures::kGridWidth; ++x) {
            all.emplace_back(x, y);
        }
    }
    const auto data = wdt_fixtures::makeValidWdt(all);
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK(reader.hasTile(0, 0));
    CHECK(reader.hasTile(63, 63));
    CHECK(!reader.hasTile(-1, 0));
    CHECK(!reader.hasTile(0, -1));
    CHECK(!reader.hasTile(64, 0));
    CHECK(!reader.hasTile(0, 64));
    CHECK(!reader.hasTile(64, 64));
    CHECK(!reader.hasTile(std::numeric_limits<int>::min(), 0));
    CHECK(!reader.hasTile(0, std::numeric_limits<int>::min()));
    CHECK(!reader.hasTile(std::numeric_limits<int>::max(), 0));
    CHECK(!reader.hasTile(0, std::numeric_limits<int>::max()));
}

void testMissingRequiredChunks() {
    std::cout << "Test: missing required chunks\n";
    worldwright::WdtReader reader;

    wdt_fixtures::Bytes noMver;
    wdt_fixtures::appendChunk(noMver, "MPHD", wdt_fixtures::makeMphd());
    wdt_fixtures::appendChunk(noMver, "MAIN", wdt_fixtures::makeEmptyMain());
    CHECK(!reader.loadFromMemory(noMver.data(), noMver.size()));
    CHECK(reader.error() == worldwright::WdtError::MissingRequiredChunk);
    CHECK(reader.errorMessage().find("MVER") != std::string::npos);
    CHECK(!reader.isLoaded());

    wdt_fixtures::Bytes noMphd;
    wdt_fixtures::appendChunk(noMphd, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(noMphd, "MAIN", wdt_fixtures::makeEmptyMain());
    CHECK(!reader.loadFromMemory(noMphd.data(), noMphd.size()));
    CHECK(reader.error() == worldwright::WdtError::MissingRequiredChunk);
    CHECK(reader.errorMessage().find("MPHD") != std::string::npos);

    wdt_fixtures::Bytes noMain;
    wdt_fixtures::appendChunk(noMain, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(noMain, "MPHD", wdt_fixtures::makeMphd());
    CHECK(!reader.loadFromMemory(noMain.data(), noMain.size()));
    CHECK(reader.error() == worldwright::WdtError::MissingRequiredChunk);
    CHECK(reader.errorMessage().find("MAIN") != std::string::npos);

    wdt_fixtures::Bytes wmoOnly;
    wdt_fixtures::appendChunk(wmoOnly, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(wmoOnly, "MPHD", wdt_fixtures::makeMphd(0x0001u));
    wdt_fixtures::appendChunk(wmoOnly, "MWMO", wdt_fixtures::Bytes{'t', 'e', 's', 't', 0});
    wdt_fixtures::appendChunk(wmoOnly, "MODF", wdt_fixtures::Bytes(64, 0));
    CHECK(!reader.loadFromMemory(wmoOnly.data(), wmoOnly.size()));
    CHECK(reader.error() == worldwright::WdtError::UnsupportedWmoOnly);

    CHECK(!reader.loadFromMemory(nullptr, 0));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);
}

void testTruncatedData() {
    std::cout << "Test: truncated WDT data\n";
    worldwright::WdtReader reader;

    auto data = wdt_fixtures::makeValidWdt({{30, 30}});
    data.pop_back();
    CHECK(!reader.loadFromMemory(data.data(), data.size()));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);

    wdt_fixtures::Bytes incompleteHeader;
    wdt_fixtures::appendChunk(incompleteHeader, "MVER", wdt_fixtures::makeMver());
    incompleteHeader.insert(incompleteHeader.end(), {'M', 'A', 'I'});
    CHECK(!reader.loadFromMemory(incompleteHeader.data(), incompleteHeader.size()));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);

    wdt_fixtures::Bytes oversized;
    wdt_fixtures::appendChunk(oversized, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(oversized, "MPHD", wdt_fixtures::makeMphd());
    wdt_fixtures::appendChunkHeader(oversized, "MAIN", 0xFFFFFFFFu);
    CHECK(!reader.loadFromMemory(oversized.data(), oversized.size()));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);

    wdt_fixtures::Bytes truncatedMain;
    wdt_fixtures::appendChunk(truncatedMain, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(truncatedMain, "MPHD", wdt_fixtures::makeMphd());
    wdt_fixtures::appendChunkHeader(truncatedMain, "MAIN", 32768);
    truncatedMain.insert(truncatedMain.end(), 64, 0);
    CHECK(!reader.loadFromMemory(truncatedMain.data(), truncatedMain.size()));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);
}

void testInvalidChunkSizes() {
    std::cout << "Test: invalid chunk sizes\n";
    worldwright::WdtReader reader;

    const std::vector<wdt_fixtures::Bytes> badMver{
        {18, 0, 0}, {18, 0, 0, 0, 0}, {}};
    for (const auto& payload : badMver) {
        wdt_fixtures::Bytes data;
        wdt_fixtures::appendChunk(data, "MVER", payload);
        wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
        wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
        CHECK(!reader.loadFromMemory(data.data(), data.size()));
        CHECK(reader.error() == worldwright::WdtError::InvalidChunkSize);
    }

    const std::vector<wdt_fixtures::Bytes> badMphd{
        wdt_fixtures::Bytes(16, 0), wdt_fixtures::Bytes(31, 0),
        wdt_fixtures::Bytes(33, 0), wdt_fixtures::Bytes(0, 0)};
    wdt_fixtures::Bytes validHeader;
    wdt_fixtures::appendChunk(validHeader, "MVER", wdt_fixtures::makeMver());
    for (const auto& payload : badMphd) {
        wdt_fixtures::Bytes data = validHeader;
        wdt_fixtures::appendChunk(data, "MPHD", payload);
        wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
        CHECK(!reader.loadFromMemory(data.data(), data.size()));
        CHECK(reader.error() == worldwright::WdtError::InvalidChunkSize);
    }

    const std::vector<wdt_fixtures::Bytes> badMain{
        wdt_fixtures::Bytes(32760, 0), wdt_fixtures::Bytes(32776, 0),
        wdt_fixtures::Bytes(16384, 0), wdt_fixtures::Bytes(0, 0)};
    wdt_fixtures::Bytes validPrefix;
    wdt_fixtures::appendChunk(validPrefix, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(validPrefix, "MPHD", wdt_fixtures::makeMphd());
    for (const auto& payload : badMain) {
        wdt_fixtures::Bytes data = validPrefix;
        wdt_fixtures::appendChunk(data, "MAIN", payload);
        CHECK(!reader.loadFromMemory(data.data(), data.size()));
        CHECK(reader.error() == worldwright::WdtError::InvalidChunkSize);
    }
}

void testUnsupportedVersion() {
    std::cout << "Test: unsupported MVER versions\n";
    worldwright::WdtReader reader;
    for (std::uint32_t version : {0u, 17u, 19u, 0xFFFFFFFFu}) {
        const auto data = wdt_fixtures::makeValidWdt({}, 0, version);
        CHECK(!reader.loadFromMemory(data.data(), data.size()));
        CHECK(reader.error() == worldwright::WdtError::UnsupportedVersion);
        CHECK(!reader.isLoaded());
        CHECK_EQ(reader.version(), 0u);
    }
}

void testMalformedMainChunk() {
    std::cout << "Test: malformed MAIN chunk\n";
    worldwright::WdtReader reader;

    const auto legacyFourByteMain = makeWdtWithMain(wdt_fixtures::Bytes(16384, 0));
    CHECK(!reader.loadFromMemory(legacyFourByteMain.data(),
                                 legacyFourByteMain.size()));
    CHECK(reader.error() == worldwright::WdtError::InvalidChunkSize);

    auto duplicate = wdt_fixtures::makeValidWdt({{30, 30}});
    wdt_fixtures::appendChunk(duplicate, "MAIN", wdt_fixtures::makeEmptyMain());
    CHECK(!reader.loadFromMemory(duplicate.data(), duplicate.size()));
    CHECK(reader.error() == worldwright::WdtError::DuplicateChunk);
}

void testDuplicateRequiredChunks() {
    std::cout << "Test: duplicate required chunks\n";
    worldwright::WdtReader reader;

    auto duplicateMver = wdt_fixtures::makeValidWdt({});
    wdt_fixtures::appendChunk(duplicateMver, "MVER", wdt_fixtures::makeMver());
    CHECK(!reader.loadFromMemory(duplicateMver.data(), duplicateMver.size()));
    CHECK(reader.error() == worldwright::WdtError::DuplicateChunk);

    auto duplicateMphd = wdt_fixtures::makeValidWdt({});
    wdt_fixtures::appendChunk(duplicateMphd, "MPHD", wdt_fixtures::makeMphd());
    CHECK(!reader.loadFromMemory(duplicateMphd.data(), duplicateMphd.size()));
    CHECK(reader.error() == worldwright::WdtError::DuplicateChunk);
}

void testUnknownChunks() {
    std::cout << "Test: optional and unknown chunks are skipped\n";
    wdt_fixtures::Bytes data;
    wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
    wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
    wdt_fixtures::appendChunk(data, "JUNK", wdt_fixtures::Bytes{1, 2, 3});
    wdt_fixtures::appendChunk(data, "EMPT", wdt_fixtures::Bytes{});
    wdt_fixtures::appendChunk(data, "MAIN",
                              wdt_fixtures::makeMain({{30, 30}, {31, 31}}));
    wdt_fixtures::appendChunk(data, "ZZZZ", wdt_fixtures::Bytes{9, 8, 7, 6, 5});

    worldwright::WdtReader reader;
    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK_EQ(reader.tileCount(), std::size_t{2});
    CHECK(reader.hasTile(30, 30));
    CHECK(reader.hasTile(31, 31));
}

void testHeaderFlags() {
    std::cout << "Test: MPHD flag recognition\n";
    worldwright::WdtReader reader;

    const std::uint32_t flags = worldwright::WdtReader::kFlagHasMccv |
                                worldwright::WdtReader::kFlagHasBigAlpha |
                                worldwright::WdtReader::kFlagDoodadsSortedBySize;
    const auto data = wdt_fixtures::makeValidWdt({}, flags);
    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK_EQ(reader.headerFlags(), flags);
    CHECK(reader.hasMccv());
    CHECK(reader.hasBigAlpha());
    CHECK(!reader.hasGlobalWmo());

    const auto globalWmoWithMain =
        wdt_fixtures::makeValidWdt({}, worldwright::WdtReader::kFlagGlobalWmo);
    CHECK(reader.loadFromMemory(globalWmoWithMain.data(),
                                globalWmoWithMain.size()));
    CHECK(reader.hasGlobalWmo());
    CHECK_EQ(reader.tileCount(), std::size_t{0});
}

void testMainFlagSemantics() {
    std::cout << "Test: MAIN existence flag semantics\n";
    auto main = wdt_fixtures::makeEmptyMain();
    wdt_fixtures::setMainEntry(main, 0, 0, 0x1u);
    wdt_fixtures::setMainEntry(main, 1, 0, 0x0u);
    wdt_fixtures::setMainEntry(main, 2, 0, 0x2u);
    wdt_fixtures::setMainEntry(main, 3, 0, 0x3u);
    wdt_fixtures::setMainEntry(main, 4, 0, 0x80000000u);
    wdt_fixtures::setMainEntry(main, 5, 0, 0xFFFFFFFFu, 0xDEADBEEFu);
    wdt_fixtures::setMainEntry(main, 6, 0, 0x1u, 0x12345678u);

    const auto data = makeWdtWithMain(main);
    worldwright::WdtReader reader;
    CHECK(reader.loadFromMemory(data.data(), data.size()));

    CHECK_EQ(reader.tileCount(), std::size_t{4});
    CHECK(reader.hasTile(0, 0));
    CHECK(!reader.hasTile(1, 0));
    CHECK(!reader.hasTile(2, 0));
    CHECK(reader.hasTile(3, 0));
    CHECK(!reader.hasTile(4, 0));
    CHECK(reader.hasTile(5, 0));
    CHECK(reader.hasTile(6, 0));
}

void testAllTilesAndBounds() {
    std::cout << "Test: full 64 x 64 grid and bounds\n";
    std::vector<std::pair<int, int>> all;
    for (int y = 0; y < wdt_fixtures::kGridHeight; ++y) {
        for (int x = 0; x < wdt_fixtures::kGridWidth; ++x) {
            all.emplace_back(x, y);
        }
    }
    const auto data = wdt_fixtures::makeValidWdt(all);
    worldwright::WdtReader reader;

    CHECK(reader.loadFromMemory(data.data(), data.size()));
    CHECK_EQ(reader.tileCount(), worldwright::WdtReader::kGridCellCount);
    CHECK_EQ(reader.existingTiles().size(), worldwright::WdtReader::kGridCellCount);

    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    CHECK(reader.tileBounds(minX, minY, maxX, maxY));
    CHECK_EQ(minX, 0);
    CHECK_EQ(minY, 0);
    CHECK_EQ(maxX, 63);
    CHECK_EQ(maxY, 63);
}

void testRepeatedLoadingClearsState() {
    std::cout << "Test: repeated loading and stale-state clearing\n";
    TempDir temp;
    const auto nine = wdt_fixtures::makeValidWdt(
        {{30, 30}, {31, 30}, {32, 30}, {30, 31}, {31, 31},
         {32, 31}, {30, 32}, {31, 32}, {32, 32}});
    const auto empty = wdt_fixtures::makeValidWdt({});
    auto truncated = wdt_fixtures::makeValidWdt({{1, 1}});
    truncated.pop_back();

    const auto ninePath = writeFile(temp.path(), "nine.wdt", nine);
    const auto emptyPath = writeFile(temp.path(), "empty.wdt", empty);
    const auto truncatedPath = writeFile(temp.path(), "truncated.wdt", truncated);

    worldwright::WdtReader reader;
    CHECK(reader.load(ninePath));
    CHECK_EQ(reader.tileCount(), std::size_t{9});
    CHECK(reader.hasTile(31, 31));

    CHECK(reader.load(emptyPath));
    CHECK_EQ(reader.tileCount(), std::size_t{0});
    CHECK(!reader.hasTile(31, 31));
    CHECK(reader.existingTiles().empty());

    CHECK(!reader.load(truncatedPath));
    CHECK(!reader.isLoaded());
    CHECK_EQ(reader.tileCount(), std::size_t{0});
    CHECK(reader.existingTiles().empty());
    CHECK_EQ(reader.version(), 0u);
    CHECK_EQ(reader.headerFlags(), 0u);
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);

    CHECK(reader.load(ninePath));
    CHECK_EQ(reader.tileCount(), std::size_t{9});

    const auto missingPath = temp.path() / "does-not-exist.wdt";
    CHECK(!reader.load(missingPath));
    CHECK(reader.error() == worldwright::WdtError::FileOpenFailed);
    CHECK(!reader.isLoaded());
    CHECK_EQ(reader.tileCount(), std::size_t{0});
    CHECK(reader.existingTiles().empty());

    CHECK(reader.load(ninePath));
    CHECK_EQ(reader.tileCount(), std::size_t{9});
}

void testPathLoading() {
    std::cout << "Test: path loading, file names with spaces, empty file\n";
    TempDir temp;
    const auto data = wdt_fixtures::makeValidWdt({{5, 6}});
    const auto path = writeFile(temp.path(), "WDT with spaces.wdt", data);

    worldwright::WdtReader reader;
    CHECK(reader.load(path));
    CHECK_EQ(reader.tileCount(), std::size_t{1});
    CHECK(reader.hasTile(5, 6));
    CHECK(reader.errorMessage().empty());

    const auto emptyPath = writeFile(temp.path(), "empty.wdt", {});
    CHECK(!reader.load(emptyPath));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);
    CHECK(!reader.isLoaded());

    const auto shortPath = writeFile(temp.path(), "short.wdt", {1, 2, 3, 4, 5});
    CHECK(!reader.load(shortPath));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);
}

void testTrailingGarbage() {
    std::cout << "Test: trailing bytes shorter than a chunk header\n";
    auto data = wdt_fixtures::makeValidWdt({{30, 30}});
    data.insert(data.end(), {0xDE, 0xAD, 0xBE});

    worldwright::WdtReader reader;
    CHECK(!reader.loadFromMemory(data.data(), data.size()));
    CHECK(reader.error() == worldwright::WdtError::TruncatedData);
}

void testFixtureEncodingMatchesDocumentedLayout() {
    std::cout << "Test: generated fixture uses the documented on-disk layout\n";
    const auto data = wdt_fixtures::makeValidWdt({{30, 30}}, 0x0Eu);

    // MVER chunk: file bytes are R E V M, then size 4 and version 18.
    CHECK_EQ(data.size(), std::size_t{32828});
    CHECK_EQ(data[0], std::uint8_t{'R'});
    CHECK_EQ(data[1], std::uint8_t{'E'});
    CHECK_EQ(data[2], std::uint8_t{'V'});
    CHECK_EQ(data[3], std::uint8_t{'M'});
    CHECK_EQ(data[4], std::uint8_t{4});
    CHECK_EQ(data[5], std::uint8_t{0});
    CHECK_EQ(data[6], std::uint8_t{0});
    CHECK_EQ(data[7], std::uint8_t{0});
    CHECK_EQ(data[8], std::uint8_t{18});
    CHECK_EQ(data[9], std::uint8_t{0});

    // MPHD chunk: D H P M, size 32, flag word 0x0E.
    CHECK_EQ(data[12], std::uint8_t{'D'});
    CHECK_EQ(data[13], std::uint8_t{'H'});
    CHECK_EQ(data[14], std::uint8_t{'P'});
    CHECK_EQ(data[15], std::uint8_t{'M'});
    CHECK_EQ(data[16], std::uint8_t{32});
    CHECK_EQ(data[20], std::uint8_t{0x0E});

    // MAIN chunk: N I A M, size 32768, then 4096 eight-byte entries.
    CHECK_EQ(data[52], std::uint8_t{'N'});
    CHECK_EQ(data[53], std::uint8_t{'I'});
    CHECK_EQ(data[54], std::uint8_t{'A'});
    CHECK_EQ(data[55], std::uint8_t{'M'});
    CHECK_EQ(data[56], std::uint8_t{0x00});
    CHECK_EQ(data[57], std::uint8_t{0x80});
    const std::size_t mainOffset = 60;
    const std::size_t entry = (std::size_t{30} * 64) + 30;
    CHECK_EQ(data[mainOffset + entry * 8], std::uint8_t{1});
    CHECK_EQ(data[mainOffset + entry * 8 + 1], std::uint8_t{0});
}

struct TestCase {
    const char* name;
    void (*function)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"empty terrain grid", testEmptyTerrainGrid},
        {"one terrain tile", testOneTerrainTile},
        {"nine terrain tiles", testNineTerrainTiles},
        {"coordinate lookup", testTileCoordinateLookup},
        {"out-of-range coordinates", testOutOfRangeCoordinates},
        {"missing required chunks", testMissingRequiredChunks},
        {"truncated data", testTruncatedData},
        {"invalid chunk sizes", testInvalidChunkSizes},
        {"unsupported version", testUnsupportedVersion},
        {"malformed MAIN chunk", testMalformedMainChunk},
        {"duplicate required chunks", testDuplicateRequiredChunks},
        {"unknown chunks", testUnknownChunks},
        {"MPHD flags", testHeaderFlags},
        {"MAIN flag semantics", testMainFlagSemantics},
        {"all tiles and bounds", testAllTilesAndBounds},
        {"repeated loading", testRepeatedLoadingClearsState},
        {"path loading", testPathLoading},
        {"trailing garbage", testTrailingGarbage},
        {"fixture byte layout", testFixtureEncodingMatchesDocumentedLayout},
    };

    std::cout << "Worldwright WDT Reader Test Suite\n";
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
