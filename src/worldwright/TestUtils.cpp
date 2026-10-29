#include "TestUtils.h"
#include <random>
#include <algorithm>

namespace worldwright {

// Generate a valid WDT file with specified number of existing tiles
std::vector<uint8_t> generateValidWdt(int num_existing_tiles, int seed) {
    std::vector<uint8_t> data;
    
    // Add header
    WdtHeader header;
    header.MVER = WDT_MAGIC;  // 0x57445431
    header.MPHD = 0;  // No special flags needed for basic tests
    
    data.push_back(&header[0]);
    data.push_back(&header[1]);
    data.push_back(&header[2]);
    data.push_back(&header[3]);
    
    data.push_back(&header[4]);
    data.push_back(&header[5]);
    data.push_back(&header[6]);
    data.push_back(&header[7]);
    
    // Add MAIN chunk (4096 entries)
    data.insert(data.end(), TOTAL_ENTRIES, 0);  // All empty initially
    
    // Set specified number of tiles to exist
    std::random_device rd;
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint32_t> dist(0, TOTAL_ENTRIES - 1);
    
    for (int i = 0; i < num_existing_tiles; ++i) {
        uint32_t index = dist(gen);
        data[index] = ENTRY_EXISTS;  // 0x01
    }
    
    return data;
}

// Generate invalid WDT with wrong MVER
std::vector<uint8_t> generateInvalidMverWdt() {
    std::vector<uint8_t> data;
    
    // Invalid MVER (should be 0x57445431, using 0xDEADBEEF instead)
    WdtHeader header;
    header.MVER = 0xDEADBEEF;
    header.MPHD = 0;
    
    data.push_back(&header[0]);
    data.push_back(&header[1]);
    data.push_back(&header[2]);
    data.push_back(&header[3]);
    
    data.push_back(&header[4]);
    data.push_back(&header[5]);
    data.push_back(&header[6]);
    data.push_back(&header[7]);
    
    // Add MAIN chunk
    data.insert(data.end(), TOTAL_ENTRIES, 0);
    
    return data;
}

// Generate empty/invalid WDT (zero size or too small)
std::vector<uint8_t> generateEmptyWdt() {
    std::vector<uint8_t> data(100);  // Too small for valid WDT
    return data;
}

// Generate WDT with corrupted MAIN chunk
std::vector<uint8_t> generateCorruptedMainWdt() {
    std::vector<uint8_t> data;
    
    // Valid header
    WdtHeader header;
    header.MVER = WDT_MAGIC;
    header.MPHD = 0;
    
    data.push_back(&header[0]);
    data.push_back(&header[1]);
    data.push_back(&header[2]);
    data.push_back(&header[3]);
    
    data.push_back(&header[4]);
    data.push_back(&header[5]);
    data.push_back(&header[6]);
    data.push_back(&header[7]);
    
    // Corrupted MAIN chunk (first entry set to invalid value)
    data.insert(data.end(), TOTAL_ENTRIES, 0);
    data[0] = 0xFF;  // Invalid value instead of 0x00 or 0x01
    
    return data;
}

// Generate WDT for out-of-bounds tests (valid tiles exist at known positions)
std::vector<uint8_t> generateWdtForBoundsTests() {
    std::vector<uint8_t> data;
    
    // Valid header
    WdtHeader header;
    header.MVER = WDT_MAGIC;
    header.MPHD = 0;
    
    data.push_back(&header[0]);
    data.push_back(&header[1]);
    data.push_back(&header[2]);
    data.push_back(&header[3]);
    
    data.push_back(&header[4]);
    data.push_back(&header[5]);
    data.push_back(&header[6]);
    data.push_back(&header[7]);
    
    // Set tiles at valid positions (all within bounds)
    data.insert(data.end(), TOTAL_ENTRIES, 0);
    data[0] = ENTRY_EXISTS;  // BLOCK_X=0, BLOCK_Y=0
    data[1] = ENTRY_EXISTS;  // BLOCK_X=1, BLOCK_Y=0
    data[63] = ENTRY_EXISTS; // BLOCK_X=63, BLOCK_Y=0
    data[64] = ENTRY_EXISTS; // BLOCK_X=0, BLOCK_Y=1
    data[4095] = ENTRY_EXISTS; // BLOCK_X=63, BLOCK_Y=63
    
    return data;
}

// Generate WDT with specific tile positions
std::vector<uint8_t> generateWdtWithKnownTiles(const std::vector<std::pair<uint32_t, uint32_t>>& tiles) {
    std::vector<uint8_t> data;
    
    // Valid header
    WdtHeader header;
    header.MVER = WDT_MAGIC;
    header.MPHD = 0;
    
    data.push_back(&header[0]);
    data.push_back(&header[1]);
    data.push_back(&header[2]);
    data.push_back(&header[3]);
    
    data.push_back(&header[4]);
    data.push_back(&header[5]);
    data.push_back(&header[6]);
    data.push_back(&header[7]);
    
    // Set specified tiles to exist
    data.insert(data.end(), TOTAL_ENTRIES, 0);
    
    for (const auto& tile : tiles) {
        // Bounds check
        if (tile.first < GRID_WIDTH && tile.second < GRID_HEIGHT) {
            uint32_t index = tile.first + tile.second * GRID_WIDTH;
            data[index] = ENTRY_EXISTS;
        }
    }
    
    return data;
}

// Helper: set specific tile
void setTile(const std::vector<uint8_t>& data, uint32_t block_x, uint32_t block_y, bool exists) {
    // Bounds check
    if (block_x >= GRID_WIDTH || block_y >= GRID_HEIGHT) {
        return;  // Don't set out-of-bounds
    }
    
    uint32_t index = block_x + block_y * GRID_WIDTH;
    if (index >= TOTAL_ENTRIES) {
        return;
    }
    
    if (exists) {
        data[index] = ENTRY_EXISTS;
    } else {
        data[index] = ENTRY_EMPTY;
    }
}

// Helper: get tile value
bool getTile(const std::vector<uint8_t>& data, uint32_t block_x, uint32_t block_y) {
    // Bounds check
    if (block_x >= GRID_WIDTH || block_y >= GRID_HEIGHT) {
        return false;
    }
    
    uint32_t index = block_x + block_y * GRID_WIDTH;
    if (index >= TOTAL_ENTRIES) {
        return false;
    }
    
    return data[index] == ENTRY_EXISTS;
}

}  // namespace worldwright
