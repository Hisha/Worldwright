#include "WdtTileMap.h"
#include <cstring>
#include <algorithm>

namespace worldwright {

TileMap::TileMap(WdtParser* parser)
    : m_parser(parser)
    , m_tile_grid()
    , m_existing_count(0)
    , m_initialized(false)
{
    if (m_parser == nullptr || !m_parser->hasValidMain()) {
        return;
    }
    
    // Initialize tile grid from MAIN chunk data
    const uint8_t* main_ptr = m_parser->getMainData();
    m_tile_grid.resize(TOTAL_ENTRIES);
    
    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        m_tile_grid[i] = main_ptr[i];
    }
    
    // Count existing (non-empty) tiles
    m_existing_count = 0;
    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        if (m_tile_grid[i] == ENTRY_EXISTS) {
            m_existing_count++;
        }
    }
    
    m_initialized = true;
}



bool TileMap::hasTile(uint32_t block_x, uint32_t block_y) const {
    // Bounds check
    if (block_x >= GRID_WIDTH || block_y >= GRID_HEIGHT) {
        return false;
    }
    
    // Convert BLOCK coordinates to grid index
    // BLOCK_X, BLOCK_Y = 64x64 grid
    size_t index = block_x + block_y * GRID_WIDTH;
    
    return m_tile_grid[index] == ENTRY_EXISTS;
}

bool TileMap::isEmptyTile(uint32_t block_x, uint32_t block_y) const {
    if (!hasTile(block_x, block_y)) {
        return true;
    }
    return false;
}

size_t TileMap::tileCount() const {
    return TOTAL_ENTRIES;  // Always 4096
}

size_t TileMap::existingTileCount() const {
    return m_existing_count;
}

std::vector<std::pair<uint32_t, uint32_t>> TileMap::existingTiles() const {
    std::vector<std::pair<uint32_t, uint32_t>> result;
    
    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        if (m_tile_grid[i] == ENTRY_EXISTS) {
            uint32_t block_x = i % GRID_WIDTH;
            uint32_t block_y = i / GRID_WIDTH;
            result.push_back({block_x, block_y});
        }
    }
    
    return result;
}

uint32_t TileMap::minX() const {
    return 0;
}

uint32_t TileMap::maxX() const {
    return GRID_WIDTH - 1;
}

uint32_t TileMap::minY() const {
    return 0;
}

uint32_t TileMap::maxY() const {
    return GRID_HEIGHT - 1;
}

size_t TileMap::width() const {
    return GRID_WIDTH;
}

size_t TileMap::height() const {
    return GRID_HEIGHT;
}

TileMap::TileMap(const void* data, size_t size)
    : m_parser(nullptr)
    , m_tile_grid()
    , m_existing_count(0)
    , m_initialized(false)
{
    if (data == nullptr || size == 0) {
        return;
    }
    
    // Check minimum size for WDT header
    if (size < sizeof(WdtHeader)) {
        return;
    }
    
    // Validate MVER
    const WdtHeader* header = static_cast<const WdtHeader*>(data);
    if (header->MVER != WDT_MAGIC) {
        return;
    }
    
    // Check if MAIN chunk is present
    if (size < sizeof(WdtHeader) + TOTAL_ENTRIES * sizeof(uint8_t)) {
        return;
    }
    
    // Initialize tile grid from MAIN chunk data
    const uint8_t* main_ptr = static_cast<const uint8_t*>(data) + sizeof(WdtHeader);
    m_tile_grid.resize(TOTAL_ENTRIES);
    
    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        m_tile_grid[i] = main_ptr[i];
    }
    
    // Count existing (non-empty) tiles
    m_existing_count = 0;
    for (size_t i = 0; i < TOTAL_ENTRIES; ++i) {
        if (m_tile_grid[i] == ENTRY_EXISTS) {
            m_existing_count++;
        }
    }
    
    m_initialized = true;
}

WdtParser* TileMap::parser() const {
    return m_parser;
}

}  // namespace worldwright
