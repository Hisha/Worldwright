#ifndef WORLDWRIGHT_WDT_READER_H
#define WORLDWRIGHT_WDT_READER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace worldwright {

// Failure categories reported by WdtReader::load() / loadFromMemory().
enum class WdtError {
    None = 0,
    FileOpenFailed,
    FileReadFailed,
    FileTooLarge,
    TruncatedData,
    MissingRequiredChunk,
    DuplicateChunk,
    InvalidChunkSize,
    UnsupportedVersion,
    UnsupportedWmoOnly,
};

// Reads a World of Warcraft 3.3.5a (build 12340) terrain WDT file.
//
// A WDT is a chunked file. For a terrain map the relevant chunks are:
//   MVER - one uint32 version, which must be 18 for 3.3.5a.
//   MPHD - a 32-byte map header whose first uint32 is the flag word.
//   MAIN - a 64 x 64 table of 8-byte entries. The low bit of the first
//          uint32 in each entry means "an ADT exists for this tile".
//
// Coordinate convention:
//   x is the grid column and y is the grid row, both in [0, 63].
//   The MAIN table is stored row-major, so entry index = y * 64 + x.
//   These are the same coordinates used in ADT file names
//   ("<map>_<x>_<y>.adt"). No world-space conversion is performed here.
class WdtReader {
public:
    static constexpr int kGridWidth = 64;
    static constexpr int kGridHeight = 64;
    static constexpr std::size_t kGridCellCount =
        static_cast<std::size_t>(kGridWidth) * static_cast<std::size_t>(kGridHeight);
    static constexpr std::uint32_t kSupportedVersion = 18;

    static constexpr std::uint32_t kFlagGlobalWmo = 0x0001;
    static constexpr std::uint32_t kFlagHasMccv = 0x0002;
    static constexpr std::uint32_t kFlagHasBigAlpha = 0x0004;
    static constexpr std::uint32_t kFlagDoodadsSortedBySize = 0x0008;

    WdtReader() = default;
    ~WdtReader() = default;

    WdtReader(const WdtReader&) = default;
    WdtReader& operator=(const WdtReader&) = default;
    WdtReader(WdtReader&&) = default;
    WdtReader& operator=(WdtReader&&) = default;

    // Loads and validates a WDT file. On failure all previously loaded state
    // is cleared and false is returned; a later call can load another file.
    bool load(const std::filesystem::path& path);

    // Parses an in-memory WDT image. The buffer is not retained. Intended for
    // embedding and tests; the parser still applies the same validation.
    bool loadFromMemory(const void* data, std::size_t size);

    bool isLoaded() const noexcept;

    // Returns true only for an in-range tile whose MAIN existence bit is set.
    // Negative or out-of-range coordinates return false.
    bool hasTile(int x, int y) const noexcept;

    // Number of existing terrain tiles (not 64 * 64).
    std::size_t tileCount() const noexcept;

    // All existing tiles in row-major order (y outer, x inner).
    std::vector<std::pair<int, int>> existingTiles() const;

    // Computes the inclusive bounding box of existing tiles. Returns false
    // when no tiles exist (the output parameters are left untouched).
    bool tileBounds(int& minX, int& minY, int& maxX, int& maxY) const noexcept;

    // MVER value. Returns 0 when no file has been loaded successfully.
    std::uint32_t version() const noexcept;

    // MPHD flag word. Returns 0 when no file has been loaded successfully.
    std::uint32_t headerFlags() const noexcept;

    bool hasGlobalWmo() const noexcept;
    bool hasMccv() const noexcept;
    bool hasBigAlpha() const noexcept;

    WdtError error() const noexcept;

    // Human-readable diagnostic from the most recent failed load. The string
    // remains valid until the next load attempt.
    const std::string& errorMessage() const noexcept;

private:
    bool parseBuffer(const void* data, std::size_t size);
    bool fail(WdtError error, std::string message);
    void reset() noexcept;

    std::string m_sourceName;
    std::array<std::uint8_t, kGridCellCount> m_tiles{};
    std::size_t m_tileCount = 0;
    std::uint32_t m_version = 0;
    std::uint32_t m_headerFlags = 0;
    bool m_loaded = false;
    WdtError m_error = WdtError::None;
    std::string m_errorMessage;
};

}  // namespace worldwright

#endif  // WORLDWRIGHT_WDT_READER_H
