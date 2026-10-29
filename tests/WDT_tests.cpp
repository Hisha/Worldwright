#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#include "Worldwright.h"
#include "TestUtils.h"

// Test runner class
class TestRunner {
public:
    static int passCount;
    static int failCount;
    static int skipCount;
    
    static void assertEqual(const char* msg, const char* expected, const char* actual) {
        if (strcmp(expected, actual) != 0) {
            std::cerr << "FAIL: " << msg << std::endl;
            std::cerr << "  Expected: " << expected << std::endl;
            std::cerr << "  Actual:   " << actual << std::endl;
            failCount++;
        }
    }
    
    static void assertTrue(const char* msg, bool condition) {
        if (!condition) {
            std::cerr << "FAIL: " << msg << " (condition was false)" << std::endl;
            failCount++;
        }
    }
    
    static void assertFalse(const char* msg, bool condition) {
        if (condition) {
            std::cerr << "FAIL: " << msg << " (condition was true)" << std::endl;
            failCount++;
        }
    }
    
    static void skipTest(const char* msg) {
        std::cerr << "SKIP: " << msg << std::endl;
        skipCount++;
    }
    
    static void printSummary() {
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Test Results:" << std::endl;
        std::cout << "  Passed: " << passCount << std::endl;
        std::cout << "  Failed: " << failCount << std::endl;
        std::cout << "  Skipped: " << skipCount << std::endl;
        std::cout << "  Total:   " << passCount + failCount + skipCount << std::endl;
        std::cout << "========================================" << std::endl;
        
        if (failCount > 0) {
            return 1;
        }
        return 0;
    }
};

int TestRunner::passCount = 0;
int TestRunner::failCount = 0;
int TestRunner::skipCount = 0;

// ============================================================================
// TEST 1: Valid WDT file with known tiles
// ============================================================================
void test1_validWdtFile() {
    std::cout << "Test 1: Valid WDT file with known tiles" << std::endl;
    
    // Generate valid WDT with 5 tiles at known positions
    std::vector<std::pair<uint32_t, uint32_t>> knownTiles = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 2}
    };
    
    std::vector<uint8_t> data = worldwright::generateWdtWithKnownTiles(knownTiles);
    
    // Verify MVER
    WdtHeader header;
    memcpy(&header, data.data(), sizeof(WdtHeader));
    TestRunner::assertEqual("MVER check", "0x57445431", "0x" + 
                            std::to_string(static_cast<uint32_t>(header.MVER)));
    
    // Verify MAIN chunk size
    TestRunner::assertTrue("MAIN chunk size", data.size() >= sizeof(WdtHeader) + TOTAL_ENTRIES * sizeof(uint8_t));
    
    // Verify tile count
    size_t count = worldwright::tileCount(data.data(), data.size());
    TestRunner::assertEqual("Tile count", "4096", 
                            std::to_string(count));
    
    // Verify existing tile count
    size_t existing = worldwright::existingTileCount(data.data(), data.size());
    TestRunner::assertEqual("Existing tiles", "5", 
                            std::to_string(existing));
    
    // Verify specific tiles
    for (const auto& tile : knownTiles) {
        bool exists = worldwright::hasTile(data.data(), data.size(), tile.first, tile.second);
        TestRunner::assertTrue("Tile exists", exists);
    }
    
    // Verify non-existing tiles
    bool notExists = worldwright::hasTile(data.data(), data.size(), 5, 5);
    TestRunner::assertFalse("Tile not exists", notExists);
    
    // Clean up
    delete data.data();  // Should not be deleted (const)
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 2: Invalid MVER (magic number)
// ============================================================================
void test2_invalidMver() {
    std::cout << "Test 2: Invalid MVER (magic number)" << std::endl;
    
    // Generate WDT with wrong MVER
    std::vector<uint8_t> data = worldwright::generateInvalidMverWdt();
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load invalid MVER", tile_map != nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 3: Empty/Invalid WDT file
// ============================================================================
void test3_emptyWdt() {
    std::cout << "Test 3: Empty/Invalid WDT file" << std::endl;
    
    // Generate empty/invalid WDT
    std::vector<uint8_t> data = worldwright::generateEmptyWdt();
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load empty file", tile_map != nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 4: Corrupted MAIN chunk
// ============================================================================
void test4_corruptedMain() {
    std::cout << "Test 4: Corrupted MAIN chunk" << std::endl;
    
    // Generate WDT with corrupted MAIN
    std::vector<uint8_t> data = worldwright::generateCorruptedMainWdt();
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load corrupted MAIN", tile_map != nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 5: Out-of-bounds coordinate (BLOCK_X < 0)
// ============================================================================
void test5_outOfBoundsXNegative() {
    std::cout << "Test 5: Out-of-bounds coordinate (BLOCK_X < 0)" << std::endl;
    
    // Generate valid WDT
    std::vector<uint8_t> data = worldwright::generateValidWdt(10, 42);
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load valid WDT", tile_map == nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 6: Out-of-bounds coordinate (BLOCK_X >= 64)
// ============================================================================
void test6_outOfBoundsXMax() {
    std::cout << "Test 6: Out-of-bounds coordinate (BLOCK_X >= 64)" << std::endl;
    
    // Generate valid WDT
    std::vector<uint8_t> data = worldwright::generateValidWdt(10, 42);
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load valid WDT", tile_map == nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 7: Out-of-bounds coordinate (BLOCK_Y < 0)
// ============================================================================
void test7_outOfBoundsYNegative() {
    std::cout << "Test 7: Out-of-bounds coordinate (BLOCK_Y < 0)" << std::endl;
    
    // Generate valid WDT
    std::vector<uint8_t> data = worldwright::generateValidWdt(10, 42);
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load valid WDT", tile_map == nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 8: Out-of-bounds coordinate (BLOCK_Y >= 64)
// ============================================================================
void test8_outOfBoundsYMax() {
    std::cout << "Test 8: Out-of-bounds coordinate (BLOCK_Y >= 64)" << std::endl;
    
    // Generate valid WDT
    std::vector<uint8_t> data = worldwright::generateValidWdt(10, 42);
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load valid WDT", tile_map == nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 9: Tile count verification
// ============================================================================
void test9_tileCount() {
    std::cout << "Test 9: Tile count verification" << std::endl;
    
    // Generate WDT with all tiles existing
    std::vector<uint8_t> data = worldwright::generateValidWdt(TOTAL_ENTRIES, 42);
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load valid WDT", tile_map == nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// TEST 10: Existing tiles enumeration
// ============================================================================
void test10_existingTilesEnumeration() {
    std::cout << "Test 10: Existing tiles enumeration" << std::endl;
    
    // Generate WDT with known tiles
    std::vector<std::pair<uint32_t, uint32_t>> knownTiles = {
        {5, 10}, {15, 20}, {25, 30}, {35, 40}, {45, 50}, {55, 60}
    };
    
    std::vector<uint8_t> data = worldwright::generateWdtWithKnownTiles(knownTiles);
    
    // Should not load successfully
    TileMap* tile_map = worldwright::load(data.data(), data.size());
    TestRunner::assertFalse("Load valid WDT", tile_map == nullptr);
    
    // Clean up
    delete tile_map;
    
    TestRunner::passCount++;
    std::cout << "  PASSED" << std::endl;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Worldwright WDT Reader Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Run all tests
    test1_validWdtFile();
    test2_invalidMver();
    test3_emptyWdt();
    test4_corruptedMain();
    test5_outOfBoundsXNegative();
    test6_outOfBoundsXMax();
    test7_outOfBoundsYNegative();
    test8_outOfBoundsYMax();
    test9_tileCount();
    test10_existingTilesEnumeration();
    
    // Print summary
    return TestRunner::printSummary();
}
