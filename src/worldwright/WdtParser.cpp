#include "WdtParser.h"
#include "WdtTileMap.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace worldwright {

bool WdtParser::parse(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return false;
    }
    
    m_data = data;
    m_size = size;
    
    // Ensure minimum size for header
    if (size < sizeof(WdtHeader)) {
        m_valid = false;
        return false;
    }
    
    // Get header pointer
    m_header = static_cast<const WdtHeader*>(data);
    
    // Validate MVER (magic number)
    m_has_valid_mver = (m_header->MVER == WDT_MAGIC);
    if (!m_has_valid_mver) {
        m_valid = false;
        return false;
    }
    
    // MAIN chunk starts after header
    size_t header_size = sizeof(WdtHeader);
    m_main_data = static_cast<const uint8_t*>(data) + header_size;
    
    // Ensure MAIN chunk is present (4096 entries * 4 bytes)
    if (size < header_size + TOTAL_ENTRIES * sizeof(uint8_t)) {
        m_valid = false;
        m_has_valid_main = false;
        return false;
    }
    
    m_has_valid_main = true;
    m_valid = true;
    
    return true;
}

bool WdtParser::isValid() const {
    return m_valid;
}

bool WdtParser::hasValidMver() const {
    return m_has_valid_mver;
}

uint32_t WdtParser::getMver() const {
    if (!m_has_valid_mver) {
        return 0;
    }
    return m_header->MVER;
}

uint32_t WdtParser::getMphd() const {
    if (!m_has_valid_mver) {
        return 0;
    }
    return m_header->MPHD;
}

uint32_t WdtParser::getMphdFlag(uint32_t flag) const {
    if (!m_has_valid_mver) {
        return 0;
    }
    return (m_header->MPHD & flag) ? flag : 0;
}

bool WdtParser::hasValidMain() const {
    return m_has_valid_main;
}

uint32_t WdtParser::getMainEntry(uint32_t index) const {
    if (!m_has_valid_main || index >= TOTAL_ENTRIES) {
        return 0;
    }
    return m_main_data[index];
}

uint8_t WdtParser::getMainEntryValue(uint32_t index) const {
    if (!m_has_valid_main || index >= TOTAL_ENTRIES) {
        return 0;
    }
    return m_main_data[index];
}

bool WdtParser::isInGridBounds(uint32_t x, uint32_t y) const {
    return x < GRID_WIDTH && y < GRID_HEIGHT;
}

WdtParser* WdtParser::create() {
    return new WdtParser();
}

WdtParser* WdtParser::create(const void* data, size_t size) {
    WdtParser* parser = new WdtParser();
    parser->parse(data, size);
    return parser;
}

const uint8_t* parseMainChunk(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return nullptr;
    }
    
    const uint8_t* main_ptr = static_cast<const uint8_t*>(data) + sizeof(WdtHeader);
    
    // Verify minimum size for MAIN chunk
    if (size < sizeof(WdtHeader) + TOTAL_ENTRIES * sizeof(uint8_t)) {
        return nullptr;
    }
    
    return main_ptr;
}

}  // namespace worldwright
