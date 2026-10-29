#include "Worldwright.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <path-to-file.wdt> [--list]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --list  List all existing tiles with coordinates" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Coordinate system: BLOCK_X, BLOCK_Y (0-63)" << std::endl;
    std::cerr << "  - BLOCK_X: Column number (0 = leftmost)" << std::endl;
    std::cerr << "  - BLOCK_Y: Row number (0 = topmost)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    const char* path = argv[1];
    bool listTiles = false;
    
    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            listTiles = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Load WDT file
    TileMap* tile_map = worldwright::load(path);
    if (tile_map == nullptr) {
        std::cerr << "Error: Failed to load WDT file: " << path << std::endl;
        return 1;
    }
    
    // Print basic info
    std::cout << "WDT File Info" << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << "Total tiles: " << worldwright::tileCount(tile_map) << std::endl;
    std::cout << "Existing tiles: " << worldwright::existingTileCount(tile_map) << std::endl;
    std::cout << "Grid size: " << tile_map->width() << " x " 
              << tile_map->height() << std::endl;
    std::cout << std::endl;
    
    // Print tile list if requested
    if (listTiles) {
        std::cout << "Existing tiles:" << std::endl;
        std::vector<std::pair<uint32_t, uint32_t>> tiles = worldwright::existingTiles(tile_map);
        for (size_t i = 0; i < tiles.size(); ++i) {
            uint32_t block_x = tiles[i].first;
            uint32_t block_y = tiles[i].second;
            std::cout << "  " << std::setw(2) << (i + 1) << " | BLOCK_X: " << block_x 
                      << " | BLOCK_Y: " << block_y << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Total existing tiles: " << tiles.size() << std::endl;
    }
    
    // Clean up
    delete tile_map;
    
    return 0;
}
