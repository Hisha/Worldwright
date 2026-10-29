#ifndef WORLDWRIGHT_WDT_PARSER_H
#define WORLDWRIGHT_WDT_PARSER_H

#include <cstdint>
#include <cstring>

namespace worldwright {

// WDT magic number: "WDT1" = 0x57445431
constexpr uint32_t WDT_MAGIC = 0x57445431;

// Header flags
constexpr uint32_t FLAG_ADT_HAS_MCCV = 0x02;
constexpr uint32_t FLAG_ADT_HAS_BIG_ALPHA = 0x04;

// MAIN chunk entry values
constexpr uint8_t ENTRY_EMPTY = 0x00;
constexpr uint8_t ENTRY_EXISTS = 0x01;

// Grid dimensions
constexpr size_t GRID_WIDTH = 64;
constexpr size_t GRID_HEIGHT = 64;
constexpr size_t TOTAL_ENTRIES = GRID_WIDTH * GRID_HEIGHT;  // 4096

// WDT header structure (binary)
#pragma pack(push, 1)
struct WdtHeader {
    uint32_t MVER;          // Magic/version
    uint32_t MPHD;         // Header flags
    // Additional header fields may follow...
} __attribute__((packed));
#pragma pack(pop)

// WDT MAIN chunk entry (4 bytes, little-endian)
struct WdtMainEntry {
    uint8_t value;
    __attribute__((aligned(4)))
    __attribute__((packed))
    uint32_t asUint32() const {
        return static_cast<uint32_t>(value);
    }
};

class WdtParser {
public:
    static WdtParser* create();
    static WdtParser* create(const void* data, size_t size);
    
    // Disable copy
    WdtParser(const WdtParser&) = delete;
    WdtParser& operator=(const WdtParser&) = delete;
    
    // Validation
    bool isValid() const;
    bool hasValidMver() const;
    
    // Header access
    uint32_t getMver() const;
    uint32_t getMphd() const;
    uint32_t getMphdFlag(uint32_t flag) const;
    
    // MAIN chunk access
    bool hasValidMain() const;
    uint32_t getMainEntry(uint32_t index) const;
    uint8_t getMainEntryValue(uint32_t index) const;
    
    // Grid bounds
    bool isInGridBounds(uint32_t x, uint32_t y) const;
    
    // Get MAIN data (const access)
    const uint8_t* getMainData() const;

private:
    WdtParser();
    
    // Internal parsing
    bool parse(const void* data, size_t size);
    
    // Data storage
    const void* m_data;
    size_t m_size;
    const WdtHeader* m_header;
    const uint8_t* m_main_data;
    bool m_valid;
    bool m_has_valid_mver;
    bool m_has_valid_main;
};

// Parse MAIN chunk data (4096 entries, 4 bytes each)
// Returns pointer to MAIN data or nullptr on error
const uint8_t* parseMainChunk(const void* data, size_t size);

}  // namespace worldwright

#endif  // WORLDWRIGHT_WDT_PARSER_H
