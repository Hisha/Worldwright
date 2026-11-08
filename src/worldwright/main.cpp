#include "worldwright/WdtReader.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void printUsage(std::ostream& out, const char* program) {
    out << "Worldwright WDT Inspector\n\n";
    out << "Usage: " << program << " <file.wdt> [--list]\n\n";
    out << "Options:\n";
    out << "  --list    List every existing terrain tile coordinate\n";
    out << "  --help    Show this help text\n\n";
    out << "Coordinates are WDT grid coordinates: x is the column and y is the\n";
    out << "row, each in the range 0-63. They match the <x>_<y> suffix of the\n";
    out << "corresponding ADT file name.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc >= 2 && (std::strcmp(argv[1], "--help") == 0 ||
                      std::strcmp(argv[1], "-h") == 0)) {
        printUsage(std::cout, argv[0]);
        return 0;
    }

    if (argc < 2) {
        printUsage(std::cerr, argv[0]);
        return 1;
    }

    const std::filesystem::path path(argv[1]);
    bool listTiles = false;

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            listTiles = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n\n";
            printUsage(std::cerr, argv[0]);
            return 1;
        }
    }

    worldwright::WdtReader reader;
    if (!reader.load(path)) {
        std::cerr << "Error: " << reader.errorMessage() << '\n';
        return 1;
    }

    std::cout << "Worldwright WDT Inspector\n";
    std::cout << "File: " << path.string() << '\n';
    std::cout << "Version: " << reader.version() << '\n';
    std::cout << "Grid: " << worldwright::WdtReader::kGridWidth << " x "
              << worldwright::WdtReader::kGridHeight << '\n';
    std::cout << "Existing tiles: " << reader.tileCount() << '\n';

    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    if (reader.tileBounds(minX, minY, maxX, maxY)) {
        std::cout << "Tile bounds: (" << minX << ',' << minY << ") to (" << maxX
                  << ',' << maxY << ")\n";
    } else {
        std::cout << "Tile bounds: none\n";
    }

    if (listTiles) {
        std::cout << "\nExisting tiles:\n";
        for (const auto& tile : reader.existingTiles()) {
            std::cout << "  (" << tile.first << ',' << tile.second << ")\n";
        }
    }

    return 0;
}
