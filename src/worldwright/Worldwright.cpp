#include "Worldwright.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace worldwright {

// Load from file
TileMap* load(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file into buffer
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        file.close();
        return nullptr;
    }
    file.close();
    
    // Load from buffer
    return load(buffer.data(), size);
}

// Load from raw buffer
TileMap* load(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return nullptr;
    }
    
    // Check minimum size for WDT header
    if (size < sizeof(WdtHeader)) {
        return nullptr;
    }
    
    // Validate MVER
    const WdtHeader* header = static_cast<const WdtHeader*>(data);
    if (header->MVER != WDT_MAGIC) {
        return nullptr;
    }
    
    // Create and return TileMap
    return new TileMap(data, size);
}

// Check if tile exists
bool hasTile(const TileMap* tile_map, uint32_t block_x, uint32_t block_y) {
    if (tile_map == nullptr) {
        return false;
    }
    return tile_map->hasTile(block_x, block_y);
}

// Check if tile is empty
bool isEmptyTile(const TileMap* tile_map, uint32_t block_x, uint32_t block_y) {
    if (tile_map == nullptr) {
        return true;
    }
    return tile_map->isEmptyTile(block_x, block_y);
}

// Get total tile count
size_t tileCount(const TileMap* tile_map) {
    if (tile_map == nullptr) {
        return 0;
    }
    return tile_map->tileCount();
}

// Get existing tile count
size_t existingTileCount(const TileMap* tile_map) {
    if (tile_map == nullptr) {
        return 0;
    }
    return tile_map->existingTileCount();
}

// Get list of existing tiles
std::vector<std::pair<uint32_t, uint32_t>> existingTiles(const TileMap* tile_map) {
    if (tile_map == nullptr) {
        return std::vector<std::pair<uint32_t, uint32_t>>();
    }
    return tile_map->existingTiles();
}

}  // namespace worldwright
