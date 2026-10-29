#ifndef WORLDWRIGHT_TEST_UTILS_H
#define WORLDWRIGHT_TEST_UTILS_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace worldwright {

// Generate a valid WDT file structure
// Parameters:
//   num_existing_tiles - Number of tiles that should exist (0-4096)
//   seed - Random seed for tile placement
// Returns: Raw WDT data buffer (header + MAIN chunk)
std::vector<uint8_t> generateValidWdt(int num_existing_tiles = 10, int seed = 42);

// Generate an invalid WDT file with wrong MVER
std::vector<uint8_t> generateInvalidMverWdt();

// Generate an empty/invalid WDT file
std::vector<uint8_t> generateEmptyWdt();

// Generate a WDT file with corrupted MAIN chunk
std::vector<uint8_t> generateCorruptedMainWdt();

// Generate a WDT file with out-of-bounds coordinate test
// This creates a file that has tiles at valid positions
std::vector<uint8_t> generateWdtForBoundsTests();

// Generate a WDT file with known tile positions
std::vector<uint8_t> generateWdtWithKnownTiles(const std::vector<std::pair<uint32_t, uint32_t>>& tiles);

// Helper: set specific tile to exist
void setTile(const std::vector<uint8_t>& data, uint32_t block_x, uint32_t block_y, bool exists);

// Helper: get tile value at position
bool getTile(const std::vector<uint8_t>& data, uint32_t block_x, uint32_t block_y);

}  // namespace worldwright

#endif  // WORLDWRIGHT_TEST_UTILS_H
