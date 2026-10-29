#ifndef WORLDWRIGHT_WDT_TILE_MAP_H
#define WORLDWRIGHT_WDT_TILE_MAP_H

#include "WdtParser.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace worldwright {

// Tile map data structure representing a parsed WDT tile grid
class TileMap {
public:
    // Constructor from parser
    explicit TileMap(WdtParser* parser);
    
    // Constructor from raw data
    TileMap(const void* data, size_t size);
    
    // Check if tile at given coordinates exists
    bool hasTile(uint32_t block_x, uint32_t block_y) const;
    
    // Check if tile at given coordinates is empty
    bool isEmptyTile(uint32_t block_x, uint32_t block_y) const;
    
    // Get tile count (total entries in grid)
    size_t tileCount() const;
    
    // Get number of existing (non-empty) tiles
    size_t existingTileCount() const;
    
    // Get list of existing tile coordinates
    std::vector<std::pair<uint32_t, uint32_t>> existingTiles() const;
    
    // Get coordinate bounds
    uint32_t minX() const;
    uint32_t maxX() const;
    uint32_t minY() const;
    uint32_t maxY() const;
    
    // Get grid dimensions
    size_t width() const;
    size_t height() const;
    
    // Get WDT parser reference
    WdtParser* parser() const;

private:
    // Disable copy
    TileMap(const TileMap&) = delete;
    TileMap& operator=(const TileMap&) = delete;
    
    WdtParser* m_parser;
    std::vector<uint8_t> m_tile_grid;
    size_t m_existing_count;
    bool m_initialized;
};

}  // namespace worldwright

#endif  // WORLDWRIGHT_WDT_TILE_MAP_H
