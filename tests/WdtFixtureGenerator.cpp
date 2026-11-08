#include "WdtFixture.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using wdt_fixtures::Bytes;

void writeFile(const std::filesystem::path& directory, const std::string& name,
               const Bytes& data) {
    const std::filesystem::path path = directory / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(data.data()),
                             static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("failed to write " + path.string());
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output-directory>\n";
        return 1;
    }

    const std::filesystem::path directory(argv[1]);
    try {
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        std::filesystem::create_directories(directory);

        writeFile(directory, "empty_grid.wdt",
                  wdt_fixtures::makeValidWdt({}));

        writeFile(directory, "one_tile_30_30.wdt",
                  wdt_fixtures::makeValidWdt({{30, 30}}));

        std::vector<std::pair<int, int>> nine;
        for (int y = 30; y <= 32; ++y) {
            for (int x = 30; x <= 32; ++x) {
                nine.emplace_back(x, y);
            }
        }
        writeFile(directory, "nine_tiles_30_30_to_32_32.wdt",
                  wdt_fixtures::makeValidWdt(nine));

        std::vector<std::pair<int, int>> all;
        all.reserve(wdt_fixtures::kGridCellCount);
        for (int y = 0; y < wdt_fixtures::kGridHeight; ++y) {
            for (int x = 0; x < wdt_fixtures::kGridWidth; ++x) {
                all.emplace_back(x, y);
            }
        }
        writeFile(directory, "all_tiles.wdt",
                  wdt_fixtures::makeValidWdt(all));

        writeFile(directory, "invalid_version_17.wdt",
                  wdt_fixtures::makeValidWdt({}, 0, 17));

        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
            writeFile(directory, "missing_mver.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
            writeFile(directory, "missing_mphd.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            writeFile(directory, "missing_main.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(
                data, "MPHD", wdt_fixtures::makeMphd(0x0001u));
            wdt_fixtures::appendChunk(data, "MWMO", Bytes{'t', 'e', 's', 't', 0});
            wdt_fixtures::appendChunk(data, "MODF", Bytes(64, 0));
            writeFile(directory, "wmo_only.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            wdt_fixtures::appendChunkHeader(data, "MAIN", 32768);
            data.insert(data.end(), 64, 0);
            writeFile(directory, "truncated_main.wdt", data);
        }
        {
            Bytes data = wdt_fixtures::makeValidWdt({{30, 30}});
            data.insert(data.end(), {0xDE, 0xAD, 0xBE});
            writeFile(directory, "trailing_garbage.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", Bytes{18, 0, 0});
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
            writeFile(directory, "invalid_mver_size.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MPHD", Bytes(16, 0));
            wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
            writeFile(directory, "invalid_mphd_size.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            wdt_fixtures::appendChunk(data, "MAIN", Bytes(32760, 0));
            writeFile(directory, "invalid_main_size.wdt", data);
        }
        {
            Bytes data = wdt_fixtures::makeValidWdt({{30, 30}});
            wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeEmptyMain());
            writeFile(directory, "duplicate_main.wdt", data);
        }
        {
            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            wdt_fixtures::appendChunk(data, "JUNK", Bytes{1, 2, 3});
            wdt_fixtures::appendChunk(data, "MAIN", wdt_fixtures::makeMain({{30, 30}}));
            wdt_fixtures::appendChunk(data, "ZZZZ", Bytes{});
            writeFile(directory, "unknown_chunks.wdt", data);
        }
        {
            Bytes main = wdt_fixtures::makeEmptyMain();
            wdt_fixtures::setMainEntry(main, 0, 0, 0x1);
            wdt_fixtures::setMainEntry(main, 1, 0, 0x0);
            wdt_fixtures::setMainEntry(main, 2, 0, 0x2);
            wdt_fixtures::setMainEntry(main, 3, 0, 0x3);
            wdt_fixtures::setMainEntry(main, 4, 0, 0x80000000u);
            wdt_fixtures::setMainEntry(main, 5, 0, 0xFFFFFFFFu, 0xDEADBEEFu);
            wdt_fixtures::setMainEntry(main, 6, 0, 0x1, 0x12345678u);

            Bytes data;
            wdt_fixtures::appendChunk(data, "MVER", wdt_fixtures::makeMver());
            wdt_fixtures::appendChunk(data, "MPHD", wdt_fixtures::makeMphd());
            wdt_fixtures::appendChunk(data, "MAIN", main);
            writeFile(directory, "flag_semantics.wdt", data);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
