#include "worldwright/WdtWriter.h"
#include "worldwright/WorldDefinition.h"

#include <charconv>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

struct Rectangle {
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
};

struct Options {
    std::filesystem::path output;
    std::vector<worldwright::TileCoord> tiles;
    std::vector<Rectangle> rectangles;
    bool overwrite = false;
};

void printUsage(std::ostream& out, const char* program) {
    out << "Worldwright WDT Creator\n\n";
    out << "Usage: " << program
        << " --output <file.wdt> [options]\n\n";
    out << "Options:\n";
    out << "  --output <file.wdt>        Output WDT path (required)\n";
    out << "  --tile <x> <y>             Enable one terrain tile; repeatable\n";
    out << "  --rectangle <minX> <minY> <maxX> <maxY>\n";
    out << "                             Enable an inclusive rectangular region;\n";
    out << "                             repeatable\n";
    out << "  --overwrite                Replace an existing output file\n";
    out << "  --help, -h                 Show this help text\n\n";
    out << "Coordinates are WDT grid coordinates: x is the column and y is the\n";
    out << "row, each in the range 0-63. They match the <x>_<y> suffix of the\n";
    out << "corresponding ADT file name. With no --tile or --rectangle options,\n";
    out << "an empty world definition is written.\n";
}

bool parseCoordinate(const char* text, const char* option, int& value,
                     std::string& errorMessage) {
    if (text == nullptr || *text == '\0') {
        errorMessage = std::string(option) + " requires a coordinate";
        return false;
    }

    const char* end = text + std::strlen(text);
    int parsed = 0;
    const auto result = std::from_chars(text, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        errorMessage = std::string(option) +
                       ": not a valid integer: '" + text + "'";
        return false;
    }
    if (parsed < 0 || parsed >= worldwright::WorldDefinition::GridSize) {
        errorMessage = std::string(option) + ": coordinate " +
                       std::to_string(parsed) + " is out of range (expected 0-" +
                       std::to_string(worldwright::WorldDefinition::GridSize - 1) +
                       ")";
        return false;
    }

    value = parsed;
    return true;
}

bool requireArguments(int argc, int first, int count, const char* option,
                      std::string& errorMessage) {
    if (first + count > argc) {
        errorMessage = std::string(option) + " requires " +
                       std::to_string(count) + " argument" +
                       (count == 1 ? "" : "s");
        return false;
    }
    return true;
}

bool parseArguments(int argc, char* argv[], Options& options,
                    std::string& errorMessage, bool& showHelp) {
    showHelp = false;

    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];

        if (std::strcmp(argument, "--help") == 0 ||
            std::strcmp(argument, "-h") == 0) {
            showHelp = true;
            return true;
        }

        if (std::strcmp(argument, "--overwrite") == 0) {
            options.overwrite = true;
            continue;
        }

        if (std::strcmp(argument, "--output") == 0) {
            if (!requireArguments(argc, i + 1, 1, "--output",
                                  errorMessage)) {
                return false;
            }
            if (!options.output.empty()) {
                errorMessage = "--output may only be specified once";
                return false;
            }
            options.output = std::filesystem::path(argv[++i]);
            continue;
        }

        if (std::strcmp(argument, "--tile") == 0) {
            if (!requireArguments(argc, i + 1, 2, "--tile",
                                  errorMessage)) {
                return false;
            }
            worldwright::TileCoord tile;
            if (!parseCoordinate(argv[i + 1], "--tile", tile.x,
                                 errorMessage) ||
                !parseCoordinate(argv[i + 2], "--tile", tile.y,
                                 errorMessage)) {
                return false;
            }
            i += 2;
            options.tiles.push_back(tile);
            continue;
        }

        if (std::strcmp(argument, "--rectangle") == 0) {
            if (!requireArguments(argc, i + 1, 4, "--rectangle",
                                  errorMessage)) {
                return false;
            }
            Rectangle rectangle;
            if (!parseCoordinate(argv[i + 1], "--rectangle", rectangle.minX,
                                 errorMessage) ||
                !parseCoordinate(argv[i + 2], "--rectangle", rectangle.minY,
                                 errorMessage) ||
                !parseCoordinate(argv[i + 3], "--rectangle", rectangle.maxX,
                                 errorMessage) ||
                !parseCoordinate(argv[i + 4], "--rectangle", rectangle.maxY,
                                 errorMessage)) {
                return false;
            }
            if (rectangle.minX > rectangle.maxX ||
                rectangle.minY > rectangle.maxY) {
                errorMessage =
                    "--rectangle: minimum coordinates must not exceed maximum "
                    "coordinates";
                return false;
            }
            i += 4;
            options.rectangles.push_back(rectangle);
            continue;
        }

        errorMessage = std::string("unknown option: '") + argument + "'";
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    Options options;
    std::string errorMessage;
    bool showHelp = false;

    if (!parseArguments(argc, argv, options, errorMessage, showHelp)) {
        std::cerr << "Error: " << errorMessage << "\n\n";
        printUsage(std::cerr, argv[0]);
        return 1;
    }

    if (showHelp) {
        printUsage(std::cout, argv[0]);
        return 0;
    }

    if (options.output.empty()) {
        std::cerr << "Error: --output is required\n\n";
        printUsage(std::cerr, argv[0]);
        return 1;
    }

    worldwright::WorldDefinition world;

    for (const auto& tile : options.tiles) {
        // Coordinates were validated while parsing.
        world.setTile(tile.x, tile.y, true);
    }

    for (const auto& rectangle : options.rectangles) {
        for (int y = rectangle.minY; y <= rectangle.maxY; ++y) {
            for (int x = rectangle.minX; x <= rectangle.maxX; ++x) {
                world.setTile(x, y, true);
            }
        }
    }

    worldwright::WdtWriteOptions writeOptions;
    writeOptions.overwrite = options.overwrite;

    worldwright::WdtWriter writer;
    if (!writer.save(world, options.output, writeOptions)) {
        std::cerr << "Error: " << writer.errorMessage() << '\n';
        return 1;
    }

    std::cout << "Worldwright WDT Creator\n";
    std::cout << "Output: " << options.output.string() << '\n';
    std::cout << "Version: " << worldwright::WdtWriter::kVersion << '\n';
    std::cout << "Grid: " << worldwright::WorldDefinition::GridWidth << " x "
              << worldwright::WorldDefinition::GridHeight << '\n';
    std::cout << "Enabled tiles: " << world.tileCount() << '\n';

    const auto bounds = world.bounds();
    if (bounds.has_value()) {
        std::cout << "Tile bounds: (" << bounds->minX << ',' << bounds->minY
                  << ") to (" << bounds->maxX << ',' << bounds->maxY << ")\n";
    } else {
        std::cout << "Tile bounds: none\n";
    }

    std::cout << "Bytes written: " << worldwright::WdtWriter::kSerializedSize
              << '\n';
    return 0;
}
