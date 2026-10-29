#ifndef WORLDWRIGHT_WORLDWRIGHT_H
#define WORLDWRIGHT_WORLDWRIGHT_H

#include "WdtParser.h"
#include "WdtTileMap.h"
#include <string>
#include <vector>
#include <cstdint>

namespace worldwright {

// Load a WDT file and return a TileMap
// Parameters:
//   path - Path to .wdt file
// Returns: TileMap* on success, nullptr on failure
TileMap* load(const std::string& path);

// Load from raw buffer
// Parameters:
//   data - Raw binary data buffer
//   size - Size of buffer in bytes
// Returns: TileMap* on success, nullptr on failure
TileMap* load(const void* data, size_t size);

// Check if a tile exists at given coordinates
// Parameters:
//   tile_map - TileMap pointer
//   block_x - X coordinate (0-63)
//   block_y - Y coordinate (0-63)
// Returns: true if tile exists, false otherwise
bool hasTile(const TileMap* tile_map, uint32_t block_x, uint32_t block_y);

// Check if a tile is empty at given coordinates
bool isEmptyTile(const TileMap* tile_map, uint32_t block_x, uint32_t block_y);

// Get total tile count in the grid
// Parameters:
//   tile_map - TileMap pointer
// Returns: Total number of tiles (always 4096)
size_t tileCount(const TileMap* tile_map);

// Get count of non-empty tiles
// Parameters:
//   tile_map - TileMap pointer
// Returns: Number of existing tiles
size_t existingTileCount(const TileMap* tile_map);

// Get list of existing tile coordinates
// Parameters:
//   tile_map - TileMap pointer
// Returns: Vector of {block_x, block_y} pairs for existing tiles
std::vector<std::pair<uint32_t, uint32_t>> existingTiles(const TileMap* tile_map);

}  // namespace worldwright

#endif  // WORLDWRIGHT_WORLDWRIGHT_H
