#include "worldwright/AdtWriter.h"
#include "worldwright/TerrainDefinition.h"
#include "worldwright/WdtWriter.h"
#include "worldwright/WorldDefinition.h"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace {

struct Rectangle { int minX, minY, maxX, maxY; };
struct Options {
    std::filesystem::path output;
    std::filesystem::path outputDir;
    std::string worldName;
    std::vector<worldwright::TileCoord> tiles;
    std::vector<Rectangle> rectangles;
    float height = 0.0f;
    bool hasHeight = false;
    bool overwrite = false;
};

void usage(std::ostream& out, const char* program) {
    out << "Worldwright ADT Creator\n\n"
        << "Single tile:\n  " << program
        << " --output <file.adt> --tile <x> <y> --height <elevation> [--overwrite]\n\n"
        << "World terrain set:\n  " << program
        << " --world <name> --output-dir <dir> [--tile x y ... | --rectangle minX minY maxX maxY ...]"
           " --height <elevation> [--overwrite]\n\n"
        << "World mode writes <name>.wdt plus one <name>_<x>_<y>.adt for each\n"
           "enabled WorldDefinition tile. Coordinates are in [0,63]. Height is\n"
           "an absolute WoW world elevation; NaN and infinity are rejected.\n";
}

bool coordinate(const char* text, int& value, std::string& error) {
    const char* end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    if (result.ec != std::errc{} || result.ptr != end ||
        !worldwright::WorldDefinition::isValidTile(value, 0)) {
        error = std::string("invalid tile coordinate (expected 0-63): '") +
                text + "'";
        return false;
    }
    return true;
}

bool elevation(const char* text, float& value, std::string& error) {
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text, &end);
    if (end == text || *end != '\0' || errno == ERANGE ||
        !std::isfinite(parsed) ||
        parsed > static_cast<double>(std::numeric_limits<float>::max()) ||
        parsed < -static_cast<double>(std::numeric_limits<float>::max())) {
        error = std::string("invalid finite elevation: '") + text + "'";
        return false;
    }
    value = static_cast<float>(parsed);
    return true;
}

bool parse(int argc, char** argv, Options& o, std::string& error,
           bool& help) {
    help = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") { help = true; return true; }
        if (a == "--overwrite") { o.overwrite = true; continue; }
        const auto need = [&](int count) { return i + count < argc; };
        if (a == "--output" || a == "--output-dir" || a == "--world" ||
            a == "--height") {
            if (!need(1)) { error = a + " requires one argument"; return false; }
            const char* v = argv[++i];
            if (a == "--output") o.output = v;
            else if (a == "--output-dir") o.outputDir = v;
            else if (a == "--world") o.worldName = v;
            else if (!elevation(v, o.height, error)) return false;
            else o.hasHeight = true;
            continue;
        }
        if (a == "--tile") {
            if (!need(2)) { error = "--tile requires two arguments"; return false; }
            worldwright::TileCoord t;
            if (!coordinate(argv[++i], t.x, error) ||
                !coordinate(argv[++i], t.y, error)) return false;
            o.tiles.push_back(t);
            continue;
        }
        if (a == "--rectangle") {
            if (!need(4)) { error = "--rectangle requires four arguments"; return false; }
            Rectangle r{};
            if (!coordinate(argv[++i], r.minX, error) ||
                !coordinate(argv[++i], r.minY, error) ||
                !coordinate(argv[++i], r.maxX, error) ||
                !coordinate(argv[++i], r.maxY, error)) return false;
            if (r.minX > r.maxX || r.minY > r.maxY) {
                error = "--rectangle minimum must not exceed maximum";
                return false;
            }
            o.rectangles.push_back(r);
            continue;
        }
        error = "unknown option: '" + a + "'";
        return false;
    }
    return true;
}

bool validWorldName(const std::string& name) {
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::string error;
    bool help = false;
    if (!parse(argc, argv, options, error, help)) {
        std::cerr << "Error: " << error << "\n\n";
        usage(std::cerr, argv[0]);
        return 1;
    }
    if (help) { usage(std::cout, argv[0]); return 0; }
    if (!options.hasHeight) {
        std::cerr << "Error: --height is required\n";
        return 1;
    }

    worldwright::TerrainDefinition terrain;
    if (!terrain.setFlatHeight(options.height)) {
        std::cerr << "Error: invalid terrain elevation\n";
        return 1;
    }

    const bool singleMode = !options.output.empty();
    const bool worldMode = !options.worldName.empty() || !options.outputDir.empty();
    if (singleMode == worldMode) {
        std::cerr << "Error: choose either --output single-tile mode or --world/--output-dir world mode\n";
        return 1;
    }

    worldwright::AdtWriteOptions adtOptions;
    adtOptions.overwrite = options.overwrite;
    if (singleMode) {
        if (options.tiles.size() != 1 || !options.rectangles.empty()) {
            std::cerr << "Error: single-tile mode requires exactly one --tile and no --rectangle\n";
            return 1;
        }
        worldwright::AdtWriter writer;
        const auto tile = options.tiles.front();
        if (!writer.save(terrain, tile.x, tile.y, options.output, adtOptions)) {
            std::cerr << "Error: " << writer.errorMessage() << '\n';
            return 1;
        }
        std::cout << "Generated: " << options.output.string() << '\n'
                  << "Tile: (" << tile.x << ',' << tile.y << ")\n"
                  << "Elevation: " << options.height << '\n'
                  << "MCNK chunks: " << worldwright::TerrainDefinition::ChunkCount << '\n'
                  << "Terrain vertices serialized: "
                  << worldwright::TerrainDefinition::SerializedVertexCount << '\n'
                  << "Bytes written: " << worldwright::AdtWriter::kSerializedSize << '\n';
        return 0;
    }

    if (!validWorldName(options.worldName) || options.outputDir.empty()) {
        std::cerr << "Error: world mode requires a path-free --world name and --output-dir\n";
        return 1;
    }
    std::error_code fsError;
    if (!std::filesystem::is_directory(options.outputDir, fsError) || fsError) {
        std::cerr << "Error: output directory does not exist or is not a directory: "
                  << options.outputDir.string() << '\n';
        return 1;
    }

    worldwright::WorldDefinition world;
    for (const auto& tile : options.tiles) world.setTile(tile.x, tile.y, true);
    for (const auto& r : options.rectangles)
        for (int y = r.minY; y <= r.maxY; ++y)
            for (int x = r.minX; x <= r.maxX; ++x) world.setTile(x, y, true);

    std::vector<std::filesystem::path> outputs;
    outputs.push_back(options.outputDir / (options.worldName + ".wdt"));
    for (const auto& tile : world.enabledTiles())
        outputs.push_back(options.outputDir /
                          (options.worldName + "_" + std::to_string(tile.x) +
                           "_" + std::to_string(tile.y) + ".adt"));
    if (!options.overwrite) {
        for (const auto& path : outputs) {
            if (std::filesystem::exists(path, fsError) && !fsError) {
                std::cerr << "Error: output file already exists (use --overwrite): "
                          << path.string() << '\n';
                return 1;
            }
        }
    }

    worldwright::WdtWriteOptions wdtOptions;
    wdtOptions.overwrite = options.overwrite;
    worldwright::WdtWriter wdt;
    if (!wdt.save(world, outputs.front(), wdtOptions)) {
        std::cerr << "Error: " << wdt.errorMessage() << '\n';
        return 1;
    }
    std::cout << "Generated: " << outputs.front().string() << '\n';
    worldwright::AdtWriter adt;
    const auto tiles = world.enabledTiles();
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        if (!adt.save(terrain, tiles[i].x, tiles[i].y, outputs[i + 1],
                      adtOptions)) {
            std::cerr << "Error: " << adt.errorMessage() << '\n';
            return 1;
        }
        std::cout << "Generated: " << outputs[i + 1].string() << '\n';
    }
    std::cout << "Enabled tiles: " << world.tileCount() << '\n'
              << "ADT files: " << tiles.size() << '\n'
              << "Elevation: " << options.height << '\n'
              << "MCNK chunks: " << tiles.size() *
                     worldwright::TerrainDefinition::ChunkCount << '\n'
              << "ADT bytes: " << tiles.size() *
                     worldwright::AdtWriter::kSerializedSize << '\n';
    return 0;
}
