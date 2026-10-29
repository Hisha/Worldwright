# WDT Reader Implementation Plan

## Task: Implement C++17 WDT Tile Reader Library and CLI for Worldwright

### Scope
- C++17, Linux build, Windows-compatible code
- No GUI, no MPQ writer, no external runtime dependencies, no GPL code
- Public API: `worldwright::load()`, `worldwright::TileMap` with `hasTile()`, `tileCount()`, `existingTiles()`
- CLI: `worldwright-wdt-info <path>` with `--list` option
- CMake/CTest tests with synthetic fixtures
- Documentation: `docs/WDT_READER.md`

### WDT Format Structure (based on research)
- **MVER**: Magic/version check (0x57445431 = "WDT1")
- **MPHD**: Header flags (adt_has_mccv=0x2, adt_has_big_alpha=0x4, etc.)
- **MAIN**: 64×64 grid, 4096 entries, 4 bytes each (little-endian, no structure packing)
  - 0x00 = empty tile
  - 0x01 = tile exists

### Files to Create/Modify

**Core Implementation:**
1. `src/worldwright/WdtParser.h` - Binary parser class
2. `src/worldwright/WdtParser.cpp` - Parser implementation
3. `src/worldwright/WdtTileMap.h` - Tile map data structure
4. `src/worldwright/WdtTileMap.cpp` - Tile map implementation
5. `src/worldwright/Worldwright.h` - Public API header
6. `src/worldwright/Worldwright.cpp` - Public API implementation
7. `src/worldwright/main.cpp` - CLI entry point

**Build System:**
8. `CMakeLists.txt` - CMake configuration

**Tests:**
9. `tests/WDT_tests.cpp` - Test suite with 10 test cases

**Documentation:**
10. `docs/WDT_READER.md` - Implementation documentation

### Test Cases (10 required)
1. Valid WDT file with known tiles
2. Invalid MVER (magic number)
3. Empty WDT file
4. Corrupted MAIN chunk
5. Out-of-bounds coordinate access (BLOCK_X < 0)
6. Out-of-bounds coordinate access (BLOCK_X >= 64)
7. Out-of-bounds coordinate access (BLOCK_Y < 0)
8. Out-of-bounds coordinate access (BLOCK_Y >= 64)
9. Tile count verification
10. Existing tiles enumeration

### Implementation Steps
1. Create directory structure
2. Implement WdtParser class (binary parsing)
3. Implement WdtTileMap class (data structure + methods)
4. Implement public API
5. Implement CLI
6. Create CMakeLists.txt
7. Write test fixtures and tests
8. Write documentation
9. Build and test

### Verification
- Build configuration: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- Run tests: `ctest --output-on-failure`
- Verify all 10 tests pass
